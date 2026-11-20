// Device program (name: PROJECT_NAME in project.json): replacement for the CAN box in a Peugeot 508 with an Android radio (FYT).
//
// What it does:
//   - listens to the car's INFO-DIV bus (125 kbit/s) and beeps from its own speaker according to the parking sensors;
//   - translates buttons, sensors and ignition state into the radio protocol (UART, "XP");
//   - in MODE_DECODER sends this to the radio and supplies it with ACC and the reverse gear signal;
//   - in the *_wifi environments additionally: web service panel and update over Wi-Fi (src/net/net.h).
//
// CAR SAFETY: in the five environments without transmission the program transmits nothing on the CAN
// bus: the controller works in listen-only mode, and the module's SLNT pin is high (transmitter
// disabled). Since 17.11.2026 jumper JP1 is closed and SLNT is wired to GPIO6 (config.h):
// there is no hardware transmitter lock any more, transmission is decided only by the program.
// The sixth environment (decoder_tx_wifi, CAN_TX_MODE in config.h) can transmit only the frames of the car
// profile's transmit allow list (PSA AEE2010: 15B; 1A9 only with -DCAN_TX_EXPERIMENTAL=1), after arming with the "cantx on" command and a restart; without arming it behaves like the others.
// The operating mode is selected in config.h (WORK_MODE); by default the device sends nothing to the radio either.
//
// USB console (115200): type "help". A CAN frame can be pasted by hand in the sniffer log
// format, e.g. 0E1#B0F000FCFC1EC200, to check the behaviour without the car.
//
// USB output goes only through console.h (conPrintf / conPrintln): in the program without network this gives
// the same bytes as Serial.printf used to, and in the program with network every line also goes to the panel.
// The net*() functions in the program without network are empty inline functions: the compiler removes them entirely.
#include <Arduino.h>
#include "driver/gpio.h"

#include "app_state.h"
#include "app_status.h"
#include "audio_out.h"
#include "can_bus.h"
#include "can_tx_arm.h"
#include "config.h"
#include "console.h"
#include "console_cmds.h"
#include "decoder.h"
#include "net/net.h"
#include "outputs.h"
#include "radio_uart.h"
#include "settings.h"
#include "sleep_ctrl.h"
#include "version.h"
#if CAN_TX_MODE == CAN_TX_SETTINGS
#include "can_tx/can_tx_port.h"     // transmit hardware: only in environment decoder_tx_wifi
#endif

static const bool DECODER_MODE = (WORK_MODE == MODE_DECODER);

// ---------- logic (lib/core, hardware independent) ----------
// All logic (buttons, sensors, radio power, frames for the radio) is in the Decoder class
// (lib/app/decoder.h). What stays here is hardware: CAN, 12 V outputs, UART, sound, LED, network, sleep.
// Console commands: console_cmds.cpp. State snapshot for the web panel: app_status.cpp.
Decoder decoder;

// ---------- program state ----------
// true = the CAN transmitter window is open (gate in state OPENING / SENDING). Read by the web task
// before starting to write the program (ota.cpp); always false in the program without transmission.
volatile bool g_canTxWindow = false;
// The transmitter window lasts at most 20 ms. During that time the loop does nothing that could write the
// flash memory (commands, deferred saves, panel requests, network handling): a flash write stops both cores
// and the window timer, so the transmitter would stay open for the duration of the write (review 18.11.2026).
static bool txWindowOpen() {
    const TxState s = decoder.canTxState();
    const bool open = s == TxState::OPENING || s == TxState::SENDING;
    g_canTxWindow = open;
    return open;
}
AppState app;                          // driver startup results, "dump", "test" (app_state.h)
// Overnight parking sensors record (lib/car_psa_aee2010/park_diag.h): in RTC slow memory, which keeps its content
// in deep sleep and loses it on power loss. Zero at power-on (no valid magic): ParkDiag::begin starts it fresh.
// Whether it also survives a software restart was not checked; the magic covers both cases.
RTC_DATA_ATTR static psa::ParkDiagRecord s_parkDiag;
// Amplifier kept on for 2 s after the last sound. A deadline with an "active" flag
// (lib/core/loop_policy.h): a plain time comparison after approx. 24.9 days of running without sleep would say "running" again.
static Deadline ampHold;

// Console lines from the decoder ("[park] ...", "[car] ...", "[key] ..."): ready text with the
// end-of-line character, to USB and to the panel the same way as before.
static void decoderLog(const char* line) { conPrintf("%s", line); }

// ---------- CAN frame handling ----------
// fromBus = frame from the bus or pasted on USB (true, as before); false = pasted from the web panel.
// A frame from the panel goes into the logic like any other, but does not count as bus traffic:
// it does not extend the device's awake time or Wi-Fi operation (net.h).
void handleFrame(const CanFrame& f, uint32_t now, bool fromBus) {
    netOnCanFrame(f, now);                 // preview in the panel; does not wait (empty in the program without network)
    if (app.dumpCan) {                         // line format as in sniffer logs: can0 ID#DATA
        // USB only: approx. 218 lines per second would push all other lines out of the panel.
        conUsbPrintf("(%lu.%03lu) can0 %03lX#", (unsigned long)(now / 1000), (unsigned long)(now % 1000),
                     (unsigned long)f.id);
        for (uint8_t i = 0; i < f.len; i++) conUsbPrintf("%02X", f.data[i]);
        conUsbPrintln();
    }
    decoder.onCanFrame(f, now, fromBus);   // all the logic: buttons, data for the radio, sensors, ignition
}

// ---------- startup ----------
// Startup reason for the USB greeting (ESP-IDF esp_reset_reason(); the panel shows the same codes).
static const char* startReasonText() {
    if (sleepWokeFromBus()) return "wake-up from the bus";
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "power on";
        case ESP_RST_SW: return "software restart (command, update)";
        case ESP_RST_PANIC: return "program CRASH (panic)";
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT: return "CRASH: the watchdog fired";
        case ESP_RST_BROWNOUT: return "supply voltage drop (brownout)";
        case ESP_RST_USB: return "reset from USB (flashing / port opened)";
        default: return "reset";
    }
}

void setup() {
    // First instruction of the program: SLNT of the CAN module (GPIO6) high = transceiver transmitter disabled.
    // After a restart without power loss (panic, watchdog) the output register keeps its old state, so we do not
    // wait with this until canBegin(). gpio_set_level before pinMode: digitalWrite before pinMode writes nothing.
    gpio_set_level(PIN_CAN_SILENT, 1);
    pinMode(PIN_CAN_SILENT, OUTPUT);
    outputsBegin();                // first the 12 V outputs to the off state
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);      // a missing computer must not block the program
    consoleBegin();                // from now on only this task (main loop) writes to USB
    statusLed(0, 0, 8);
    // Settings from NVS (volume, button table, Wi-Fi). false = NVS unavailable: we run
    // on default values, like a program without settings.
    bool settingsOk = settingsBegin();
    DecoderConfig dcfg;
    dcfg.decoderMode = DECODER_MODE;
    dcfg.car = &CAR_PROFILE_SELECTED;          // -DCAR_PROFILE (config.h), default PSA AEE2010
    dcfg.radio = &RADIO_PROTOCOL_SELECTED;     // -DRADIO_PROTOCOL (config.h), default XP
    decoder.begin(dcfg, decoderLog);
    decoder.attachParkDiag(&s_parkDiag, sleepWokeFromBus());   // counts the wake-up, compares the state once

    // --- arming of CAN transmission ---
    // NVS holds the build string of the program in which "cantx on" was entered. It is valid only for THIS
    // SAME program (same string); any other program, including one without transmission, erases it at startup.
    // Thanks to that, after flashing another program (and after returning to the old one) transmission is disarmed.
    char txTag[SET_CANTX_TAG_MAX];
    const bool haveTxTag = settingsGetCanTxTag(txTag, sizeof(txTag));
    bool txArmed = false;
#if CAN_TX_MODE == CAN_TX_SETTINGS
    txArmed = haveTxTag && cantx::armTagValid(txTag, fwIdent().build);
#endif
    const bool txTagDropped = haveTxTag && !txArmed;
    if (txTagDropped) settingsClearCanTxTag();     // flash write in setup(), before CAN reception starts

    // Armed: controller NORMAL + SLNT high. Otherwise (and always in the program without transmission): listen-only.
    app.canOk = canBegin(txArmed);
#if CAN_TX_MODE == CAN_TX_SETTINGS
    // Transmit hardware for the gate. Without the window timer or without the CAN driver the gate stays disarmed.
    const bool txPortOk = canTxPortBegin();
    app.txArmed = txArmed && app.canOk && txPortOk;
    app.txArmNext = txArmed;
    decoder.setSettingsMask(vset::sanitizeMask(settingsVsetMask(vset::ENABLED_DEFAULT)));
    decoder.attachCanTx(canTxPort(), app.txArmed);
#endif
    app.audioOk = audioBegin();
    app.radioOk = radioUartBegin(DECODER_MODE);

    // Stored volume, but not lower than AUDIO_VOLUME_MIN_SAVED (sensor beeping must not
    // be muted permanently). Without a stored value AUDIO_VOLUME_DEFAULT applies, as before.
    uint8_t vol = settingsVolume(AUDIO_VOLUME_DEFAULT);
    if (vol < AUDIO_VOLUME_MIN_SAVED) vol = AUDIO_VOLUME_MIN_SAVED;
    audioSetVolume(vol);
    // Button remap table. No stored value = default entries of the car profile (set in decoder.begin).
    char mapText[xp::KeyCustomizer::MAP_TEXT_MAX];
    bool mapSaved = settingsGetKeyMap(mapText, sizeof(mapText));
    bool mapOk = !mapSaved || decoder.keyMap().parseMap(mapText, strlen(mapText));   // on error the table is unchanged

    delay(300);
#if CAN_TX_MODE == CAN_TX_SETTINGS
    // Frames of the allow list (lib/car_psa_aee2010/tx_allowlist.cpp); the default build prints the same line as earlier versions.
#if CAN_TX_EXPERIMENTAL
#define TX_FRAMES_TEXT "15B 1A9 (EXPERIMENTAL)"
#else
#define TX_FRAMES_TEXT "15B"
#endif
    conPrintf("# " PROJECT_NAME ". CAN %lu kbit/s, TRANSMIT " TX_FRAMES_TEXT ": built, armed=%u. Mode: %s.\n",
              (unsigned long)(CAR_PROFILE_SELECTED.canBitrate / 1000), app.txArmed,
              DECODER_MODE ? "DECODER (sends to the radio, drives ACC)" : "alongside the box (sends nothing)");
    conPrintf("# CAN controller: %s\n", canListenOnly() ? "LISTEN ONLY (transmit disarmed)"
                                                       : "NORMAL, transmitter closed outside the window of one " TX_FRAMES_TEXT " frame");
    if (!txPortOk) conPrintln("# ERROR: the transmitter window timer was not created, transmit stays disarmed");
#else
    conPrintf("# " PROJECT_NAME ". CAN %lu kbit/s LISTEN ONLY. Mode: %s.\n",
              (unsigned long)(CAR_PROFILE_SELECTED.canBitrate / 1000),
              DECODER_MODE ? "DECODER (sends to the radio, drives ACC)" : "alongside the box (sends nothing)");
#endif
    conPrintf("# profile: car=%s radio=%s\n", CAR_PROFILE_SELECTED.id, RADIO_PROTOCOL_SELECTED.id);
    if (txTagDropped) conPrintln("# CAN transmit arming stored by another program: cleared");
    const FwIdent& id = fwIdent();
    conPrintf("# version: %s %s, built %s, network=%s, volume=%u%%\n", id.product, id.version, id.build,
              id.net ? "yes" : "no", audioVolume());
    conPrintf("# can=%s audio=%s radio=%s\n", app.canOk ? "ok" : "FAIL", app.audioOk ? "ok" : "FAIL",
              app.radioOk ? "ok" : "FAIL");
    conPrintf("# start: %s\n", startReasonText());
    if (!settingsOk) conPrintln("# settings unavailable (NVS): defaults, changes will not be stored");
    if (!mapOk) conPrintln("# stored key swap table is invalid: the fixed entries apply");
    // Driver error: we stay in the safe state (12 V outputs off, amplifier asleep: outputsAllowed / ampAllowed
    // in loop()) and signal it with the red LED (statusLedState keeps it red in loop()).
    // No restart loop: the radio and the car must not receive ACC pulses.
    if (!app.canOk || !app.audioOk || !app.radioOk) statusLed(16, 0, 0);

    // Network (only *_wifi environments; without network both functions are empty). At the end of setup(): netBegin
    // enables the main loop time watchdog (5 s), and earlier delay() calls would bring it needlessly closer.
    netBegin();
    netBootCheck(app.canOk && app.audioOk && app.radioOk);    // may not return: rollback of a faulty program
}

// ---------- main loop ----------
void loop() {
    uint32_t now = millis();

    CanFrame f;
    while (canReceive(f)) handleFrame(f, now, true);
    if (!txWindowOpen()) cmdPollUsb(now);
    // Settings saves from USB deferred while reversing / sensors active (a flash write stops CAN reception).
    if (!txWindowOpen()) cmdPollDeferredSaves(now);
    consolePoll();                // ESP-IDF error lines (only the program with network)
    if (!txWindowOpen()) cmdPollNetRequests(now);
#if CAN_TX_MODE == CAN_TX_SETTINGS
    // Controller events in NORMAL mode (send result, bus-off and reception recovery); does nothing in listen-only.
    canAlertsPoll(now);
    // Additional transmit gate locks: the whole firmware update session and a pending settings save.
    decoder.setTxBlock(netOtaBusy(), cmdSavesPending());
#endif
    // Writing flash memory (update over Wi-Fi) loses 0F6 frames: during that time a longer
    // freshness limit, so that a lost frame does not turn off the radio's ACC. Without network always false (as before).
    // tick(): buttons -> radio power (ACC, reverse) -> frames for the radio -> sensor sound.
    const DecoderOutputs& d = decoder.tick(now, netFlashBusy(now));
    const PowerOutputs& po = d.power;
    // Driver start results (setup()): any failure keeps the safe state (lib/core/loop_policy.h).
    DriverInit drv;
    drv.can = app.canOk;
    drv.audio = app.audioOk;
    drv.radio = app.radioOk;
    // The car profile must allow the 12 V outputs (CarProfile::powerOutputs; generic: never).
    if (outputsAllowed(DECODER_MODE, drv, CAR_PROFILE_SELECTED.powerOutputs)) outputsSetRadio(po.acc, po.illumination, po.reverse);
    else outputsAllOff();

    radioUartPoll(now, decoder);
    txWindowOpen();               // a radio command may have just opened the window: refresh the flag

    // --- sensor sound ---
    const BeeperOutput& out = d.beep;
    bool tone = out.toneOn;
    uint16_t freq = out.freqHz;
    app.test.poll(now);                           // after the deadline clears the "active" flag
    if (app.test.running(now)) {                  // "test" command: beep every 200 ms
        tone = (app.test.leftMs(now) / 100) % 2 == 0;
        freq = 1000;
    }
    // The amplifier is woken when the sensors are active and kept on for 2 s after the last sound.
    // This way the first beep is not cut off by the amplifier startup time.
    if (tone || out.active || d.sensorsOn) ampHold.start(now, 2000);
    ampHold.poll(now);
    audioSetAmp(ampAllowed(drv) && ampHold.running(now));   // after a driver error: amplifier asleep
    audioSetTone(tone, freq);

    // --- CAN controller supervision: state read once per second, a line only on change ---
    // Switches nothing: the 12 V outputs still follow the ignition from the last fresh 0F6 frames.
    if (canHealthPoll(now)) {
        const CanHealthInfo& h = canHealth();
        conPrintf("[can] controller: state=%s tec=%u rec=%u\n", canHealthStateName(), h.tec, h.rec);
    }

    // --- LED: off = silence on the bus, green = frames, yellow = sound,
    //     red = a driver failed in setup() or the CAN controller is not running (state from canHealthPoll) ---
    // With the ignition off it does not light, so as not to draw current from the battery.
    static uint8_t ledState = 255;
    bool busAlive = d.busAlive;
    uint8_t want = statusLedState(drv, canHealth().running, out.active, busAlive);
    if (want != ledState) {
        ledState = want;
        if (want == 3) statusLed(16, 0, 0);
        else if (want == 2) statusLed(12, 8, 0);
        else if (want == 1) statusLed(0, 8, 0);
        else statusLed(0, 0, 0);
    }

    // --- network: everything below is empty in the program without network and disappears at compile time ---
    if (netStatusDue(now)) appPublishStatus(now, busAlive, out);
    NetLoopInputs ni;
    ni.busAlive = busAlive;
    // ACC counts for the network only with real bus traffic in the last 5 s
    // (frames pasted from the panel do not extend the awake time or Wi-Fi).
    ni.accOn = d.accForNet;
    ni.carBusy = d.carBusy;
    ni.engineRunning = d.engineRunning;
    // With the transmitter window open we skip network handling on this pass (it may write flash);
    // "hold" = true, so that the device certainly does not fall asleep on this pass.
    bool hold = txWindowOpen() ? true : netLoop(ni, now);
    if (netRestartDue(now)) {
        // Scheduled restart (after an update, a program rollback, from the panel). First the safe
        // state: the radio loses ACC, the amplifier sleeps. We do not stop the network (the restart turns it off anyway).
        decoder.canTxForceClose();     // CAN transmitter closed (does nothing in the program without transmission)
        outputsAllOff();
        audioSetAmp(false);
        statusLed(0, 0, 0);
        ESP.restart();
    }

    sleepPoll(busAlive, po.acc, hold, now);
    // Last call before yielding the processor. It may write the otadata sector (confirmation or rollback of an
    // update), which stops both cores: not while the CAN transmitter window is open, like netLoop above.
    if (!txWindowOpen()) netConfirmHealthy(now);

    vTaskDelay(1);    // yields the processor to other tasks (sound), loop approx. every 1 ms
}
