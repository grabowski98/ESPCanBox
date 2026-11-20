// Firmware update over Wi-Fi: receiving the file (POST /api/ota), state (GET /api/ota/status),
// rollback (POST /api/ota/rollback), confirmation of new firmware after start and restart scheduling.
// Rules, order of steps A-F and who calls what: ota.h. Response format: docs/PANEL_API.md, chapter 6.
//
// Flash memory and CAN: writing and erasing flash memory disables the processor cache, and the CAN driver
// interrupt is not in IRAM, so during that time CAN frames may be lost in the controller itself. Therefore
// every flash write in this file is performed only when the car does not need the device,
// in OTA_CHUNK-byte portions, with vTaskDelay(1) after each portion.
#include "config.h"

#if NET_ENABLED

#include <string.h>

#include <esp_app_desc.h>
#include <esp_err.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "console.h"
#include "diag_hub.h"
#include "http_server.h"
#include "ota.h"
#include "version.h"
#include "settings.h"

extern volatile bool g_canTxWindow;   // main.cpp: CAN transmitter window open

// Before every switch to another firmware we clear the stored CAN transmit arming. Older firmware
// (before 1.0.0) does not know this key and would not clear it: after returning to the same image
// transmission would be armed without a new "cantx on" (review 18.11.2026).
static void disarmBeforeSwitch() {
    char tag[SET_CANTX_TAG_MAX];
    if (settingsGetCanTxTag(tag, sizeof(tag))) settingsClearCanTxTag();
}

// Without this definition the Arduino core confirms new firmware already before setup() and the automatic
// rollback of faulty firmware would never work. It must be in src/ (not in a library).
extern "C" bool verifyRollbackLater() { return true; }

// ---------- mode-change marker (RTC memory, survives a software restart) ----------

// Written in step F when uploading an image in a different operating mode; checked and cleared in otaBootCheck.
// RTC_NOINIT: at power-on the content is random, hence the magic and the comparison of the ELF hash.
struct OtaModeMark {
    uint32_t magic;
    uint8_t elf[8];
};
static const uint32_t OTA_MODE_MARK_MAGIC = 0x4D4F4445;   // "MODE"
static RTC_NOINIT_ATTR OtaModeMark s_modeMark;

// ---------- state of the running firmware (changed by the main loop) ----------

static volatile OtaImgState s_state = OtaImgState::NONE;
static char s_elf[17] = {0};
static uint32_t s_bootMs = 0;            // start of the countdown to confirmation
static uint32_t s_lastPollMs = 0;
static uint32_t s_heldMs = 0;            // time with netHeld (does not count towards OTA_CONFIRM_NET_TIMEOUT_MS)
static uint8_t s_confirmTries = 0;
static uint32_t s_nextTryMs = 0;
static volatile bool s_confirmDone = false;   // end of PENDING state handling (given up or rejected)

// ---------- upload (changed by the web task) ----------

static volatile bool s_busy = false;
static volatile bool s_carLatch = false;      // set by the main loop (otaNoteCarBusy), cleared by step C
static volatile OtaLastResult s_lastResult = OtaLastResult::NONE;
static char s_lastError[16] = {0};
static volatile uint32_t s_lastBytes = 0;

// ---------- claim and restart (two tasks: short critical section) ----------

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_claimed = false;
static bool s_restartPending = false;
static RestartReason s_restartReason = RestartReason::NONE;
static uint32_t s_restartAt = 0;
static bool s_restartAnnounced = false;  // main loop only

// ---------- the other firmware slot (web task only) ----------

static OtaSlotInfo s_other;
static bool s_otherScanned = false;
// From step C of any upload until the restart: the other slot has been (even partially) overwritten.
// Even when it now holds a complete image (rejected in step E) or the beginning of a new image with
// the marker, it is no longer "the previous firmware", so rollback is unavailable until the restart
// (docs/PANEL_API.md, chapter 6). After the restart the variable returns to false together with the whole program.
static bool s_otherOverwritten = false;
static StatusSnapshot s_snap;            // copy of the state snapshot for the handlers in this file

// ---------- helpers ----------

static void toHexUpper(const uint8_t* b, size_t n, char* out) {
    static const char digits[] = "0123456789ABCDEF";
    for (size_t i = 0; i < n; i++) {
        out[2 * i] = digits[b[i] >> 4];
        out[2 * i + 1] = digits[b[i] & 0x0F];
    }
    out[2 * n] = 0;
}

static void copyLabel(char* dst, size_t cap, const esp_partition_t* p) {
    dst[0] = 0;
    if (p == nullptr) return;
    strncpy(dst, p->label, cap - 1);
    dst[cap - 1] = 0;
}

static void setLast(OtaLastResult r, const char* err, uint32_t bytes) {
    s_lastResult = r;
    strncpy(s_lastError, err != nullptr ? err : "", sizeof(s_lastError) - 1);
    s_lastError[sizeof(s_lastError) - 1] = 0;
    s_lastBytes = bytes;
}

static const char* reasonName(RestartReason r) {
    switch (r) {
        case RestartReason::USER: return "user";
        case RestartReason::OTA: return "ota";
        case RestartReason::ROLLBACK: return "rollback";
        default: return "none";
    }
}

// The car needs the device according to the last state snapshot. 'have' = a snapshot was available.
static bool snapCarBusy(bool& have) {
    have = diagGetStatus(s_snap);
    return have && carNeedsDevice(s_snap);
}

// ---------- program start (setup) ----------

void otaBootCheck(bool driversOk) {
    const esp_app_desc_t* d = esp_app_get_description();
    toHexUpper(d->app_elf_sha256, 8, s_elf);

    // Mode-change marker: valid only with a correct magic and the hash of THIS firmware; always cleared.
    bool modeMark = s_modeMark.magic == OTA_MODE_MARK_MAGIC && memcmp(s_modeMark.elf, d->app_elf_sha256, 8) == 0;
    s_modeMark.magic = 0;

    OtaImgState st = OtaImgState::NONE;
    const esp_partition_t* run = esp_ota_get_running_partition();
    esp_ota_img_states_t ist;
    if (run != nullptr && esp_ota_get_state_partition(run, &ist) == ESP_OK) {
        if (ist == ESP_OTA_IMG_PENDING_VERIFY) st = OtaImgState::PENDING;
        else if (ist == ESP_OTA_IMG_VALID) st = OtaImgState::VALID;
    }
    s_state = st;
    if (st != OtaImgState::PENDING) return;

    if (!driversOk) {
        conPrintf("[ota] drivers or network did not come up, rolling back\n");
        disarmBeforeSwitch();
        esp_err_t e = esp_ota_mark_app_invalid_rollback_and_reboot();   // does not return on success
        // Nothing to go back to: we stay with the current firmware and confirm it, so that
        // the bootloader does not restart in a loop.
        conPrintf("[ota] rollback impossible (%s), keeping the current program\n", esp_err_to_name(e));
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) s_state = OtaImgState::VALID;
        s_confirmDone = true;
        return;
    }
    if (modeMark) {
        // Firmware uploaded with consent to a mode change: confirmation right away, so that a power loss while
        // re-plugging wires does not bring back the previous mode. The outputs are still off.
        esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();
        if (e == ESP_OK) {
            s_state = OtaImgState::VALID;
            conPrintf("[ota] mode change: confirmed at once, no automatic rollback\n");
            return;
        }
        conPrintf("[ota] mode change: confirmation failed (%s), normal countdown\n", esp_err_to_name(e));
    }
    s_bootMs = millis();
    s_lastPollMs = s_bootMs;
    s_heldMs = 0;
}

// ---------- main loop ----------

void otaConfirmPoll(uint32_t nowMs, bool netHealthy, bool netHeld, bool accOn) {
    if (s_state != OtaImgState::PENDING || s_confirmDone) return;

    uint32_t dt = nowMs - s_lastPollMs;
    s_lastPollMs = nowMs;
    if (netHeld) {                           // the car needs the device: no flash write
        s_heldMs += dt;
        return;
    }
    // The radio has ACC: the confirmation may be written, but the time does not count towards the automatic rollback
    // (rollback = restart, and a restart with ACC would remove power from the radio while driving).
    if (accOn) s_heldMs += dt;
    int32_t since = (int32_t)(nowMs - s_bootMs);
    if (since < 0) since = 0;

    if ((uint32_t)since >= OTA_CONFIRM_AFTER_MS && netHealthy) {
        if (s_confirmTries > 0 && (int32_t)(nowMs - s_nextTryMs) < 0) return;
        esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();     // flash write, once per update
        if (e == ESP_OK) {
            s_state = OtaImgState::VALID;
            conPrintf("[ota] new program confirmed\n");
            return;
        }
        s_confirmTries++;
        s_nextTryMs = nowMs + 1000;
        if (s_confirmTries >= OTA_CONFIRM_TRIES) {
            // Giving up: the device may fall asleep, and the bootloader rolls back the firmware at the next start.
            s_confirmDone = true;
            conPrintf("[ota] confirmation failed: %s\n", esp_err_to_name(e));
        }
        return;
    }

    uint32_t effective = (uint32_t)since > s_heldMs ? (uint32_t)since - s_heldMs : 0;
    if (!netHealthy && !accOn && effective > OTA_CONFIRM_NET_TIMEOUT_MS) {
        // The network did not come up: without it the new firmware could no longer be fixed without a cable.
        if (!otaTryClaim()) return;          // another rollback is already in progress or a restart is scheduled
        disarmBeforeSwitch();
        esp_err_t e = esp_ota_mark_app_invalid_rollback();
        if (e == ESP_OK) {
            s_confirmDone = true;
            conPrintf("[ota] the network did not come up, rolling back\n");
            otaScheduleRestart(RestartReason::ROLLBACK, 100);   // main.cpp has time to turn off the outputs
            return;
        }
        conPrintf("[ota] rollback impossible (%s), confirming the current program\n", esp_err_to_name(e));
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) s_state = OtaImgState::VALID;
        s_confirmDone = true;
        otaRelease();
    }
}

bool otaExecuteRollback(bool modeChange) {
    if (s_busy) return false;                // the claim then belongs to the upload: do not release
    esp_err_t e;
    disarmBeforeSwitch();
    if (s_state == OtaImgState::PENDING) {
        e = esp_ota_mark_app_invalid_rollback();   // rejecting the running firmware
    } else {
        const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);
        // Hash of the target image for the mode-change marker (read before switching).
        esp_app_desc_t desc;
        bool haveDesc = other != nullptr && esp_ota_get_partition_description(other, &desc) == ESP_OK;
        // IDF itself checks the checksum and SHA-256 of that image before switching.
        e = other != nullptr ? esp_ota_set_boot_partition(other) : ESP_ERR_NOT_FOUND;
        if (e == ESP_OK && modeChange && haveDesc) {
            // With rollback enabled the target starts as PENDING_VERIFY. Without the marker a power loss
            // while re-plugging wires (or no network for 60 s) would bring back the previous mode,
            // e.g. a decoder transmitting to the radio in parallel with the RP5 box. Marker = confirmation right away.
            s_modeMark.magic = OTA_MODE_MARK_MAGIC;
            memcpy(s_modeMark.elf, desc.app_elf_sha256, sizeof(s_modeMark.elf));
        }
    }
    if (e != ESP_OK) {
        conPrintf("[ota] rollback failed: %s\n", esp_err_to_name(e));
        otaRelease();
        return false;
    }
    if (s_state == OtaImgState::PENDING) s_confirmDone = true;
    conPrintf("[ota] rolling back to the previous program, restart\n");
    otaScheduleRestart(RestartReason::ROLLBACK, OTA_RESTART_DELAY_MS);
    return true;
}

bool otaRestartDue(uint32_t nowMs) {
    portENTER_CRITICAL(&s_mux);
    bool pending = s_restartPending;
    uint32_t at = s_restartAt;
    RestartReason why = s_restartReason;
    portEXIT_CRITICAL(&s_mux);
    if (!pending) return false;
    if ((int32_t)(nowMs - at) < 0) return false;
    if (!s_restartAnnounced) {
        s_restartAnnounced = true;
        conPrintf("[ota] restart (%s)\n", reasonName(why));
    }
    return true;
}

void otaNoteCarBusy(bool carBusy) {
    if (s_busy && carBusy) s_carLatch = true;
}

// ---------- any task ----------

bool otaBusy() { return s_busy; }
bool otaPendingVerify() { return s_state == OtaImgState::PENDING && !s_confirmDone; }
OtaImgState otaRunningState() { return s_state; }
const char* otaRunningElf() { return s_elf; }

bool otaTryClaim() {
    portENTER_CRITICAL(&s_mux);
    bool ok = !s_claimed && !s_restartPending;
    if (ok) s_claimed = true;
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

void otaRelease() {
    portENTER_CRITICAL(&s_mux);
    s_claimed = false;
    portEXIT_CRITICAL(&s_mux);
}

bool otaScheduleRestart(RestartReason why, uint32_t delayMs) {
    uint32_t at = millis() + delayMs;        // absolute deadline (comparison via int32_t)
    portENTER_CRITICAL(&s_mux);
    bool ok = !s_restartPending;
    if (ok) {
        s_restartPending = true;
        s_restartReason = why;
        s_restartAt = at;
    }
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

bool otaRestartPending() {
    portENTER_CRITICAL(&s_mux);
    bool p = s_restartPending;
    portEXIT_CRITICAL(&s_mux);
    return p;
}

RestartReason otaRestartReason() {
    portENTER_CRITICAL(&s_mux);
    RestartReason r = s_restartReason;
    portEXIT_CRITICAL(&s_mux);
    return r;
}

// ---------- state (web task) ----------

// Scans the other firmware slot for the FwIdent marker. Flash read in portions
// of OTA_CHUNK into httpBuf(); the marker usually lies in the constants area near the start of the image, so
// the scan usually ends after a few hundred KB. Aborted when the car needs the device
// (checked every 64 KB) or when the server is being stopped.
static void scanOther() {
    const esp_partition_t* p = esp_ota_get_next_update_partition(nullptr);
    OtaSlotInfo info;
    copyLabel(info.label, sizeof(info.label), p);
    if (p == nullptr) {
        s_other = info;
        s_otherScanned = true;
        return;
    }
    uint8_t* buf = (uint8_t*)httpBuf();
    if (esp_partition_read(p, 0, buf, 1) == ESP_OK) info.hasImage = buf[0] == 0xE9;
    esp_ota_img_states_t ist;
    if (esp_ota_get_state_partition(p, &ist) == ESP_OK) {
        info.bootable = info.hasImage && ist != ESP_OTA_IMG_INVALID && ist != ESP_OTA_IMG_ABORTED;
    } else {
        info.bootable = info.hasImage;       // no entry in otadata: the image itself decides about the start
    }
    if (!info.hasImage || s_otherOverwritten) {
        // Empty or overwritten by an upload since start: nothing to search for and nothing to go back to.
        if (s_otherOverwritten) info.bootable = false;
        info.scanned = true;
        s_other = info;
        s_otherScanned = true;
        return;
    }
    bool have = false;
    if (snapCarBusy(have) || !have) {        // without a state snapshot it is unknown whether the car is busy
        s_other = info;                      // scanned = false: next attempt at the next request
        return;
    }
    IdentScanner sc(fwIdent().magic);
    for (uint32_t off = 0; off < p->size; off += OTA_CHUNK) {
        if (httpStopRequested()) {
            s_other = info;
            return;
        }
        if (off > 0 && (off % 65536u) == 0) {
            vTaskDelay(1);                   // idle task watchdog of core 0
            if (snapCarBusy(have) || !have) {
                s_other = info;
                return;
            }
        }
        size_t n = p->size - off < OTA_CHUNK ? p->size - off : OTA_CHUNK;
        if (esp_partition_read(p, off, buf, n) != ESP_OK) {
            s_other = info;
            return;
        }
        sc.feed(buf, n);
        if (sc.found()) break;
    }
    info.scanned = true;
    // ELF hash from the application description (as for the running firmware): distinguishes images with the same version
    // and the same build time. The description is read from flash (approx. 256 B), without verifying the whole image.
    esp_app_desc_t desc;
    if (esp_ota_get_partition_description(p, &desc) == ESP_OK) toHexUpper(desc.app_elf_sha256, 8, info.elf);
    if (sc.found()) {
        const FwIdent& id = sc.ident();
        info.identFound = fwIdentAcceptable(id, FW_PRODUCT, false);
        if (info.identFound) {
            // Texts from the image are untrusted: copied with a length limit, always zero-terminated.
            size_t nv = strnlen(id.version, sizeof(id.version));
            if (nv > sizeof(info.version) - 1) nv = sizeof(info.version) - 1;
            memcpy(info.version, id.version, nv);
            info.version[nv] = 0;
            size_t nb = strnlen(id.build, sizeof(id.build));
            if (nb > sizeof(info.build) - 1) nb = sizeof(info.build) - 1;
            memcpy(info.build, id.build, nb);
            info.build[nb] = 0;
            info.mode = id.mode;
            info.net = id.net;
            info.canTx = id.canTx;
        }
    }
    s_other = info;
    s_otherScanned = true;
}

void otaGetStatus(OtaStatus& out) {
    out = OtaStatus();
    out.running = s_state;
    memcpy(out.runningElf, s_elf, sizeof(out.runningElf));
    copyLabel(out.runningLabel, sizeof(out.runningLabel), esp_ota_get_running_partition());
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    out.slotSize = next != nullptr ? next->size : 0;
    if (s_state == OtaImgState::PENDING && !s_confirmDone) {
        int32_t since = (int32_t)(millis() - s_bootMs);
        if (since < 0) since = 0;
        out.confirmInMs = (uint32_t)since < OTA_CONFIRM_AFTER_MS ? OTA_CONFIRM_AFTER_MS - (uint32_t)since : 0;
    }
    out.busy = s_busy;
    out.restartPending = otaRestartPending();
    out.restartReason = otaRestartReason();
    if (!s_otherScanned) scanOther();
    out.other = s_other;
    out.canRollback = (s_state == OtaImgState::PENDING) ||
                      (s_other.identFound && s_other.bootable && esp_ota_check_rollback_is_possible());
    out.lastResult = s_lastResult;
    memcpy(out.lastError, s_lastError, sizeof(out.lastError));
    out.lastError[sizeof(out.lastError) - 1] = 0;
    out.lastBytes = s_lastBytes;
}

// ---------- GET /api/ota/status ----------

static esp_err_t handleStatus(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::READ)) return ret;
    OtaStatus st;
    otaGetStatus(st);                        // destroys httpBuf(): JSON is assembled only afterwards
    bool have = false;
    bool carBusy = snapCarBusy(have);
    const FwIdent& id = fwIdent();

    JsonWriter j(httpBuf(), HTTP_BUF_SIZE);
    j.beginObject();
    j.key("state");
    j.str(st.running == OtaImgState::VALID ? "valid" : st.running == OtaImgState::PENDING ? "pending" : "none");
    j.key("slotSize"); j.unum(st.slotSize);
    j.key("confirmInMs"); j.unum(st.confirmInMs);
    j.key("busy"); j.boolean(st.busy);
    j.key("carBusy"); j.boolean(carBusy);
    j.key("restartPending"); j.boolean(st.restartPending);
    j.key("restartReason"); j.str(reasonName(st.restartReason));
    j.key("canRollback"); j.boolean(st.canRollback);

    j.key("running");
    j.beginObject();
    j.key("slot"); j.str(st.runningLabel);
    j.key("version"); j.strN(id.version, strnlen(id.version, sizeof(id.version)));
    j.key("build"); j.strN(id.build, strnlen(id.build, sizeof(id.build)));
    j.key("mode"); j.str(fwModeName(id.mode));
    j.key("canTx"); j.boolean(id.canTx != 0);
    j.key("net"); j.boolean(true);
    j.key("elf"); j.str(st.runningElf);
    j.endObject();

    const OtaSlotInfo& o = st.other;
    const bool known = o.scanned && o.identFound;
    j.key("other");
    j.beginObject();
    j.key("slot"); j.str(o.label);
    j.key("image"); j.boolean(o.hasImage);
    j.key("scanned"); j.boolean(o.scanned);
    j.key("known"); j.boolean(known);   // the marker of this project was found
    j.key("bootable"); j.boolean(o.bootable);
    j.key("version"); if (known) j.str(o.version); else j.null();
    j.key("build"); if (known) j.str(o.build); else j.null();
    j.key("mode"); if (known) j.str(fwModeName(o.mode)); else j.null();
    j.key("canTx"); if (known) j.boolean(o.canTx != 0); else j.null();
    j.key("net"); if (known) j.boolean(o.net == 1); else j.null();
    j.key("elf"); if (o.scanned && o.elf[0] != 0) j.str(o.elf); else j.null();
    j.endObject();

    j.key("last");
    j.beginObject();
    j.key("result");
    j.str(st.lastResult == OtaLastResult::OK ? "ok" : st.lastResult == OtaLastResult::FAILED ? "error" : "none");
    j.key("error"); j.str(st.lastError);
    j.key("bytes"); j.unum(st.lastBytes);
    j.endObject();

    j.endObject();
    return httpSendJson(req, j);
}

// ---------- POST /api/ota/rollback ----------

// Consent from a URL parameter: exactly "1".
static bool consent(httpd_req_t* req, const char* key) {
    uint32_t v = 0;
    bool present = false;
    return httpQueryU32(req, key, v, present) && v == 1;
}

static esp_err_t handleRollback(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::WRITE)) return ret;
    bool keepConn = true;
    if (req->content_len > 0) {              // no body; any body is skipped
        keepConn = req->content_len <= HTTP_FORM_DRAIN_MAX &&
                   httpDiscardBody(req, req->content_len, millis() + HTTP_DRAIN_DEADLINE_MS);
    }
    const bool force = consent(req, "force");
    const bool modeOk = consent(req, "mode");

    esp_err_t e;
    bool have = false;
    bool carBusy = snapCarBusy(have);
    if (!have) {
        e = httpSendError(req, HttpStatus::UNAVAILABLE, "not_ready");
    } else if (carBusy) {
        e = httpSendError(req, HttpStatus::CONFLICT, "car_busy");
    } else if (otaRestartPending()) {
        e = httpSendError(req, HttpStatus::CONFLICT, "restart_pending");
    } else {
        OtaStatus st;
        otaGetStatus(st);
        const OtaSlotInfo& o = st.other;
        const bool pending = st.running == OtaImgState::PENDING;
        if (!st.canRollback) {
            e = httpSendError(req, HttpStatus::CONFLICT, "no_rollback");
        } else if (o.hasImage && !o.scanned) {
            // Unknown what is in the other slot (scan aborted): try again shortly.
            e = httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
        } else if (!pending && o.identFound && o.net == 0 && !force) {
            e = httpSendError(req, HttpStatus::CONFLICT, "no_net");
        } else if (o.identFound && fwIdentModeChange(fwIdent(), o.mode, o.canTx) && !modeOk) {
            // Change of operating mode or of the ability to transmit on CAN: separate consent (mode=1).
            e = httpSendError(req, HttpStatus::CONFLICT, "mode_change");
        } else if (o.identFound && fwIdentModeChange(fwIdent(), o.mode, o.canTx) && !httpPeerIsAp(req)) {
            // The same rule as "cantx on": a change of the ability to transmit only from the device's own network,
            // even after "wifi lan on" (review 19.11.2026).
            e = httpSendError(req, HttpStatus::FORBIDDEN, "ap_only");
        } else if (!otaTryClaim()) {
            e = httpSendError(req, HttpStatus::CONFLICT, "restart_pending");
        } else {
            // The request is approx. 200 B: static, not on the web task stack (http_server.h, rule 3).
            static NetRequest rq;
            rq = NetRequest();
            rq.kind = NetReqKind::OTA_ROLLBACK;
            // Mode change (consent mode=1 checked above): the main loop will write the mode marker.
            if (!pending && o.identFound && fwIdentModeChange(fwIdent(), o.mode, o.canTx)) rq.text[0] = 'M';
            if (!diagPostRequest(rq)) {
                otaRelease();
                e = httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
            } else {
                // The claim stays: otaExecuteRollback releases it on failure, otherwise the restart does.
                conLogf("[ota] rollback requested from the panel");
                char b[64];
                JsonWriter j(b, sizeof(b));
                j.beginObject();
                j.key("queued"); j.boolean(true);
                j.key("restartInMs"); j.unum(OTA_RESTART_DELAY_MS);
                j.endObject();
                e = httpSendJson(req, j);
            }
        }
    }
    return (keepConn && e == ESP_OK) ? ESP_OK : ESP_FAIL;
}

// ---------- POST /api/ota ----------

// Refusal with reading and discarding the rest of the body ('remaining' bytes), so that the browser gets
// a response and not a broken connection. Result: ESP_FAIL when the rest could not be discarded.
static esp_err_t otaReject(httpd_req_t* req, size_t remaining, HttpStatus status, const char* code,
                           const char* detail, uint32_t bytes) {
    setLast(OtaLastResult::FAILED, code, bytes);
    conLogf("[ota] rejected: %s%s%s", code, detail != nullptr ? " " : "", detail != nullptr ? detail : "");
    bool drained = remaining == 0 || httpDiscardBody(req, remaining, millis() + OTA_DISCARD_TIMEOUT_MS);
    if (!drained || httpStopRequested()) return ESP_FAIL;
    esp_err_t e = httpSendError(req, status, code, detail);
    return e == ESP_OK ? ESP_OK : ESP_FAIL;
}

// Abort after step C without a response (broken connection, timeout, server stop).
static esp_err_t otaAbortSilent(esp_ota_handle_t h, const char* why, uint32_t bytes) {
    esp_ota_abort(h);
    s_busy = false;
    otaRelease();
    setLast(OtaLastResult::FAILED, "aborted", bytes);
    conLogf("[ota] aborted: %s, received %lu bytes", why, (unsigned long)bytes);
    return ESP_FAIL;
}

// Receiving a portion of exactly 'want' bytes into 'dst'. Receive timeout: retry, at most
// OTA_RECV_TIMEOUTS_MAX times in a row. false = error, disconnect, total deadline or server stop.
static bool recvExact(httpd_req_t* req, uint8_t* dst, size_t want, uint32_t startMs) {
    size_t got = 0;
    uint8_t timeouts = 0;
    while (got < want) {
        if (httpStopRequested()) return false;
        if ((uint32_t)(millis() - startMs) >= OTA_TOTAL_TIMEOUT_MS) return false;
        int r = httpd_req_recv(req, (char*)dst + got, want - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            if (++timeouts >= OTA_RECV_TIMEOUTS_MAX) return false;
            continue;
        }
        if (r <= 0) return false;
        timeouts = 0;
        got += (size_t)r;
    }
    return true;
}

static esp_err_t handleUpload(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    // A1. Login (body > 64 KB: on refusal the connection is closed).
    if (!httpRequireAuth(req, ret, HttpAccess::WRITE)) return ret;
    const uint32_t startMs = millis();
    const bool force = consent(req, "force");
    const bool modeOk = consent(req, "mode");

    // A2. Length.
    const size_t total = req->content_len;
    if (total == 0) {
        setLast(OtaLastResult::FAILED, "length_required", 0);
        return httpSendError(req, HttpStatus::LENGTH_REQUIRED, "length_required");
    }
    const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
    if (part == nullptr || total > part->size) {
        setLast(OtaLastResult::FAILED, "too_large", 0);
        httpSendError(req, HttpStatus::TOO_LARGE, "too_large");
        return ESP_FAIL;                     // body not read: close the connection
    }
    if (total < OTA_MIN_IMAGE) return otaReject(req, total, HttpStatus::BAD_REQUEST, "bad_image", "short", 0);

    // A3. Device and car state.
    if (s_state == OtaImgState::PENDING) {
        return otaReject(req, total, HttpStatus::CONFLICT, "ota_pending", nullptr, 0);
    }
    if (otaRestartPending() || !otaTryClaim()) {
        return otaReject(req, total, HttpStatus::CONFLICT, "restart_pending", nullptr, 0);
    }
    // From this moment every error path releases the claim.
    bool have = false;
    bool carBusy = snapCarBusy(have);
    if (!have) {
        otaRelease();
        return otaReject(req, total, HttpStatus::UNAVAILABLE, "not_ready", nullptr, 0);
    }
    if (carBusy) {
        otaRelease();
        return otaReject(req, total, HttpStatus::CONFLICT, "car_busy", nullptr, 0);
    }

    // B. First portion, before any write: is this firmware for the ESP32-S3.
    uint8_t* buf = (uint8_t*)httpBuf();
    if (!recvExact(req, buf, OTA_CHUNK, startMs)) {
        otaRelease();
        setLast(OtaLastResult::FAILED, "aborted", 0);
        return ESP_FAIL;
    }
    const bool headerOk = buf[0] == 0xE9 &&                       // ESP image header
                          buf[12] == 0x09 && buf[13] == 0x00 &&   // chip_id = ESP32-S3
                          buf[0x20] == 0x32 && buf[0x21] == 0x54 && buf[0x22] == 0xCD && buf[0x23] == 0xAB;   // application description
    if (!headerOk) {
        otaRelease();
        return otaReject(req, total - OTA_CHUNK, HttpStatus::BAD_REQUEST, "bad_image", "header", OTA_CHUNK);
    }
    uint8_t newElf[8];
    memcpy(newElf, buf + 0xB0, sizeof(newElf));   // field app_elf_sha256 of the application description (0x20 + 0x90)

    // C. Start of the write. From now on the main loop sees otaBusy(): mode tolerant of lost CAN frames.
    s_carLatch = false;
    s_busy = true;
    // The main loop blocks the CAN transmit gate in its next pass (approx. 1 ms). We wait for that,
    // and then for the window to close, should one be open right now (at most 20 ms): only then the flash write.
    vTaskDelay(pdMS_TO_TICKS(5));
    for (int i = 0; i < 40 && g_canTxWindow; i++) vTaskDelay(pdMS_TO_TICKS(1));
    s_otherScanned = false;                  // the other slot is about to be overwritten
    s_otherOverwritten = true;
    esp_ota_handle_t h = 0;
    esp_err_t e = esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &h);
    if (e != ESP_OK) {
        s_busy = false;
        otaRelease();
        return otaReject(req, total - OTA_CHUNK, HttpStatus::INTERNAL, "flash", esp_err_to_name(e), OTA_CHUNK);
    }
    conLogf("[ota] start, %lu bytes", (unsigned long)total);
    const uint32_t writeStartMs = millis();   // OTA_TOTAL_TIMEOUT_MS counted from step C

    IdentScanner sc(fwIdent().magic);
    size_t got = 0;
    size_t chunk = OTA_CHUNK;                // the first portion is already in the buffer
    // D. Loop: (receive ->) marker search -> write -> yield the processor -> car latch.
    while (true) {
        sc.feed(buf, chunk);
        e = esp_ota_write(h, buf, chunk);
        got += chunk;
        vTaskDelay(1);                       // other tasks and the core 0 watchdog
        if (e != ESP_OK) {
            esp_ota_abort(h);
            s_busy = false;
            otaRelease();
            return otaReject(req, total - got, HttpStatus::INTERNAL, "flash", esp_err_to_name(e), got);
        }
        if (s_carLatch) {
            // Reverse or sensors: the write is aborted immediately, the rest of the file is only read.
            esp_ota_abort(h);
            s_busy = false;
            otaRelease();
            return otaReject(req, total - got, HttpStatus::CONFLICT, "car_busy", nullptr, got);
        }
        if (got >= total) break;
        chunk = total - got < OTA_CHUNK ? total - got : OTA_CHUNK;
        if (!recvExact(req, buf, chunk, writeStartMs)) {
            return otaAbortSilent(h, httpStopRequested() ? "server stop" : "receive", got);
        }
    }

    // E. Image check by IDF (checksum, SHA-256), then the device marker.
    e = esp_ota_end(h);                      // the handle is released regardless of the result
    if (e != ESP_OK) {
        s_busy = false;
        otaRelease();
        return otaReject(req, 0, HttpStatus::BAD_REQUEST, "bad_image", "verify", got);
    }
    if (!sc.found() || !fwIdentAcceptable(sc.ident(), FW_PRODUCT, false)) {
        s_busy = false;
        otaRelease();
        return otaReject(req, 0, HttpStatus::BAD_REQUEST, "wrong_product", nullptr, got);
    }
    const FwIdent& nid = sc.ident();
    if (nid.net == 0 && !force) {
        s_busy = false;
        otaRelease();
        return otaReject(req, 0, HttpStatus::CONFLICT, "no_net", nullptr, got);
    }
    // Change of operating mode or of the ability to transmit on CAN (canTx): both require consent mode=1.
    const bool modeChange = fwIdentModeChange(fwIdent(), nid.mode, nid.canTx);
    if (modeChange && !modeOk) {
        s_busy = false;
        otaRelease();
        return otaReject(req, 0, HttpStatus::CONFLICT, "mode_change", nullptr, got);
    }
    // A change of operating mode or of canTx only from the device's own network, like "cantx on" (even after
    // "wifi lan on"; review 19.11.2026). An image that lies in its marker is not stopped by this: SECURITY.md.
    if (modeChange && !httpPeerIsAp(req)) {
        s_busy = false;
        otaRelease();
        return otaReject(req, 0, HttpStatus::FORBIDDEN, "ap_only", nullptr, got);
    }
    // Reverse or sensors appeared during step E: step F writes flash too (otadata, NVS), so it is skipped
    // (review: the latch was checked only inside the write loop D).
    if (s_carLatch) {                        // esp_ota_end above already released the handle
        s_busy = false;
        otaRelease();
        return otaReject(req, 0, HttpStatus::CONFLICT, "car_busy", nullptr, got);
    }

    // F. Switching the bootloader to the new slot.
    disarmBeforeSwitch();
    e = esp_ota_set_boot_partition(part);
    if (e != ESP_OK) {
        s_busy = false;
        otaRelease();
        return otaReject(req, 0, HttpStatus::INTERNAL, "flash", esp_err_to_name(e), got);
    }
    if (modeChange) {
        s_modeMark.magic = OTA_MODE_MARK_MAGIC;
        memcpy(s_modeMark.elf, newElf, sizeof(newElf));
    }
    otaScheduleRestart(RestartReason::OTA, OTA_RESTART_DELAY_MS);   // the claim stays until the restart
    setLast(OtaLastResult::OK, "", (uint32_t)got);

    char elfHex[17];
    toHexUpper(newElf, sizeof(newElf), elfHex);
    conLogf("[ota] accepted %lu bytes, version %.*s, restart in %lu ms", (unsigned long)got,
            (int)strnlen(nid.version, sizeof(nid.version)), nid.version, (unsigned long)OTA_RESTART_DELAY_MS);

    JsonWriter j(httpBuf(), HTTP_BUF_SIZE);
    j.beginObject();
    j.key("ok"); j.boolean(true);
    j.key("bytes"); j.unum((uint32_t)got);
    // Texts from the marker in the file: untrusted, length limited by the field size.
    j.key("version"); j.strN(nid.version, strnlen(nid.version, sizeof(nid.version)));
    j.key("build"); j.strN(nid.build, strnlen(nid.build, sizeof(nid.build)));
    j.key("mode"); j.str(fwModeName(nid.mode));
    j.key("canTx"); j.boolean(nid.canTx != 0);
    j.key("net"); j.boolean(nid.net == 1);
    j.key("elf"); j.str(elfHex);
    j.key("restartInMs"); j.unum(OTA_RESTART_DELAY_MS);
    j.endObject();
    esp_err_t r = httpSendJson(req, j);
    s_busy = false;
    return r;
}

// ---------- registration ----------

bool otaRegisterRoutes(httpd_handle_t server) {
    // Longer addresses before the shorter "/api/ota" (reason: http_server.h, httpRegister).
    return httpRegister(server, "/api/ota/rollback", HTTP_POST, handleRollback) &&
           httpRegister(server, "/api/ota/status", HTTP_GET, handleStatus) &&
           httpRegister(server, "/api/ota", HTTP_POST, handleUpload);
}

#endif  // NET_ENABLED
