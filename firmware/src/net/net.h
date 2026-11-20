// The device's network as seen from main.cpp: web service panel and firmware update over Wi-Fi.
//
// This is the only file from src/net that main.cpp (and sleep_ctrl.cpp) includes. When the program is built
// without networking (NET_ENABLED = 0, environments "listen" and "decoder"), every function below
// is an empty inline function: main.cpp needs no #if, and the compiler removes the calls
// and all the code around them. Nothing from Wi-Fi ends up in the program then.
//
// CAR SAFETY: the network code transmits nothing on the CAN bus and has no "transmit frame" function.
// From the panel a frame can be pasted into the INTERNAL logic, just like from the USB console. In the
// transmit-capable build (environment decoder_tx_wifi) the panel can additionally arm or disarm
// transmission (frames of the car profile allow list) and change the item mask, but only a client connected to the device's OWN
// network (api_handlers.cpp: httpPeerIsAp), never via the home network; the frame itself is decided by the gate
// (lib/core/can_tx_gate.h), not by the network.
// Note: the Wi-Fi update allows anyone who knows the device password to upload a DIFFERENT program
// (images are not signed). There is no hardware transmitter lock: since 17.11.2026 on the prototype jumper
// JP1 is closed and SLNT is connected to GPIO6 (config.h), so transmission is decided solely by the
// uploaded program. Protection against mistakes: the canTx marker in the image and a separate consent at
// update time (lib/core/fw_ident.h); arming the transmitter does not carry over to another program.
// Open risk, for the owner to decide: image signing or bringing the jumper back.
//
// Division of work:
//   main loop    : all the car logic as before; it only HANDS data to the network (frames, state snapshot)
//                  and RECEIVES requests. All functions from this file are called only by the main loop
//                  and NONE of them waits for the network (exception: netBootCheck in setup()).
//   web task     : HTTP server (ESP-IDF esp_http_server) in a separate task on core 0. It reads only
//                  copies of data from src/net/diag_hub and puts requests into the queue. It does not touch CAN,
//                  the UART, sound, outputs or logic objects. It does not write to USB.
//   net_wifi task : all blocking WiFi, mDNS and server start/stop calls (wifi_mgr.h).
//
// Time: timestamps written by other tasks (client activity, Wi-Fi events) may be
// NEWER than 'nowMs' taken at the start of the loop pass. Compute every time difference as
// (int32_t)(nowMs - stamp), and treat a negative result as 0. Store deadlines (restart) as
// absolute time and compare (int32_t)(nowMs - deadline) >= 0.
//
// How to hook into main.cpp (the order matters):
//
//   setup():
//     outputsBegin(); Serial.begin(115200); Serial.setTxTimeoutMs(0); consoleBegin();
//     settingsBegin();
//     canOk = canBegin(); audioOk = audioBegin(); radioOk = radioUartBegin(DECODER_MODE);
//     ... volume (at least AUDIO_VOLUME_MIN_SAVED) and key table from the settings ...
//     netBegin();
//     netBootCheck(canOk && audioOk && radioOk);     // may not return: rollback of a faulty program
//
//   handleFrame(f, now, fromBus):  netOnCanFrame(f, now);   // at the very beginning; fromBus see below
//
//   handleLine(line, now, fromUsb): if (netConsoleCommand(line, fromUsb, now)) return;  // BEFORE
//                                   parseCandumpLine: "wifi sta AB#..." must not be taken for a frame
//
//   loop():
//     while (canReceive(f)) handleFrame(f, now, true);
//     pollConsole(now);                        // frames pasted over USB: handleFrame(f, now, true), as before
//     consolePoll();                           // ESP-IDF error lines (console.h)
//     NetRequest rq;
//     while (netTakeRequest(rq, now)) { ... CONSOLE_LINE, SET_KEYMAP, RESET_KEYMAP ... }
//     power.setLossTolerant(netFlashBusy(now)); // BEFORE power.update(): a flash write loses 0F6 frames
//     ... existing logic: keys, radio power, sound, LED ...
//     if (netStatusDue(now)) { ... fill StatusSnapshot ...; netPublishStatus(snap); }
//     bool hold = netLoop({busAlive, accFromBus, carBusy, engineRunning}, now);
//     if (netRestartDue(now)) { outputsAllOff(); audioSetAmp(false); statusLed(0, 0, 0); ESP.restart(); }
//     sleepPoll(busAlive, powerOut.acc, hold, now);  // asks netPrepareSleep() before sleeping
//     netConfirmHealthy(now);                        // last call before vTaskDelay(1)
//     vTaskDelay(1);
//
//   Frames pasted from the PANEL (CONSOLE_LINE): handleFrame(f, now, false). Such a frame reaches the logic
//   and the preview like any other, but does NOT set lastAnyFrameMs / haveAnyFrame: it does not count as
//   bus traffic, so it does not extend the wake period or keep Wi-Fi up (a client cannot use it to
//   bypass the 10 min limit). Frames from USB work as before (the build without networking is unchanged).
//   accFromBus = powerOut.acc && haveAnyFrame && (now - lastAnyFrameMs) < 5000.
//   engineRunning = fresh frame 0F6 (body) with byte 1 equal to 0x8E.
//
// Implementation: src/net/net.cpp (ties together diag_hub, wifi_mgr, http_server, ota and lib/core/net_policy).
// Every src/net/*.cpp file starts with #include "config.h" and has all the rest inside #if NET_ENABLED,
// so that it contributes no code in environments without networking.
#pragma once
#include <stdint.h>

#include "can_frame.h"
#include "config.h"          // NET_ENABLED
#include "diag_types.h"      // StatusSnapshot, NetRequest: exist also without networking

// What the main loop knows and the network does not. Supplied on every loop pass.
struct NetLoopInputs {
    bool busAlive = false;   // CAN frame FROM THE BUS within the last second (frames pasted from the panel do not count)
    bool accOn = false;      // the radio has ACC applied and the bus was alive within the last 5 s (accFromBus above)
    // The car needs the device: reverse or active parking sensors (the same conditions as
    // carNeedsDevice() from diag_types.h, but from the loop's fresh variables: powerOut.reverse ||
    // (beeper.frameFresh(now) && beeper.last().anyActive())). When true: Wi-Fi does not start or
    // reconfigure, flash-writing requests wait in the queue, firmware confirmation and
    // a scheduled restart wait, an upload in progress is aborted.
    bool carBusy = false;
    bool engineRunning = false;   // engine running: no attempts to connect to the home network
};

#if NET_ENABLED

// ---------- start ----------

// Call once in setup(), after consoleBegin(), settingsBegin() and after the drivers. In order:
//   1. enableLoopWDT() (esp32-hal.h): from this moment the main loop task is guarded by the
//      watchdog (5 s, restart). The loop calls nothing that waits for the network, so it fires
//      only on a real hang; without it a hung loop means no ACC, no beeping
//      and no sleep until the battery is disconnected. Only in the build with networking (the build without
//      networking behaves as before);
//   2. counter of fault restarts in RTC memory (RTC_NOINIT_ATTR, structure {magic, counter,
//      held}; survives a software restart and deep sleep): reset reason PANIC, INT_WDT,
//      TASK_WDT or WDT increments the counter; any other reason leaves it alone; POWERON (and a bad magic)
//      clears everything. BROWNOUT does not count (a voltage dip during engine cranking is not a program
//      error). With the counter >= NET_FAULT_RESETS_MAX the network is held back: Wi-Fi does not start
//      (like "wifi off", but without writing the settings), on USB the line
//      "[wifi] network held after crash restarts; wifi on enables it again". The counter is also cleared
//      by "wifi on" and by NET_FAULT_CLEAR_MS of uninterrupted RUNNING phase;
//   3. diagBegin, wifiMgrBegin (creates the net_wifi task; does not turn on the radio). The firmware state after
//      an update is read only by netBootCheck (otaBootCheck).
// Does not block for more than a few ms.
// false = the network did not start (prints the reason via conPrintf). The program keeps running without networking:
// after such a result the remaining functions from this file are safe and do nothing (except netBootCheck,
// which rolls back a program awaiting confirmation anyway).
bool netBegin();

// Call once in setup(), right after netBegin(). driversOk = all drivers (CAN, sound, radio UART)
// started correctly. When the running program has just been uploaded over Wi-Fi and is awaiting
// confirmation:
//   - driversOk == false OR netBegin() returned false: the program is faulty (without networking it
//     could no longer be fixed without a cable). Message, image marked invalid, restart; the bootloader
//     starts the previous program. DOES NOT RETURN. (The 12 V outputs are off: outputsBegin() was first.)
//   - an operating mode change was recorded (ota.h, otaBootCheck): confirmation right away;
//   - otherwise the countdown to confirmation starts, see netConfirmHealthy().
// In every other case it does nothing.
void netBootCheck(bool driversOk);

// ---------- on every loop pass ----------

// Every CAN frame accepted by the logic, including those pasted from the console. Never waits (details:
// diagOnCanFrame). Also counts frames per second for the state snapshot.
void netOnCanFrame(const CanFrame& f, uint32_t nowMs);

// true every STATUS_PERIOD_MS: time to take a state snapshot. Only then does main.cpp fill the structure
// (in the build without networking the function returns a constant false and the compiler removes the whole fill).
bool netStatusDue(uint32_t nowMs);

// Publishes the snapshot. main.cpp fills the fields from canOk to lastKeyMs; this function sets 'ms',
// adds the load fields, resetReason, bootId and the Wi-Fi state and hands a copy to diag_hub.
// 's' is a static structure in main.cpp (approx. 300 bytes; do not keep it on the loop stack).
// Waits for the hub lock at most DIAG_LOCK_WAIT_MAIN_MS.
void netPublishStatus(StatusSnapshot& s);

// The next request from the panel to be executed by main.cpp. Does not wait.
// When carBusy was true in the last netLoop(), only CONSOLE_LINE leaves the queue; other kinds
// (all of them write flash memory) stay at its head until this state ends (FreeRTOS queue:
// xQueuePeek, then xQueueReceive). A console line behind such a request also waits (order preserved).
// Returns true only for CONSOLE_LINE, SET_KEYMAP, RESET_KEYMAP and IMPORT_SETTINGS. It handles the rest itself:
//   SET_STA, CLEAR_STA, SET_PASSWORD: write to the settings IMMEDIATELY (result on USB via conPrintf
//     "[wifi] ...", without passwords), and wifiMgrReconfigure() after NET_APPLY_DELAY_MS (one timestamp;
//     another such request moves the deadline). The new password applies in the panel from the moment it is saved;
//   OTA_ROLLBACK: otaExecuteRollback(text[0] == 'M');
//   SET_LANG: settingsSetUiLang(text) (code from WEB_UI_LANGS) or settingsClearUiLang() for an empty text,
//     result on USB as "[lang] from the panel: ...".
// Call in a while loop: at most NET_REQ_QUEUE_LEN requests per pass.
// What main.cpp does:
//   CONSOLE_LINE : conPrintf("# www> %s\n", rq.text); handleLine(rq.text, now, false /* not from USB */);
//                  with fromUsb == false and carBusy, handleLine rejects "test", "vol N" and "map Z NA"
//                  ("[www] skipped: the car needs the device").
//   SET_KEYMAP   : if (keyCustom.parseMap(rq.text, strlen(rq.text))) {
//                      char buf[xp::KeyCustomizer::MAP_TEXT_MAX]; keyCustom.serializeMap(buf, sizeof(buf));
//                      if (settingsSetKeyMap(buf) != SetResult::OK) conPrintln("[key] table write failed");
//                  } else conPrintln("[key] bad table from the panel");
//                  (the text from serializeMap is saved, not the raw request text)
//   RESET_KEYMAP : keyCustom.resetMap(); if settingsClearKeyMap() != OK: "[key] table write failed".
bool netTakeRequest(NetRequest& out, uint32_t nowMs);

// The heart of network handling, call on EVERY loop pass, after the car logic. In order:
//   1. measures the loop load (number of passes and the longest gap between calls per second,
//      core load from the idle task counters);
//   2. remembers in.carBusy (for netTakeRequest, netRestartDue, netConfirmHealthy) and in.accOn
//      (for netRestartDue and netConfirmHealthy: restart and automatic rollback wait for ACC off) and, while an
//      upload is in progress, passes carBusy == true to otaNoteCarBusy() (the upload aborts at the
//      next chunk, also when the next 0F6 / 0E1 frame is lost during the flash write);
//   3. assembles the NetPolicy inputs: the user switch from the settings (and the hold after fault
//      restarts), in.busAlive, in.accOn, PC on USB (HWCDC::isPlugged()), otaActive = firmware
//      awaiting confirmation || scheduled restart || upload in progress, uploadBusy = otaBusy(),
//      in.engineRunning, client activity from diag_hub (a stamp newer than nowMs = age 0),
//      voltage from the last published snapshot (valid from NET_VBAT_VALID_MIN_V, threshold
//      NET_MIN_RAIL_V), Wi-Fi state;
//   4. passes on the decision: wifiMgrPoll(wifiOn, staAttempt, in.carBusy, in.engineRunning, nowMs)
//      (does not wait; the work is done by the net_wifi task);
//   5. after the deadline from netTakeRequest has passed calls wifiMgrReconfigure() (when !carBusy);
//   6. prints on USB (conPrintf) the start and end of a firmware upload when it notices a change of
//      otaBusy(): the web task itself does not write to USB. At the end of an upload also the CAN counters
//      (rx, missed, overrun) from the start and the end, to assess losses.
// Time: microseconds.
// Returns holdAwake: true = must not fall asleep despite silence on the bus. Always true while
// a firmware upload is in progress, the firmware awaits confirmation or a restart is scheduled.
bool netLoop(const NetLoopInputs& in, uint32_t nowMs);

// "The main loop is alive": call as the LAST thing of every pass, after sleepPoll().
// Matters only after a Wi-Fi update, when the firmware awaits confirmation
// (otaConfirmPoll in ota.h): after OTA_CONFIRM_AFTER_MS (20 s) of uninterrupted loop operation, with working
// drivers and a working network (own network started and the server accepting requests, or the network
// switched off by the user) the firmware confirms itself; if the network does not come up within
// OTA_CONFIRM_NET_TIMEOUT_MS, the firmware is rejected and a restart is scheduled. Writing the confirmation
// (flash memory) waits while carBusy == true.
// A loop hang before confirmation: the loop watchdog (netBegin item 1) restarts the chip,
// and the bootloader goes back to the previous program at that start. A restart before confirmation
// (crash, watchdog, power loss) also rolls the program back.
// Outside this period the function only checks one flag.
void netConfirmHealthy(uint32_t nowMs);

// ---------- update and restart ----------

// true = a firmware upload over Wi-Fi is in progress right now. CAN reception then has gaps (a write to flash
// memory stops the CAN driver interrupt), and sound may stutter. For the status LED and messages.
bool netOtaBusy();

// true = an upload is in progress or ended less than NET_FLASH_TAIL_MS ago. main.cpp passes this to
// PowerPolicy::setLossTolerant(): during this time lost 0F6 frames do not turn off ignition, ACC,
// illumination or reverse (frame 0F6 arrives every 500 ms, and the normal limit is 1000 ms).
bool netFlashBusy(uint32_t nowMs);
const uint32_t NET_FLASH_TAIL_MS = 3000;

// true = the time of a scheduled restart has come (after an update, after a rollback, on request
// from the panel) AND in the last netLoop() carBusy and accOn were false. While the car needs the device, the restart waits
// (arbitrarily long: the old program runs, holdAwake stays true, and the state passes by itself without fresh frames).
// Then main.cpp: 12 V outputs off, amplifier in standby, LED off, ESP.restart().
// Wi-Fi is not stopped beforehand: the restart resets the radio anyway, and stopping the server
// could wait for a slow client. The function prints the restart reason on the first true and right before
// returning true disconnects the clients of the device's network (deauth, approx. 100 ms), so they reconnect quickly after the restart.
bool netRestartDue(uint32_t nowMs);

// Preparation for deep sleep. Does NOT WAIT: on the first call it asks the net_wifi task
// for a full stop (web server, mDNS, WiFi.mode(WIFI_OFF)); returns true when the radio is already
// off (or was never on). Rule for sleepPoll():
//   - call when all other sleep conditions are met, before the safe state and setting up the wake-up;
//   - false: do not fall asleep in this pass (the next pass checks everything again, so
//     the car waking up meanwhile simply interrupts falling asleep);
//   - when false lasts longer than NET_STOP_WAIT_MS since the first request: print
//     "[sleep] network did not stop, sleeping anyway" and fall asleep (deep sleep cuts the radio anyway,
//     and staying awake drains the battery).
// If sleep does not happen after all (the bus came back), netLoop() starts the network by the usual rules.
bool netPrepareSleep();
const uint32_t NET_STOP_WAIT_MS = 6000;

// ---------- USB console ----------

// "wifi ..." commands from the console. Call from handleLine() as the FIRST check (before
// parseCandumpLine). Returns true when the line started with the word "wifi" (handled, also when invalid).
// fromUsb = false (line from the web panel; the web task does not accept such lines, this is only a second barrier):
// the command is NOT executed, the function prints "[wifi] USB console only" and returns true.
// Reason: USB means physical access to the device, and only there may the password be changed without the old one.
//   wifi                  state: name and address of the own network, number of clients, home network and its address,
//                         name espcanbox.local. WITHOUT the password (wifiMgrPrintInfo)
//   wifi showpass         device password, one line on USB only (wifiMgrPrintPassword).
//                         Do not keep the "[wifi] password:" line in logs intended for publication
//   wifi pass <new>       new device password (10..63 printable characters without spaces); the network restarts
//   wifi sta <name>|<password>   home network; split at the FIRST '|' character (names containing '|'
//                         are set from the panel); password mandatory, 8..63 characters or 64 hex digits (open networks rejected)
//   wifi sta off          forget the home network
//   wifi lan on|off       allow changes from the home network (password, Wi-Fi, commands, keys, restart,
//                         update); default off: from the home network the panel is read-only. Persisted
//   wifi off | wifi on    network switch, persisted across restarts; "wifi on" also clears the counter
//                         of failed starts and the counter of fault restarts
//   wifi newpass          new random device password; prints "[wifi] the old password stops working
//                         on all devices". With the radio running the password is generated
//                         and saved right away (settingsGeneratePassword + settingsSetDevicePassword),
//                         then wifiMgrReconfigure(), so there is no moment without a password. With the radio
//                         off only settingsClearDevicePassword() (the network start generates a new one, step 1)
// Old (Polish) names kept as aliases: wifi haslo = wifi showpass, wifi nowehaslo = wifi newpass.
// All reply lines start with "[wifi] ". Settings are written immediately (a command
// from USB means a human is present at the car); Wi-Fi reconfiguration is done by the net_wifi task.
// During a firmware upload "wifi off/pass/sta/newpass" write the settings, but Wi-Fi stays
// on until the upload ends: "[wifi] firmware upload in progress, the change follows when it ends".
bool netConsoleCommand(const char* line, bool fromUsb, uint32_t nowMs);

// Suffix for the console help line: " | wifi [showpass|pass|sta|lan|on|off|newpass]"
// (empty text in the build without networking).
const char* netConsoleHelp();

// Comma-separated codes of the panel dictionaries built into this image (WEB_UI_LANGS from the generated
// web_langs.h, e.g. "de,en,es,pl,ru,uk"). "" in the build without networking (no panel).
const char* netUiLangs();

// Delay of the Wi-Fi reconfiguration after a request from the panel (password change, home network): the HTTP response
// must reach the browser before the network disappears.
const uint32_t NET_APPLY_DELAY_MS = 1000;
// Fault-restart count from which the network is held back, and the run time that clears it.
const uint8_t NET_FAULT_RESETS_MAX = 2;
const uint32_t NET_FAULT_CLEAR_MS = 300000;

#else   // ---------- build without networking: empty functions ----------

static inline bool netBegin() { return false; }
static inline void netBootCheck(bool) {}
static inline void netOnCanFrame(const CanFrame&, uint32_t) {}
static inline bool netStatusDue(uint32_t) { return false; }
static inline void netPublishStatus(StatusSnapshot&) {}
static inline bool netTakeRequest(NetRequest&, uint32_t) { return false; }
static inline bool netLoop(const NetLoopInputs&, uint32_t) { return false; }
static inline void netConfirmHealthy(uint32_t) {}
static inline bool netOtaBusy() { return false; }
static inline bool netFlashBusy(uint32_t) { return false; }
static inline bool netRestartDue(uint32_t) { return false; }
static inline bool netPrepareSleep() { return true; }
static inline bool netConsoleCommand(const char*, bool, uint32_t) { return false; }
static inline const char* netConsoleHelp() { return ""; }
static inline const char* netUiLangs() { return ""; }
const uint32_t NET_STOP_WAIT_MS = 6000;

#endif  // NET_ENABLED
