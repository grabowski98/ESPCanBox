// Data exchanged between the main loop and the web panel. Structures only, no networking code:
// the file is also included in the build without networking (via net.h), so main.cpp needs no #if.
//
//   StatusSnapshot : "state snapshot" taken by the main loop every STATUS_PERIOD_MS. The web server task
//                    reads only the copy from src/net/diag_hub and never touches the logic objects.
//   NetRequest     : request from the panel to be executed in the main loop (console line, key table,
//                    Wi-Fi settings, firmware rollback). Fixed size, no pointers.
//
// The file does not depend on Arduino (it also compiles on a PC).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "can_bus.h"          // CanStats
#include "console.h"          // CON_LINE_MAX
#include "psa_frames.h"       // psa::ParkingAid
#include "radio_protocol.h"   // RadioInfo, RadioRxStats
#include "radio_uart.h"       // RadioUartStats
#include "settings.h"         // SET_SSID_MAX, SET_STA_PASS_MAX
#include "xp_keys_custom.h"   // xp::KeyCustomizer::MAP_SIZE
#include "xp_protocol.h"      // xp::ParserStats (= RadioRxStats)

// How often the main loop publishes the state snapshot. 200 ms: the sensor chart in the panel keeps up with
// the car (frame 0E1 arrives every 100 ms), and copying about 300 bytes 5 times a second costs nothing.
const uint32_t STATUS_PERIOD_MS = 200;

// ---------- Wi-Fi state (filled by src/net/wifi_mgr, main loop) ----------

enum class WifiPhase : uint8_t {
    OFF = 0,     // radio off
    STARTING,    // start-up in progress (successive steps in successive loop passes)
    RUNNING,     // own network is up, the web server accepts requests
    STOPPING,    // shutdown in progress
    FAILED,      // start failed; retry after the WIFI_RETRY_MS pause or, with gaveUp, never
};

struct WifiStatus {
    WifiPhase phase = WifiPhase::OFF;
    char apSsid[SET_SSID_MAX + 1] = {0};     // name of the own network: NET_AP_SSID_PREFIX + e.g. "A1B2"
    uint8_t apIp[4] = {0, 0, 0, 0};          // device address in its own network
    uint8_t apChannel = 0;                   // channel the network actually runs on (0 = unknown)
    uint8_t apStations = 0;                  // devices connected to the own network
    bool staConfigured = false;              // a home network is saved
    char staSsid[SET_SSID_MAX + 1] = {0};    // its name (empty when none)
    bool staConnected = false;               // connected and has an IP address
    uint8_t staIp[4] = {0, 0, 0, 0};         // address in the home network (zeros when not connected)
    int8_t staRssi = 0;                      // router signal strength in dBm (valid only with staConnected)
    uint8_t staLastReason = 0;               // code of the last disconnect (wifi_err_reason_t), 0 = none yet
    uint16_t maxStepMs = 0;                  // longest single step of the net_wifi task since program start
                                             // (for this time the net_wifi task stalls, NOT the main loop)
    uint8_t startFails = 0;                  // failed starts in a row (reset after a successful start)
    bool gaveUp = false;                     // WIFI_START_TRIES_MAX failed starts: no more tries until "wifi on"
    bool blockedByFaults = false;            // network held back after fault restarts (net.h, netBegin)
    uint16_t stackFree = 0;                  // lowest stack headroom of the net_wifi task in bytes
};

// ---------- state snapshot ----------

struct KeyMapEntry {
    uint8_t from = 0;      // key code from the car (as from the box)
    uint8_t to = 0;        // code sent to the radio
};

// Who fills it: fields from canOk to lastKeyMs by main.cpp (when netStatusDue() returns true), the ms field
// and everything below the line by net.cpp in netPublishStatus(). The web task gets a copy of the whole
// structure via diagGetStatus().
struct StatusSnapshot {
    uint32_t ms = 0;                 // time the snapshot was taken (millis); set by netPublishStatus()

    // --- drivers: start-up result in setup() ---
    bool canOk = false;
    bool audioOk = false;
    bool radioOk = false;

    // --- CAN bus ---
    CanStats can;                    // canStats()
    bool busAlive = false;           // frame within the last second (same variable as for the LED and sleep)
    bool canListenOnly = true;       // canListenOnly(): controller mode chosen at start (false only
                                     // in the transmit-capable build after an armed start)
    bool canTxArmNext = false;       // NVS holds a valid arming: the next start will be armed
    // CAN transmit gate (since 0.9.0; lib/core/can_tx_gate.h). In a build without transmit hardware:
    // canTxState = 0 (TxState::NOT_BUILT) and all zeros.
    uint8_t canTxState = 0;          // TxState as a number (decoder.canTxState())
    uint8_t canTxFault = 0;          // TxFault as a number; 0 = no latch
    uint16_t canTxTec = 0;           // controller error counters from the gate's last read
    uint16_t canTxRec = 0;
    uint32_t canTxSent = 0;          // decoder.canTxStats(): frames handed to the controller,
    uint32_t canTxAcked = 0;         // acknowledged,
    uint32_t canTxFailed = 0;        // failed,
    uint32_t canTxRefused = 0;       // requests refused by the gate

    // --- car ---
    bool ignition = false;           // PowerOutputs::ignition: ignition per frame 0F6, only with fresh frames
    uint8_t ignitionRaw = 0;         // byte 1 of frame 0F6: 0x80 off, 0x88 on, 0x8E engine running
    bool ignitionFresh = false;      // frame 0F6 not older than 1 s (otherwise ignitionRaw in JSON = null)
    bool doorsKnown = false;         // a door frame has arrived since start (otherwise doors in JSON = null)
    bool acc = false;                // PowerOutputs::acc: the radio has (or would have) ACC applied
    bool reverse = false;            // PowerOutputs::reverse: reverse gear with ignition on and a fresh frame
    bool lights = false;             // PowerOutputs::illumination: lights on (frame 036) with ignition on
    uint8_t doors = 0;               // decoder.doorState().open, vehicle::DOOR_*: 0x40 driver, 0x80 passenger, 0x10 rear left,
                                     // 0x20 rear right, 0x08 boot (layout of byte 4 of frame 1A1)
    float batteryV = 0.0f;           // batteryVolts(): voltage of the rail AFTER diode D1 (approx. 0.8 V below the battery);
                                     // 0 when the board has no divider or there is no 12 V. Measure once per snapshot.
    bool haveOutsideTemp = false;    // true when frame 0F6 (at least 7 bytes) arrived no more than 1 s ago
    float outsideTempC = 0.0f;       // byte 7 of frame 0F6 (data[6]) / 2 - 40, step 0.5 degree

    // --- vehicle settings (personalisation), read only (since 0.8.0) ---
    bool set260Seen = false;         // frame 260 has been seen since start (otherwise raw260 in JSON = null)
    uint8_t set260Len = 0;           // its length 0..8 (a valid one has 7)
    uint8_t set260[8] = {0};         // content of the last frame 260 (decoder.car().f260)
    uint32_t set260Ms = 0;           // when it arrived (millis)
    uint8_t set38[5] = {0};          // DATA[1..5] of frame 38 sent to the radio (decoder.settings38())
    bool setHaveLast = false;        // the radio has sent command 80 since start (otherwise last in JSON = null)
    uint8_t setLastId = 0;           // item number from the command
    uint8_t setLastValue = 0;        // requested value
    uint8_t setLastResult = 0;       // SettingCmdResult (lib/app/decoder.h) as a number
    uint32_t setLastMs = 0;          // when the result was produced (millis)
    uint16_t setLastTookMs = 0;      // time from command to result
    bool setLastHaveFrame = false;   // setLastFrame holds the 15B frame handed to the gate
    uint8_t setLastFrame[8] = {0};
    uint32_t setMask = 0;            // mask of enabled items (decoder.settingsMask())
    uint32_t setAccepted = 0;        // counters of decoder.settingCmds(); accepted and notAccepted = 0
    uint32_t setNotAccepted = 0;     // as long as the build cannot transmit
    uint32_t setRefused = 0;

    // --- parking sensors ---
    psa::ParkingAid park;            // beeper.last(): last frame 0E1
    bool parkFresh = false;          // beeper.frameFresh(now)
    bool beeping = false;            // BeeperOutput::active: beeping in progress (also in the gap between beeps)
    bool continuous = false;         // BeeperOutput::continuous: continuous tone

    // --- radio (UART, XP protocol) ---
    bool radioTxEnabled = false;     // radioUartTxEnabled(): only in decoder mode
    RadioInfo radio;                 // radio.info()
    xp::ParserStats radioRx;         // radioUartParserStats()
    RadioUartStats radioTx;          // radioUartStats()
    uint8_t txQueue = 0;             // radio.txQueued()
    uint8_t txQueueCap = 0;          // RadioSession::txCapacity()
    uint32_t txDropped = 0;          // radio.txDropped()
    uint32_t txGated = 0;            // radio.txGated(): frames refused because the radio has no ACC
    uint32_t slotsFull = 0;          // radio.slotsFull(): state frames skipped for lack of space
    uint32_t keysDropped = 0;        // keys.dropped()

    // --- sound ---
    uint8_t audioVolume = 0;         // audioVolume(), percent

    // --- keys ---
    uint8_t keyMapCount = 0;                             // number of valid keyMap entries
    KeyMapEntry keyMap[xp::KeyCustomizer::MAP_SIZE];     // used entries of the remap table, in table order
    bool haveLastKey = false;        // at least one key event since start
    uint8_t lastKeyCode = 0;         // last event from keys.pop(): code from the car, BEFORE remapping
    uint8_t lastKeyState = 0;        // 1 pressed, 2 held, 0 released
    uint32_t lastKeyMs = 0;          // when (millis)

    // ===== filled below by net.cpp (main.cpp does not touch these fields) =====

    // --- load, counted in one-second windows; values from the last complete window ---
    uint16_t framesPerSec = 0;       // how many times netOnCanFrame() was called
    uint16_t loopsPerSec = 0;        // how many times netLoop() was called, i.e. main loop passes
    uint32_t loopMaxUs = 0;          // longest gap between two netLoop() calls in microseconds
                                     // (includes vTaskDelay(1), so about 1000-2000 at idle)
    uint8_t loopBusyPct = 0;         // load of core 1 (main loop): 100 - share of the idle task
    uint8_t cpu0Pct = 0;             // load of core 0 (Wi-Fi, sound, web server), computed the same way
    uint8_t resetReason = 0;         // esp_reset_reason() read once at start (values 0..15)
    uint32_t bootId = 0;             // esp_random() at netBegin(); changes after every restart (only has to differ)

    WifiStatus wifi;
};

// The car "needs the device": reverse engaged or parking sensors active. In this state nothing
// may be done that stalls CAN reception or the main loop for longer: writing firmware to
// flash memory (update), saving settings and confirming the firmware (also a flash write),
// restart (including one scheduled earlier: it waits for this state to end), starting and reconfiguring Wi-Fi.
// Active sensors = fresh 0E1 frames and front or rear state equal to 4 (PARK_ACTIVE).
inline bool carNeedsDevice(const StatusSnapshot& s) {
    return s.reverse || (s.parkFresh && s.park.anyActive());
}

// Frames carrying the car's VIN: 336 (3 characters), 3B6 (6 characters), 2B6 (8 characters), 17 ASCII characters in total.
// Checked 16.11.2026 on capture test_data/captures/can_20261111_191045.log (461 frames of each ID, lengths
// 3, 6 and 8 bytes, content is printable characters). Frame export from the panel masks them by default.
inline bool isVinFrameId(uint32_t id) { return id == 0x336 || id == 0x3B6 || id == 0x2B6; }

// ---------- requests panel -> main loop ----------

enum class NetReqKind : uint8_t {
    NONE = 0,
    // Executed by main.cpp (it gets them from netTakeRequest):
    CONSOLE_LINE,    // text = command line, as typed over USB (printable ASCII, 1..CON_LINE_MAX-1 characters;
                     // lines starting with the word "wifi" are rejected by the web task and not queued)
    SET_KEYMAP,      // text = remap table in xp::KeyCustomizer::parseMap format (may be empty)
    RESET_KEYMAP,    // no data: return to the default entries of the car profile
    IMPORT_SETTINGS, // settings import (lib/core/settings_backup.h), already checked by the web task:
                     // setPresent / setNull = backup::F_* bits, text = key table, text2 = language code,
                     // setVol = volume. The panel never sends net / lan (USB only)
    // Executed by net.cpp in the main loop (main.cpp does not see them):
    SET_STA,         // text = home network name, text2 = password (8..63 characters or 64 hex digits; open networks are rejected)
    CLEAR_STA,       // no data: forget the home network
    SET_PASSWORD,    // text = new device password
    OTA_ROLLBACK,    // go back to the previous firmware and restart; text[0] == 'M' = operating mode change
    SET_LANG,        // text = panel language code from WEB_UI_LANGS, or "" = forget (English)
};

const size_t NET_REQ_TEXT_MAX = 128;                    // including the terminating zero
const size_t NET_REQ_TEXT2_MAX = SET_STA_PASS_MAX + 1;  // including the terminating zero

// Texts are always zero-terminated. Everything that arrives here has already been checked in the web task
// (length, allowed characters, syntax); the executor checks again when writing to the settings.
// Device restart is NOT a queued request: it is requested via otaScheduleRestart()
// (src/net/ota.h), so that the restart after an update does not depend on free space in the queue.
struct NetRequest {
    NetReqKind kind = NetReqKind::NONE;
    // CONSOLE_LINE: the client is in the device's own network (httpPeerIsAp, by connection addresses,
    // not by headers). This decides whether CAN transmit may be armed (lib/core/can_tx_arm.h).
    bool fromAp = false;
    uint8_t setPresent = 0;                  // IMPORT_SETTINGS only
    uint8_t setNull = 0;
    uint8_t setVol = 0;
    char text[NET_REQ_TEXT_MAX] = {0};
    char text2[NET_REQ_TEXT2_MAX] = {0};
};

// Request queue in diag_hub. 4 slots: a human clicks at most a few times per second, and the main
// loop drains the queue on every pass (about every 1 ms). Full queue = response 503 "busy".
// When the car needs the device, the main loop leaves in the queue the requests that write flash
// memory (all except CONSOLE_LINE); it executes them after this state ends (net.h, netTakeRequest).
const uint8_t NET_REQ_QUEUE_LEN = 4;

static_assert(CON_LINE_MAX <= NET_REQ_TEXT_MAX, "a console line does not fit in NetRequest::text");
static_assert(xp::KeyCustomizer::MAP_TEXT_MAX <= NET_REQ_TEXT_MAX, "the key table does not fit in NetRequest::text");
static_assert(SET_SSID_MAX + 1 <= NET_REQ_TEXT_MAX, "a network name does not fit in NetRequest::text");
static_assert(webauth::PASSWORD_MAX + 1 <= NET_REQ_TEXT_MAX, "a password does not fit in NetRequest::text");
static_assert(SET_UI_LANG_CAP <= NET_REQ_TEXT2_MAX, "a language code does not fit in NetRequest::text2");
