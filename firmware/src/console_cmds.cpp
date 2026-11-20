// Console commands (description in console_cmds.h). Code cut out of main.cpp (17.11.2026) without changing
// the line contents: the USB port format is read by tools.
#include "console_cmds.h"

#include <Arduino.h>

#include "app_state.h"
#include "audio_out.h"
#include "can_bus.h"
#include "candump.h"
#include "config.h"
#include "console.h"
#include "line_assembler.h"
#include "loop_policy.h"
#include "net/net.h"
#include "outputs.h"
#include "radio_uart.h"
#include "settings.h"
#include "settings_backup.h"
#include "ui_lang.h"
#include "version.h"
#if CAN_TX_MODE == CAN_TX_SETTINGS
#include "can_tx/can_tx_port.h"
#endif

static const bool DECODER_MODE = (WORK_MODE == MODE_DECODER);

static void printStat(uint32_t now) {
    CanStats s = canStats();
    conPrintf("[stat] mode=%s can=%s audio=%s radio=%s\n", DECODER_MODE ? "DECODER" : "alongside the box",
              app.canOk ? "ok" : "FAIL", app.audioOk ? "ok" : "FAIL", app.radioOk ? "ok" : "FAIL");
    conPrintf("[stat] rx=%lu lost=%lu errors=%lu bus=%s volume=%u%%",
              (unsigned long)s.received, (unsigned long)s.missed, (unsigned long)s.busErrors,
              decoder.busAlive(now) ? "active" : "quiet", audioVolume());
#if HAS_VBAT_DIVIDER
    // Rail BEHIND diode D1 (approx. 0.8 V below the battery). Below NET_VBAT_VALID_MIN_V it is divider noise
    // when powered from USB, not a voltage: the same threshold as in the panel (field batteryV = null).
    float vbat = batteryVolts();
    if (vbat >= NET_VBAT_VALID_MIN_V) conPrintf(" supply=%.2fV (after the diode)", vbat);
    else conPrintf(" supply=no 12V (%.2fV)", vbat);
#endif
    conPrintln();
    const PowerOutputs& po = decoder.power();
    conPrintf("[stat] ignition=%u (0x%02X) acc=%u reverse=%u lights=%u\n", po.ignition, decoder.ignitionRaw(),
              po.acc, po.reverse, po.illumination);
    char park[160];                // "[park] ..." line with the state from the last 0E1 frame
    decoder.formatParking(park, sizeof(park));
    conPrintf("%s", park);
    // CAN transmit gate: in the program without transmission always "can-tx=none" and zeros.
    decoder.formatCanTx(park, sizeof(park));
    conPrintf("%s", park);
    // Overnight record (RTC memory): wake-ups, parking sensors state changes while asleep, 1A9 presses / releases.
    decoder.formatParkDiag(park, sizeof(park));
    conPrintf("%s", park);
}

static void printRadio() {
    const RadioInfo& r = decoder.link().info();
    const xp::ParserStats& p = radioUartParserStats();
    RadioUartStats u = radioUartStats();
    conPrintf("[radio] transmit=%s received frames=%lu acks=%lu bad checksum=%lu truncated=%lu garbage=%lu\n",
              radioUartTxEnabled() ? "YES" : "no", (unsigned long)p.frames, (unsigned long)p.acks,
              (unsigned long)p.badChecksum, (unsigned long)p.timeouts, (unsigned long)p.strayBytes);
    conPrintf("[radio] sent frames=%lu held back=%lu lost in queue=%lu lost keys=%lu\n",
              (unsigned long)u.framesSent, (unsigned long)u.framesSuppressed,
              (unsigned long)decoder.link().txDropped(), (unsigned long)decoder.keys().dropped());
    // Supervision counters: frames dropped because the radio has no ACC (usually buttons and sensors with
    // the radio off), and status frames skipped for lack of space (should be 0).
    conPrintf("[radio] dropped without ACC=%lu no slot for a state frame=%lu too long to frame=%lu\n",
              (unsigned long)decoder.link().txGated(), (unsigned long)decoder.link().slotsFull(),
              (unsigned long)decoder.radioTxTooLong());
    if (!r.everHeard) {
        conPrintln("[radio] the radio has not spoken yet");
        return;
    }
    conPrintf("[radio] on=%u volume=%u%s", r.radioOn, r.volume, r.muted ? " (muted)" : "");
    if (r.haveTime) conPrintf(" time=20%02u-%02u-%02u %02u:%02u", r.year, r.month, r.day, r.hour, r.minute);
    conPrintf(" source=%02X %02X\n", r.source[0], r.source[1]);
    conPrintf("[radio] settings: %lu (last no. %u = %u), climate: %lu (last no. %u = %u), unknown: %lu\n",
              (unsigned long)r.settingCount, r.lastSettingId, r.lastSettingValue,
              (unsigned long)r.airCount, r.lastAirId, r.lastAirValue, (unsigned long)r.unknownCount);
}

static void printVersion() {
    const FwIdent& id = fwIdent();
    conPrintf("[ver] %s %s %s, built %s\n", id.product, id.version, fwModeName(id.mode), id.build);
}

static void printKeyMap() {
    for (uint8_t i = 0; i < xp::KeyCustomizer::MAP_SIZE; i++) {
        const xp::KeyCustomizer::Entry& e = decoder.keyMap().entry(i);
        if (e.used) conPrintf("[key] swap %02X -> %02X\n", e.from, e.to);
    }
}

// Writes to flash memory (NVS) stop CAN reception for a few ms. When the car needs the device
// (reverse, active sensors), a write from the USB console is deferred: the change takes effect immediately in
// RAM, and goes to flash when that state ends (cmdPollDeferredSaves). Decision: lib/core/loop_policy.h.
// From the web panel such commands are rejected entirely at that time (as before, cmdHandleLine).
static SaveDeferral saves;
static uint8_t pendingVolume = 0;      // volume waiting to be saved (valid with SaveDeferral::VOLUME)

// Saving the remap table to the settings in canonical form (serializeMap), not the raw text.
// Write to flash memory: only on explicit command (stops CAN reception for a few ms).
static bool writeKeyMap() {
    char buf[xp::KeyCustomizer::MAP_TEXT_MAX];
    return decoder.keyMap().serializeMap(buf, sizeof(buf)) && settingsSetKeyMap(buf) == SetResult::OK;
}

// carBusy = true only for USB commands at a time when the car needs the device.
static void saveKeyMap(bool carBusy) {
    if (!saves.request(SaveDeferral::KEYMAP_SAVE, carBusy)) {
        conPrintln("[key] table write deferred: the car needs the device");
        return;
    }
    if (!writeKeyMap()) conPrintln("[key] table write failed");
}

static void clearKeyMap(bool carBusy) {
    if (!saves.request(SaveDeferral::KEYMAP_CLEAR, carBusy)) {
        conPrintln("[key] table write deferred: the car needs the device");
        return;
    }
    if (settingsClearKeyMap() != SetResult::OK) conPrintln("[key] table write failed");
}

// Transmitting on CAN: disarming ("cantx off") takes effect immediately in RAM; when the car
// needs the device, only the write to flash memory waits (like the other settings writes). Other writes
// (arming, item mask) are then rejected entirely.
static bool txClearPending = false;

// Deferred erasing of the CAN transmit arming ("cantx off" while reversing / sensors active).
static void pollTxClear(uint32_t now) {
    if (!txClearPending || decoder.carBusy(now)) return;
    txClearPending = false;
    conPrintf("[cantx] deferred disarm write: %s\n",
              settingsClearCanTxTag() == SetResult::OK ? "done" : "FAILED");
}

// Deferred writes: at most one per loop pass and only when the car does not need the device.
void cmdPollDeferredSaves(uint32_t now) {
    if (saves.pending() == 0) {
        pollTxClear(now);          // at most one write per loop pass
        return;
    }
    uint8_t what = saves.takeDue(decoder.carBusy(now));
    if (what == SaveDeferral::VOLUME) {
        bool ok = settingsSetVolume(pendingVolume) == SetResult::OK;
        conPrintf("[audio] deferred volume write %u%%: %s\n", pendingVolume, ok ? "done" : "FAILED");
    } else if (what == SaveDeferral::KEYMAP_SAVE) {
        conPrintf("[key] deferred table write: %s\n", writeKeyMap() ? "done" : "FAILED");
    } else if (what == SaveDeferral::KEYMAP_CLEAR) {
        conPrintf("[key] deferred table write: %s\n",
                  settingsClearKeyMap() == SetResult::OK ? "done" : "FAILED");
    }
}

// ---------- panel language and settings backup ----------
// Both write flash memory: never while the car needs the device (from USB the command is refused, from the
// panel the request waits in the queue, net.h) and never inside the CAN transmitter window (main.cpp does not
// call the console or the request queue while the window is open). Equal values write nothing (settings.cpp).

static void printLang() {
    const char* list = netUiLangs();
    if (list[0] == 0) {
        conPrintln("[lang] no web panel in this build");
        return;
    }
    char code[SET_UI_LANG_CAP];
    const bool stored = settingsGetUiLang(code, sizeof(code));
    if (stored && uilang::inList(code, list))
        conPrintf("[lang] panel language=%s (stored in the device); available: %s\n", code, list);
    else if (stored)
        conPrintf("[lang] panel language=en (stored %s is not in this image); available: %s\n", code, list);
    else
        conPrintf("[lang] panel language=en (default, nothing stored); available: %s\n", list);
}

// "lang" = show, "lang CODE" = store, "lang default" = forget (English).
static void cmdLang(const char* arg, bool carBusy) {
    const char* list = netUiLangs();
    if (*arg == 0 || list[0] == 0) {
        printLang();
        return;
    }
    if (carBusy) {
        conPrintln("[lang] skipped: the car needs the device (reverse / sensors), try again shortly");
        return;
    }
    SetResult r;
    if (strcmp(arg, "default") == 0) {
        r = settingsClearUiLang();
    } else if (!uilang::inList(arg, list)) {
        conPrintf("[lang] unknown language; available: %s | default\n", list);
        return;
    } else {
        r = settingsSetUiLang(arg);
    }
    if (r != SetResult::OK) conPrintln("[lang] settings write failed, nothing changed");
    else printLang();
}

static void printSettings() {
    static backup::Values v;       // static, not on the loop stack
    static char text[backup::TEXT_MAX];
    settingsExport(v);
    if ((v.present & backup::F_LANG) && !uilang::inList(v.lang, netUiLangs())) v.present &= (uint8_t)~backup::F_LANG;
    if (backup::format(v, text, sizeof(text))) conPrintf("[settings] %s\n", text);
    else conPrintln("[settings] export failed");
}

// Checks the values an import would write (the same rules as the single commands). nullptr = all fine,
// otherwise the name of the first bad field. Nothing is written.
static const char* importProblem(const backup::Values& v) {
    const uint8_t given = (uint8_t)(v.present & (uint8_t)~v.isNull);
    if ((given & backup::F_LANG) && !uilang::inList(v.lang, netUiLangs())) return "lang";
    // Stored only from AUDIO_VOLUME_MIN_SAVED: a backup must not mute the sensor beeping permanently.
    if ((given & backup::F_VOL) && v.vol < AUDIO_VOLUME_MIN_SAVED) return "vol";
    if ((given & backup::F_KEYS) && !settingsKeyMapAcceptable(v.keys)) return "keys";
    return nullptr;
}

static const char* resultWord(SetResult r) { return r == SetResult::OK ? "ok" : "FAILED"; }

// Writes the checked values one by one; a failed write leaves the others done (reported per field).
// net / lan are never imported: they decide who can reach the device (USB commands wifi on|off, wifi lan on|off).
static void applyImport(const backup::Values& v) {
    const char* langRes = "-";
    const char* volRes = "-";
    const char* keysRes = "-";
    if (v.present & backup::F_LANG)
        langRes = resultWord((v.isNull & backup::F_LANG) ? settingsClearUiLang() : settingsSetUiLang(v.lang));
    if (v.present & backup::F_VOL) {
        if (v.isNull & backup::F_VOL) {
            audioSetVolume((uint8_t)AUDIO_VOLUME_DEFAULT);
            volRes = resultWord(settingsClearVolume());
        } else {
            audioSetVolume(v.vol);                       // audio_out applies AUDIO_VOLUME_MAX
            volRes = resultWord(settingsSetVolume(audioVolume()));
        }
    }
    if (v.present & backup::F_KEYS) {
        if (v.isNull & backup::F_KEYS) {
            decoder.keyMap().resetMap();
            keysRes = resultWord(settingsClearKeyMap());
        } else if (decoder.keyMap().parseMap(v.keys, strlen(v.keys))) {
            keysRes = writeKeyMap() ? "ok" : "FAILED";
        } else {
            keysRes = "FAILED";                          // checked before; should not happen
        }
    }
    conPrintf("[settings] import: lang=%s vol=%s keys=%s\n", langRes, volRes, keysRes);
    if (v.present & (backup::F_NET | backup::F_LAN))
        conPrintln("[settings] net and lan are not imported: USB commands wifi on|off and wifi lan on|off");
}

// "settings import {...}" from the console (USB or panel line).
static void cmdImport(const char* json, bool carBusy) {
    static backup::Values v;
    const backup::ParseError pe = backup::parse(json, strlen(json), v);
    if (pe != backup::ParseError::OK) {
        conPrintf("[settings] import refused (%s), nothing changed\n", backup::errorName(pe));
        return;
    }
    const char* bad = importProblem(v);
    if (bad != nullptr) {
        conPrintf("[settings] import refused (bad %s), nothing changed\n", bad);
        return;
    }
    if (carBusy) {
        conPrintln("[settings] skipped: the car needs the device (reverse / sensors), try again shortly");
        return;
    }
    applyImport(v);
}

// ---------- transmitting on CAN: state, arming, item mask ----------
bool cmdSavesPending() { return saves.pending() != 0 || txClearPending; }

static void printCanTx() {
#if CAN_TX_MODE == CAN_TX_SETTINGS
    conPrintf("[cantx] built=1 armed=%u after_restart=%u controller=%s mask=0x%08lX\n", app.txArmed,
              app.txArmNext, canListenOnly() ? "listen" : "NORMAL", (unsigned long)decoder.settingsMask());
    static char row[200];          // static, not on the loop stack
    decoder.formatCanTxStatus(row, sizeof(row));
    conPrintf("%s", row);
    const CanAlertInfo& ai = canAlertInfo();
    conPrintf("[cantx] controller: bus errors=%lu arbitration=%lu no ack=%lu bus-off=%lu recoveries=%lu "
              "window counter=%lu\n",
              (unsigned long)ai.busErrorAlerts, (unsigned long)ai.arbLost, (unsigned long)ai.txFailed,
              (unsigned long)ai.busOff, (unsigned long)ai.recovered, (unsigned long)canTxPortHwTimeouts());
#else
    conPrintln("[cantx] built=0: no CAN transmit in this build (CAN listen only)");
#endif
}

static void handleTxCommand(const cantx::Parsed& c, uint32_t now, cantx::Source src) {
    if (c.cmd == cantx::Cmd::BAD) {
        conPrintln("[cantx] bad syntax. Commands: cantx | cantx on | cantx off | cantx reset | vset on NN | vset off NN | set NN VV");
        return;
    }
    if (c.cmd == cantx::Cmd::STATUS) {
        printCanTx();
        return;
    }
#if CAN_TX_MODE != CAN_TX_SETTINGS
    (void)now;
    (void)src;
    conPrintln("[cantx] REFUSED: this program cannot transmit on CAN");
#else
    if (!cantx::allowed(c.cmd, src)) {
        // Commands allowed from the panel only on the device's own network; the rest only from USB.
        if (cantx::allowed(c.cmd, cantx::Source::PANEL_AP))
            conPrintln("[cantx] REFUSED: from the panel only through the device's own network (not the home network)");
        else
            conPrintln("[cantx] REFUSED: this command is USB only");
        return;
    }
    const bool busy = decoder.carBusy(now);
    switch (c.cmd) {
        case cantx::Cmd::ARM_ON:
            if (busy) {
                conPrintln("[cantx] skipped: the car needs the device (reverse / sensors), try again shortly");
                break;
            }
            txClearPending = false;
            if (settingsSetCanTxTag(fwIdent().build) != SetResult::OK) {
                conPrintln("[cantx] arm write FAILED, nothing changed");
                break;
            }
            app.txArmNext = true;
            conPrintf("[cantx] arming stored. Takes effect AFTER A RESTART of the device (now armed=%u). "
                      "Flashing another program disarms.\n", app.txArmed);
            break;
        case cantx::Cmd::ARM_OFF:
            decoder.setCanTxArmed(false);          // immediately: the gate refuses, the transmitter is closed
            app.txArmed = false;
            app.txArmNext = false;
            {
                // Disarming is ALWAYS saved immediately, also while reversing / sensors active: a short write
                // is a lesser evil than arming coming back after a reset (review 18.11.2026).
                txClearPending = false;
                conPrintf("[cantx] DISARMED immediately%s\n",
                          settingsClearCanTxTag() == SetResult::OK ? "" : "; flash write FAILED (it may come back after a restart)");
            }
            if (!canListenOnly()) conPrintln("[cantx] until a restart the controller stays in NORMAL mode with the transmitter closed");
            break;
        case cantx::Cmd::RESET:
            decoder.canTxClearFault();
            conPrintln("[cantx] latch cleared");
            printCanTx();
            break;
        case cantx::Cmd::MASK_ON:
        case cantx::Cmd::MASK_OFF: {
            uint32_t mask = 0;
            const bool on = c.cmd == cantx::Cmd::MASK_ON;
            const vset::MaskResult r = vset::maskChange(decoder.settingsMask(), c.id, on, mask);
            if (r == vset::MaskResult::UNKNOWN_ID) {
                conPrintf("[vset] item %02X is not in the table\n", c.id);
                break;
            }
            if (r == vset::MaskResult::NOT_SENDABLE) {
                conPrintf("[vset] item %02X cannot be enabled: fixed refusal in the program (no measurement or no frame)\n", c.id);
                break;
            }
            if (busy) {
                conPrintln("[vset] skipped: the car needs the device (reverse / sensors), try again shortly");
                break;
            }
            if (settingsSetVsetMask(mask) != SetResult::OK) {
                conPrintln("[vset] mask write FAILED, nothing changed");
                break;
            }
            decoder.setSettingsMask(mask);
            const vset::SettingDef* d = vset::find(c.id);
            conPrintf("[vset] item %02X (%s) %s, mask=0x%08lX\n", c.id, d ? d->name : "?", on ? "ENABLED" : "disabled",
                      (unsigned long)mask);
            break;
        }
        case cantx::Cmd::SET:
            // The same path as the radio command 80 NN VV (the same refusals, the same gate).
            conPrintf("[vset] command set %02X %02X (like radio command 80 %02X %02X)\n", c.id, c.value, c.id, c.value);
            decoder.requestSetting(c.id, c.value, now);
            break;
        default:
            break;
    }
#endif
}

// src = where the line came from: USB console or web panel (from the device's own network / via the home network).
void cmdHandleLine(char* line, uint32_t now, cantx::Source src) {
    const bool fromUsb = src == cantx::Source::USB;
    // "wifi ..." commands first: "wifi sta AB#..." must not be taken for a frame.
    if (netConsoleCommand(line, fromUsb, now)) return;
    // Transmitting on CAN: cantx, cantx on|off|reset, vset on|off NN, set NN VV. A bare "vset" goes below.
    const cantx::Parsed txc = cantx::parse(line);
    if (txc.cmd != cantx::Cmd::NONE) {
        handleTxCommand(txc, now, src);
        return;
    }
    CanFrame f;
    if (parseCandumpLine(line, f)) {          // frame pasted by hand
        handleFrame(f, now, fromUsb);
        return;
    }
    // From the panel, when the car needs the device (reverse, sensors): no test beeping (it would drown out
    // the sensors) and no writes to flash memory (CAN reception stops then). From USB as before, immediately.
    unsigned from = 0, to = 0;
    bool mapSet = strncmp(line, "map", 3) == 0 && sscanf(line + 3, "%x %x", &from, &to) == 2;
    bool mapReset = strcmp(line, "map reset") == 0;     // also a flash write (table removal)
    const bool settingsWrite = strncmp(line, "lang ", 5) == 0 || strncmp(line, "settings import ", 16) == 0;
    if (!fromUsb && decoder.carBusy(now) &&
        (strcmp(line, "test") == 0 || strncmp(line, "vol ", 4) == 0 || mapSet || mapReset || settingsWrite)) {
        conPrintln("[www] skipped: the car needs the device");
        return;
    }
    // From USB the command takes effect immediately, but the flash write waits until the car no longer needs
    // the device. From the panel carBusy is already false at this point (above), so the write goes immediately.
    const bool deferSave = fromUsb && decoder.carBusy(now);
    if (strcmp(line, "stat") == 0) {
        printStat(now);
    } else if (strcmp(line, "radio") == 0) {
        printRadio();
    } else if (strcmp(line, "ver") == 0) {
        printVersion();
    } else if (strncmp(line, "vol ", 4) == 0) {
        audioSetVolume((uint8_t)constrain(atoi(line + 4), 0, 100));
        uint8_t v = audioVolume();
        // Stored only from AUDIO_VOLUME_MIN_SAVED: a single "vol 0" must not permanently mute
        // the sensor beeping (in decoder mode it is the only beeping in the car).
        if (v >= AUDIO_VOLUME_MIN_SAVED) {
            conPrintf("[audio] volume=%u%%\n", v);
            if (!saves.request(SaveDeferral::VOLUME, deferSave)) {
                pendingVolume = v;
                conPrintln("[audio] volume write deferred: the car needs the device");
            } else if (settingsSetVolume(v) != SetResult::OK) {
                conPrintln("[audio] volume write failed");
            }
        } else {
            conPrintf("[audio] volume=%u%% (below %d%% only until restart)\n", v, AUDIO_VOLUME_MIN_SAVED);
        }
    } else if (strcmp(line, "test") == 0) {
        app.test.start(now, 1500);                // 1.5 s of test beeping without the car
        conPrintln("[audio] test");
    } else if (strncmp(line, "key ", 4) == 0) {
        // Button code test: sends "pressed" and "released" to the radio (works in decoder mode).
        uint8_t code = (uint8_t)strtoul(line + 4, nullptr, 16);
        decoder.injectKey(code, xp::KEY_STATE_PRESSED);
        decoder.injectKey(code, xp::KEY_STATE_RELEASED);
        conPrintf("[key] trial code %02X%s\n", code, radioUartTxEnabled() ? "" : ": not sent (listen mode)");
    } else if (strncmp(line, "kd ", 3) == 0 || strncmp(line, "ku ", 3) == 0) {
        // Only "pressed" (kd) or only "released" (ku): for testing keys that the radio
        // treats differently when held (e.g. screen blanking).
        uint8_t code = (uint8_t)strtoul(line + 3, nullptr, 16);
        uint8_t state = line[1] == 'd' ? xp::KEY_STATE_PRESSED : xp::KEY_STATE_RELEASED;
        decoder.injectKey(code, state);
        conPrintf("[key] manual code=%02X state=%u%s\n", code, state,
                  radioUartTxEnabled() ? "" : ": not sent (listen mode)");
    } else if (strcmp(line, "map reset") == 0) {
        // Back to the default entries of the car profile (PSA AEE2010: TEL, assistant button; generic: none);
        // save = removal from the settings.
        decoder.keyMap().resetMap();
        clearKeyMap(deferSave);
        printKeyMap();
    } else if (strncmp(line, "map", 3) == 0) {
        // "map 9B 1F": the button with code 9B sends 1F to the radio. "map 9B 9B" reverts. A bare "map" = list.
        // The change is stored in the settings and comes back after a restart.
        if (mapSet) {
            if (!decoder.keyMap().setMap((uint8_t)from, (uint8_t)to)) conPrintln("[key] swap table full");
            else saveKeyMap(deferSave);
        }
        printKeyMap();
    } else if (strcmp(line, "lang") == 0 || strncmp(line, "lang ", 5) == 0) {
        cmdLang(line[4] == ' ' ? line + 5 : line + 4, decoder.carBusy(now));
    } else if (strcmp(line, "settings") == 0) {
        printSettings();
    } else if (strncmp(line, "settings import ", 16) == 0) {
        cmdImport(line + 16, decoder.carBusy(now));
    } else if (strcmp(line, "doorswap") == 0) {
        decoder.toggleDoorSwap(now);
        conPrintf("[car] door side swap=%u; until restart\n", decoder.doorSwap());
    } else if (strcmp(line, "dump") == 0) {
        app.dumpCan = !app.dumpCan;
        conPrintf("# CAN frame dump=%u\n", app.dumpCan);
    } else if (strcmp(line, "rxlog") == 0) {
        radioUartSetLog(!radioUartLog());
        conPrintf("[radio_rx] printing=%u\n", radioUartLog());
    } else if (strcmp(line, "vset") == 0 || strcmp(line, "ust") == 0) {   // "ust": old name, kept as an alias
        // Vehicle settings table (radio screen "Function settings"): command 80 number, name, state
        // from CAN frame 260, base, mask and the result of the pure 15B frame logic. Read only: transmits nothing.
        static char row[200];      // static, not on the loop stack
        decoder.formatSettingsHeader(now, row, sizeof(row));
        conPrintf("%s", row);
        decoder.formatSettingsDiff(row, sizeof(row));   // difference of 260 relative to the state at startup
        conPrintf("%s", row);
        for (uint8_t i = 0; i < decoder.settingsCount(); i++) {
            decoder.formatSettingRow(i, now, row, sizeof(row));
            conPrintf("%s", row);
        }
    } else if (strcmp(line, "keys") == 0) {
        decoder.setKeyLog(!decoder.keyLog());
        conPrintf("[key] printing=%u\n", decoder.keyLog());
    } else if (line[0] != 0) {
        if (strcmp(line, "help") != 0) conPrintf("Unknown command: %s\n", line);
        // The panel rejects "wifi" commands anyway (they contain passwords): from the panel they are left out of the help.
        conPrintf("Commands: stat | radio | ver | vset | cantx | vol 0-100 | test | keys | key/kd/ku CODE | map FROM TO | map reset"
                  " | lang [CODE|default] | settings [import JSON] | doorswap | rxlog | dump%s | frame e.g. 0E1#B0F000FCFC1EC200\n",
                  fromUsb ? netConsoleHelp() : "");
    }
}

// USB lines may be longer than panel lines: "settings import " (16) + the longest export (backup::TEXT_MAX - 1)
// must fit, so that the text printed by "settings" can be pasted back (review: it was cut at 127 characters).
static const size_t USB_LINE_MAX = 16 + backup::TEXT_MAX;
static_assert(USB_LINE_MAX >= CON_LINE_MAX, "a USB line must hold every panel line");

// Work per call (per main loop pass), like RX_BUDGET in src/radio_uart.cpp: a capture pasted or streamed into the
// USB console must not hold the loop (CAN receive, radio UART, watchdog) until the whole burst is read. The rest
// stays in the USB receive buffer for the next passes (the host is held back by USB flow control).
static const int USB_RX_BUDGET = 256;     // bytes per call
static const int USB_LINES_MAX = 2;       // complete lines handled per call

void cmdPollUsb(uint32_t now) {
    static char buf[USB_LINE_MAX];
    static LineAssembler line(buf, sizeof(buf));    // lib/core/line_assembler.h: an overlong line is dropped whole
    int budget = USB_RX_BUDGET;
    int lines = 0;
    while (budget-- > 0 && Serial.available() > 0) {
        const int c = Serial.read();
        if (c < 0) break;
        const LineAssembler::Result r = line.feed((char)c);
        if (r == LineAssembler::TOO_LONG) {
            conPrintf("[con] line longer than %u characters: ignored\n", (unsigned)(USB_LINE_MAX - 1));
        } else if (r == LineAssembler::LINE) {
            cmdHandleLine(line.buffer(), now, cantx::Source::USB);
            // A line (e.g. "set NN VV") may have opened the transmitter window. The next lines of the same
            // burst could write flash (lang, vol, map, settings import): a flash write stops both cores and
            // the window timer. Leave them in the USB buffer for a later loop pass, when the window is closed.
            const TxState s = decoder.canTxState();
            if (s == TxState::OPENING || s == TxState::SENDING) return;
            if (++lines >= USB_LINES_MAX) return;   // the next lines on a later pass
        }
    }
}

// #if: in the program without network the queue is always empty anyway, but the compiler kept in RAM
// the static buffer of the function below (checked in the memory map 17.11.2026).
#if NET_ENABLED
void cmdPollNetRequests(uint32_t now) {
    static NetRequest rq;          // approx. 200 bytes: static, not on the loop stack
    while (netTakeRequest(rq, now)) {
        switch (rq.kind) {
            case NetReqKind::CONSOLE_LINE:
                conPrintf("# www> %s\n", rq.text);
                // fromAp: the web task checked the address of the other side of the connection (httpPeerIsAp).
                cmdHandleLine(rq.text, now, rq.fromAp ? cantx::Source::PANEL_AP : cantx::Source::PANEL_OTHER);
                break;
            case NetReqKind::SET_KEYMAP:
                // Requests from the panel save immediately, as before: the network keeps them queued as long as the car
                // needs the device (net.cpp, netTakeRequest).
                if (decoder.keyMap().parseMap(rq.text, strlen(rq.text))) saveKeyMap(false);
                else conPrintln("[key] bad table from the panel");
                break;
            case NetReqKind::RESET_KEYMAP:
                decoder.keyMap().resetMap();
                clearKeyMap(false);
                break;
            case NetReqKind::IMPORT_SETTINGS: {
                // Checked by the web task (api_handlers.cpp); checked again here with the same rules.
                // The queue holds this request while the car needs the device (net.h).
                static backup::Values v;
                v = backup::Values();
                v.present = rq.setPresent & (backup::F_LANG | backup::F_VOL | backup::F_KEYS);
                v.isNull = rq.setNull & v.present;
                v.vol = rq.setVol;
                memcpy(v.lang, rq.text2, sizeof(v.lang) - 1);
                memcpy(v.keys, rq.text, sizeof(v.keys) - 1);
                const char* bad = importProblem(v);
                if (bad != nullptr) conPrintf("[settings] import from the panel refused (bad %s)\n", bad);
                else applyImport(v);
                break;
            }
            default:
                break;             // other kinds are handled by net.cpp
        }
    }
}
#endif
