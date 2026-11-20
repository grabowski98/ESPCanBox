// Firmware update over Wi-Fi: receiving the file, checks, switching, confirmation after start,
// rollback to the previous firmware and restart scheduling.
//
// The flash memory has two firmware slots (app0 and app1, 3 342 336 bytes each, table default_8MB.csv).
// New firmware always goes into the unused slot; the running one is not touched. Only after
// a successful write and the checks does the bootloader get the command to start from the new slot.
// The uploaded file is .pio/build/<environment>/firmware.bin (NOT the merged image of all partitions).
//
// SAFETY:
//  - A write to flash memory stops CAN frame reception (the CAN driver interrupt does not run during
//    the write, frames are lost in the controller itself: counter rx_overrun) and may make sound stutter. Therefore
//    update, rollback, confirmation and restart are NOT performed while the car needs the device
//    (carNeedsDevice(): reverse or active sensors), and an upload in progress is aborted when
//    such a state appears. Because frames are lost during an upload, detecting reverse may take
//    1-3 s (estimate, not measured). For the duration of the upload and NET_FLASH_TAIL_MS after it main.cpp switches
//    PowerPolicy into a mode tolerant of lost frames (net.h, netFlashBusy).
//  - While the running firmware awaits confirmation (state PENDING), a new file must not be accepted
//    (it would overwrite the only verified firmware), nor may the device enter deep sleep.
//  - An image built in a different operating mode than the running one (decoder / next to the box) requires explicit
//    consent in the request: decoder mode enables the UART transmitter and the 12 V outputs, and with the factory
//    box plugged in that shorts the outputs. An image without the network panel also requires consent (after it
//    updating is no longer possible without a cable). The same consent applies to a rollback (also in state PENDING)
//    when the firmware in the other slot has a different mode.
//  - A mode change must not revert by itself: firmware uploaded with consent mode=1 confirms itself right away
//    after the first start with working drivers (otaBootCheck), with no period during which a power
//    loss (e.g. while re-plugging wires) would bring back the previous mode.
//  - The file is checked against mistakes, not against forgery (no signature). Access is protected by the password.
//    There is no hardware CAN transmitter lock (since 17.11.2026 jumper JP1 on the prototype is closed,
//    SLNT on GPIO6): transmission on the car bus is decided solely by the uploaded firmware. An image that
//    can transmit (marker canTx = 1) requires at upload time the same separate consent as a mode change,
//    and after upload it starts disarmed. The field can.listenOnly in /api/status describes the controller mode
//    chosen by the firmware at start, not a measurement.
//
// Who calls what:
//   main loop : otaBootCheck, otaConfirmPoll, otaExecuteRollback, otaRestartDue, otaNoteCarBusy
//   web task  : handlers registered by otaRegisterRoutes, otaGetStatus
//   any       : otaBusy, otaPendingVerify, otaRunningState, otaScheduleRestart, otaRestartPending,
//               otaRestartReason, otaTryClaim, otaRelease, otaRunningElf
// State changed by the bootloader functions (which firmware starts, whether it is confirmed) is changed
// after start only by the main loop; the web task does it once, at the end of a successful upload. A collision of the two
// tasks is excluded by the "claim" (otaTryClaim): whoever wants to write to the other firmware slot or
// schedule a restart after a firmware change (upload, rollback) takes it first; the other one gets
// a refusal. The claim is released on every failure; after success it stays until the restart.
//
// Time: the restart deadline is stored as absolute time (the requester's millis() + delay)
// and compared (int32_t)(nowMs - deadline) >= 0, because the main loop's 'nowMs' is sometimes older than the moment
// of the request from the web task.
//
// Internal file of src/net; included only from files compiled with NET_ENABLED = 1.
// Implementation: src/net/ota.cpp. That file must also contain the definition
//     extern "C" bool verifyRollbackLater() { return true; }
// (it must be in src/, not in a library). Without it the Arduino core confirms new firmware already before
// setup() and rollback of faulty firmware would never work. In the build WITHOUT networking this definition
// is absent: such an image confirms itself before setup(), so there is no automatic rollback for it.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include <esp_http_server.h>

#include "fw_ident.h"

// ---------- constants ----------

// After this many ms of uninterrupted main loop operation (with working drivers and a working network)
// new firmware confirms itself.
const uint32_t OTA_CONFIRM_AFTER_MS = 20000;
// If the network is not working by this time since start, the new firmware is considered faulty and rolled back.
const uint32_t OTA_CONFIRM_NET_TIMEOUT_MS = 60000;
// Delay between the restart request and the restart: the HTTP response must have time to leave the device.
const uint32_t OTA_RESTART_DELAY_MS = 1500;
// Receive and write chunk. 4096 bytes = one flash erase page; after every chunk
// vTaskDelay(1), so that the other tasks run and the core 0 watchdog (5 s) does not fire.
const size_t OTA_CHUNK = 4096;
// A file shorter than this cannot be firmware (the header with the application description alone is 0x120 bytes,
// and the smallest Arduino program is hundreds of kilobytes); rejected before any write.
const size_t OTA_MIN_IMAGE = 65536;
// How many receive timeouts in a row (HTTP_RECV_TIMEOUT_S seconds each) end the upload with an error.
const uint8_t OTA_RECV_TIMEOUTS_MAX = 3;
// Longest time of the whole upload (from step C). After it: abort and close the connection. 3.3 MB at
// 100 KB/s is 33 s; the limit protects against a link that trickles data and would keep the device awake.
const uint32_t OTA_TOTAL_TIMEOUT_MS = 180000;
// How many times (every 1 s) to retry writing the confirmation when ESP-IDF returns an error.
const uint8_t OTA_CONFIRM_TRIES = 3;
// Longest discarding of the rest of the file after an error detected midway (so the browser gets a response).
const uint32_t OTA_DISCARD_TIMEOUT_MS = 15000;

// ---------- states ----------

// State of the RUNNING firmware per the bootloader (esp_ota_get_state_partition for esp_ota_get_running_partition).
enum class OtaImgState : uint8_t {
    NONE = 0,   // no entry or undefined state: firmware flashed over a USB cable (the usual case)
    VALID,      // uploaded over Wi-Fi and confirmed
    PENDING,    // uploaded over Wi-Fi, after the first start, not yet confirmed
};

enum class RestartReason : uint8_t {
    NONE = 0,
    USER,        // POST /api/restart
    OTA,         // after a successful firmware upload
    ROLLBACK,    // after a rollback to the previous firmware (on request or after a failed confirmation)
};

// Result of the last upload since program start (returns to NONE after a restart).
enum class OtaLastResult : uint8_t { NONE = 0, OK, FAILED };

// What is in the other firmware slot (the rollback target).
struct OtaSlotInfo {
    char label[8] = {0};        // slot name: "app0" or "app1"
    bool hasImage = false;      // the first byte is 0xE9 (ESP32 image header)
    bool scanned = false;       // the content has been scanned for the FwIdent marker
    bool identFound = false;    // the marker was found (the fields below are valid)
    bool bootable = false;      // the state per the bootloader allows a start (not INVALID and not ABORTED)
    char version[16] = {0};
    char build[24] = {0};
    uint8_t mode = 0;           // 0 = next to the box, 1 = decoder
    uint8_t net = 0;            // 1 = has the network panel
    uint8_t canTx = 0;          // 1 = the image can transmit on the car's CAN (byte from the marker; old images: 0)
    char elf[17] = {0};         // first 8 bytes of the ELF hash (hex) from the application description; "" = unknown
};

struct OtaStatus {
    OtaImgState running = OtaImgState::NONE;
    char runningElf[17] = {0};      // otaRunningElf()
    char runningLabel[8] = {0};     // slot of the running firmware: "app0" or "app1"
    uint32_t slotSize = 0;          // size of the slot for new firmware in bytes (upper limit of the file)
    uint32_t confirmInMs = 0;       // with PENDING: time left until the earliest confirmation; otherwise 0
    bool busy = false;              // upload in progress
    bool restartPending = false;
    RestartReason restartReason = RestartReason::NONE;
    OtaSlotInfo other;
    // Rollback possible: the running firmware awaits confirmation (then rollback = rejecting it),
    // or the other slot holds firmware of this device that may be started
    // (other.identFound && other.bootable && esp_ota_check_rollback_is_possible()).
    bool canRollback = false;
    OtaLastResult lastResult = OtaLastResult::NONE;
    char lastError[16] = {0};       // error word as in the HTTP response (e.g. "bad_image"); empty when none
    uint32_t lastBytes = 0;         // how many bytes of the last file were received
};

// ---------- program start (main loop, setup()) ----------

// Call once, from netBootCheck(). driversOk = the drivers came up AND netBegin() succeeded.
// Reads the state of the running firmware and remembers it; computes otaRunningElf() once.
// State PENDING and driversOk == false: prints "[ota] drivers or network did not come up, rolling back"
// via conPrintf, calls esp_ota_mark_app_invalid_rollback_and_reboot() and DOES NOT RETURN (if the IDF function
// returned an error because there is nothing to go back to: it prints that and stays with the current firmware,
// confirming it, so as not to restart in a loop).
// State PENDING, driversOk == true and the RTC memory holds the mode-change marker (written in step F
// of the upload, structure RTC_NOINIT_ATTR {magic, elf[8]}, elf matching otaRunningElf()): confirms
// right away (esp_ota_mark_app_valid_cancel_rollback), clears the marker and prints
// "[ota] mode change: confirmed at once, no automatic rollback". A flash write in setup()
// is acceptable: the car cannot have started a manoeuvre yet, and the outputs are off.
// State PENDING and driversOk == true without the marker: remembers the start time of the countdown to confirmation.
// The marker is cleared at every start, also when it does not match.
void otaBootCheck(bool driversOk);

// ---------- main loop ----------

// Call on every loop pass (from netConfirmHealthy), as proof that the loop is running.
// netHealthy = the network does its job: wifiMgrServing(), or the user switched the network off
//              (settingsNetEnabled() == false).
// netHeld    = network start-up is currently held back because the car needs the device
//              (NetLoopInputs::carBusy from the last netLoop). This time does NOT count towards the limit
//              OTA_CONFIRM_NET_TIMEOUT_MS: good firmware must not be rolled back just because
//              the driver parked for a long time right after the update.
// accOn      = the radio has ACC (NetLoopInputs::accOn). This time also does NOT count towards the limit
//              OTA_CONFIRM_NET_TIMEOUT_MS, and with accOn there is no automatic rollback (a rollback is a
//              restart, and a restart would remove ACC from the radio while driving); confirmation works normally.
// Outside state PENDING: immediate return. In state PENDING, and ONLY when netHeld == false (both
// branches write flash memory; with netHeld they wait for the next pass, without a deadline: until then
// the device only does not fall asleep):
//   - when OTA_CONFIRM_AFTER_MS has passed since otaBootCheck and netHealthy: esp_ota_mark_app_valid_cancel_rollback(),
//     state VALID, message "[ota] new program confirmed". IDF error: retry every 1 s, at most
//     OTA_CONFIRM_TRIES times; then the warning "[ota] confirmation failed" and otaPendingVerify()
//     returns false (the device may fall asleep; the bootloader rolls back the firmware at the next start);
//   - when the time since otaBootCheck minus the time with netHeld has exceeded OTA_CONFIRM_NET_TIMEOUT_MS
//     and still !netHealthy: esp_ota_mark_app_invalid_rollback() (the version WITHOUT restart)
//     and otaScheduleRestart(RestartReason::ROLLBACK, 100), so that main.cpp has time to turn off the outputs;
//     when IDF refuses (no firmware to go back to): confirms the current one and prints a warning.
// Writing the state to flash memory takes tens of ms (not measured) and happens once per update.
void otaConfirmPoll(uint32_t nowMs, bool netHealthy, bool netHeld, bool accOn);

// Execution of the NetReqKind::OTA_ROLLBACK request (from net.cpp, main loop). The web task has already checked
// the conditions and consents and taken the claim (otaTryClaim); here those that may have changed
// are checked again. When otaBusy() (should not happen with the claim): false right away.
//   state PENDING : esp_ota_mark_app_invalid_rollback() (rejecting the running firmware);
//   other state   : esp_ota_set_boot_partition(other slot) (the IDF function itself checks the checksum
//                   and SHA-256 of that image). When modeChange (the web task checked that the firmware
//                   in the other slot has a different operating mode, and had consent mode=1): writes the
//                   mode-change marker with the hash of that image, as during an upload, so the target confirms
//                   itself right away at start (without an automatic return to the previous mode).
// On success: otaScheduleRestart(RestartReason::ROLLBACK, OTA_RESTART_DELAY_MS) and true (the claim stays).
// Error: message "[ota] rollback failed: ..." via conPrintf, otaRelease(), false, no restart.
bool otaExecuteRollback(bool modeChange);

// true = the deadline of the scheduled restart has passed. On the first true prints the reason via conPrintf.
// net.cpp (netRestartDue) adds the condition "the car does not need the device". Then main.cpp: safe
// state of the outputs, ESP.restart().
bool otaRestartDue(uint32_t nowMs);

// Main loop (net.cpp, netLoop): carBusy from the current pass. When an upload is in progress and carBusy == true,
// sets a latch (volatile variable) that the web task checks after EVERY chunk; the latch is cleared by
// the web task in step C. Thanks to the latch the upload aborts even when further
// 0F6 / 0E1 frames are lost during the flash write and the state snapshot stops showing "car busy".
void otaNoteCarBusy(bool carBusy);

// ---------- any task (volatile variables, no locks, no waiting) ----------

bool otaBusy();                     // a file upload is in progress
bool otaPendingVerify();            // the running firmware awaits confirmation (and it has not been given up)
OtaImgState otaRunningState();      // state remembered at start, changed on confirmation

// Hash of the running firmware: the first 8 bytes of esp_app_get_description()->app_elf_sha256
// (esp_app_desc.h) as 16 UPPERCASE hex digits and a zero; computed once in otaBootCheck().
// NOT esp_app_get_elf_sha256(): in this build it returns at most 9 characters (CONFIG_APP_RETRIEVE_LEN_ELF_SHA 9).
// The firmware.bin file has the same hash in bytes 0xB0..0xB7 (the application description starts at byte 0x20,
// the field app_elf_sha256 is at 0x90 within it). Before otaBootCheck: empty text.
const char* otaRunningElf();

// Claim of the other firmware slot and of the right to schedule a restart after a firmware change.
// The same critical section (portMUX) as otaScheduleRestart. otaTryClaim: true = claimed; false =
// already claimed by someone or a restart is scheduled. otaRelease: release (no effect when not claimed).
bool otaTryClaim();
void otaRelease();

// Schedules a restart in 'delayMs' (deadline = the caller's millis() + delayMs). The first request
// wins: when a restart is already scheduled, it returns false and changes nothing. The variables are protected
// by a short critical section (portMUX), because two tasks (web and main loop) make requests.
bool otaScheduleRestart(RestartReason why, uint32_t delayMs);
bool otaRestartPending();
RestartReason otaRestartReason();

// ---------- web task ----------

// Registers (via httpRegister from http_server.h), in this order: POST /api/ota/rollback,
// GET /api/ota/status, POST /api/ota (longer addresses before the shorter one, reason at httpRegister).
// Called by httpServerStart(). false = one of the registrations failed.
bool otaRegisterRoutes(httpd_handle_t server);

// State for the GET /api/ota/status response. Web task ONLY.
// The first call after program start (and the first after every upload) scans the other slot
// for the FwIdent marker: reading in OTA_CHUNK portions into httpBuf() (destroys its content),
// with vTaskDelay(1) every 64 KB, until the marker is found or the end of the slot; at most approx. 3.3 MB,
// expected time below 2 s (not measured). The result is remembered. Scanning is skipped
// or aborted (other.scanned = false, next attempt at the next call) when the car needs the
// device or httpStopRequested().
void otaGetStatus(OtaStatus& out);

// ---------- course of POST /api/ota (mandatory order; error codes in docs/PANEL_API.md) ----------
//
// Request body = the raw firmware.bin file. URL parameters: force=1 (consent to an image without networking),
// mode=1 (consent to an image in a different operating mode than the running one).
//
//  A. Without reading the body:
//     1. httpRequireAuth(req, ret, HttpAccess::WRITE) (on error the connection is closed: the body
//        is larger than 64 KB);
//     2. Content-Length must be given (411 length_required), not smaller than OTA_MIN_IMAGE
//        (400 bad_image) and not larger than the free slot size (413 too_large, connection closed);
//     3. state PENDING: 409 ota_pending; scheduled restart or otaTryClaim() == false: 409 restart_pending;
//        no state snapshot: 503 not_ready; carNeedsDevice(): 409 car_busy. From a successful otaTryClaim()
//        EVERY error path calls otaRelease().
//     Responses from items 2-3 (except 413) are sent after reading and discarding the whole body
//     (httpDiscardBody with deadline OTA_DISCARD_TIMEOUT_MS), so that the browser gets a response
//     and not a broken connection.
//  B. First chunk (exactly OTA_CHUNK bytes, collected in httpBuf()), BEFORE any write:
//     byte 0 == 0xE9; bytes 12-13 == 09 00 (ESP32-S3 chip); bytes 0x20..0x23 == 32 54 CD AB
//     (application description; rejects among others the merged image of all partitions). Otherwise 400 bad_image.
//  C. Clearing the otaNoteCarBusy latch, esp_ota_get_next_update_partition(NULL) ->
//     esp_ota_begin(slot, OTA_WITH_SEQUENTIAL_WRITES, &h). From this moment otaBusy() == true, and EVERY
//     error and disconnect path calls esp_ota_abort(h) and otaRelease().
//     Message "[ota] start, <n> bytes" via conLogf (the main loop prints its own on USB when it
//     notices netOtaBusy()).
//  D. Loop: httpd_req_recv into httpBuf() (at most OTA_CHUNK at a time) -> IdentScanner::feed ->
//     esp_ota_write -> vTaskDelay(1). Receive timeout: retry, at most OTA_RECV_TIMEOUTS_MAX
//     times in a row; then, on any other receive error, after OTA_TOTAL_TIMEOUT_MS from step C
//     and on httpStopRequested(): abort, no response, return ESP_FAIL.
//     After EVERY chunk: the otaNoteCarBusy latch; when set: abort, discard the rest of the body,
//     409 car_busy. Write error: abort, discard the rest, 500 flash.
//  E. After the last byte: esp_ota_end(h) (IDF checks the checksum and SHA-256 of the image; error:
//     400 bad_image). Then the marker: missing or a different product (fwIdentAcceptable with FW_PRODUCT):
//     400 wrong_product, consent force does NOT change this; net == 0 without force=1: 409 no_net;
//     mode other than fwIdent().mode OR canTx other than fwIdent().canTx (fwIdentModeChange)
//     without mode=1: 409 mode_change.
//  F. esp_ota_set_boot_partition(slot) (error: 500 flash) -> when the mode or canTx of the new image is
//     different than in fwIdent(): mode-change marker in RTC memory (magic + bytes 0xB0..0xB7 of the file), see
//     otaBootCheck -> otaScheduleRestart(RestartReason::OTA, OTA_RESTART_DELAY_MS) -> response 200
//     with the version, mode and ELF hash of the new image (bytes 0xB0..0xB7 of the file, 16 UPPERCASE hex
//     digits, JsonWriter::hex) -> otaBusy() == false. The restart happens when the delay has passed
//     and the car does not need the device (net.h, netRestartDue).
//  After a rejection in item E the previous firmware in the other slot has already been overwritten (rollback
//  unavailable until the next successful upload); the running firmware remains untouched.
//  The result of every attempt goes to OtaStatus::lastResult / lastError / lastBytes and via conLogf to the panel.
