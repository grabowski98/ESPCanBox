// The panel's /api/... requests (except the firmware update, which is in ota.cpp).
// Format of every response: docs/PANEL_API.md; handler rules: http_server.h.
//
// All functions of this file run in the web server task. They take car data ONLY from the copies
// in diag_hub (state snapshot, rings), and order commands from the main loop via diagPostRequest.
// No function touches CAN, the UART, sound, outputs or logic objects, and none writes to USB.
// The working buffers are static: there is a single web task, so requests do not overlap.
#include "config.h"

#if NET_ENABLED

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "can_tx_arm.h"
#include "console.h"
#include "decoder.h"          // SettingCmdResult, settingCmdResultName (lib/core, no hardware)
#include "diag_hub.h"
#include "http_server.h"
#include "net.h"              // NET_APPLY_DELAY_MS
#include "ota.h"
#include "settings.h"
#include "settings_backup.h"
#include "ui_lang.h"
#include "version.h"
#include "web_auth.h"
#include "xp_keys_custom.h"

extern TaskHandle_t loopTaskHandle;   // main loop task (Arduino core, main.cpp)

// ---------- web task working buffers ----------

static StatusSnapshot s_snap;                           // copy of the state snapshot
static TraceRecord s_frames[HTTP_FRAME_BATCH];          // portion of frames from under the lock
static LineLog::Entry s_logEntries[HTTP_LOG_ENTRIES];   // portion of lines
static char s_logText[HTTP_LOG_TEXT];
static TableEntry s_table[FrameTable::MAX_IDS];
static NetRequest s_req;                                // request for the queue (approx. 200 B, not on the stack)
static xp::KeyCustomizer s_keyCheck;                    // trial parseMap: checking the table notation

// Device boot number (bootId from the state snapshot). It does not change until a restart, so it is enough to
// read it once; afterwards /api/hello and the responses with cursors do not need the hub lock.
static uint32_t s_bootId = 0;
static bool s_haveBoot = false;

// Copy of the state snapshot into s_snap. false: an error response has been sent ('ret' to return).
// Never published = 503 not_ready; lock taken = 503 busy.
static bool loadSnapshot(httpd_req_t* req, esp_err_t& ret) {
    if (diagGetStatus(s_snap)) {
        s_bootId = s_snap.bootId;
        s_haveBoot = true;
        return true;
    }
    ret = httpSendError(req, HttpStatus::UNAVAILABLE, s_haveBoot ? "busy" : "not_ready");
    return false;
}

// Boot number for the response. When still unknown, an attempt to read the state snapshot.
static bool loadBoot(httpd_req_t* req, esp_err_t& ret) {
    if (s_haveBoot) return true;
    return loadSnapshot(req, ret);
}

// Age of a timestamp: 'nowMs' taken AFTER copying the data, a negative result = 0 (net.h, "Time").
static uint32_t ageMs(uint32_t nowMs, uint32_t stampMs) {
    int32_t d = (int32_t)(nowMs - stampMs);
    return d < 0 ? 0u : (uint32_t)d;
}

// Text from a fixed-size field that need not be zero-terminated.
static void jsonField(JsonWriter& j, const char* s, size_t cap) { j.strN(s, strnlen(s, cap)); }

static void jsonIp(JsonWriter& j, const uint8_t ip[4]) {
    char b[16];
    snprintf(b, sizeof(b), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    j.str(b);
}

static const char* resetReasonName(uint8_t r) {
    static const char* const names[16] = {"unknown", "poweron", "ext", "sw", "panic", "int_wdt", "task_wdt", "wdt",
                                          "deepsleep", "brownout", "sdio", "usb", "jtag", "efuse", "pwr_glitch",
                                          "cpu_lockup"};
    return r < 16 ? names[r] : "unknown";
}

static const char* otaStateName(OtaImgState s) {
    switch (s) {
        case OtaImgState::VALID: return "valid";
        case OtaImgState::PENDING: return "pending";
        default: return "none";
    }
}

static const char* wifiPhaseName(WifiPhase p) {
    switch (p) {
        case WifiPhase::STARTING: return "starting";
        case WifiPhase::RUNNING: return "running";
        case WifiPhase::STOPPING: return "stopping";
        case WifiPhase::FAILED: return "failed";
        default: return "off";
    }
}

// Panel language stored in the device, only when this image has that dictionary. false = none (English).
static bool storedLang(char* out, size_t cap) {
    return settingsGetUiLang(out, cap) && uilang::inList(out, netUiLangs());
}

// Room for finishing a response with an array (closing the array and the object, plus a margin).
static const size_t JSON_TAIL_RESERVE = 64;

// ---------- GET /api/hello (no login) ----------

static esp_err_t handleHello(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!loadBoot(req, ret)) return ret;     // without the boot number the panel would misdetect a restart
    char b[160];
    JsonWriter j(b, sizeof(b));
    j.beginObject();
    j.key("product"); j.str(FW_PRODUCT);
    j.key("api"); j.unum(FW_API_VERSION);
    j.key("boot"); j.unum(s_bootId);
    // The lockout applies only to clients outside the device's own network (httpRequireAuth): a client from the
    // device's network gets 0, otherwise the panel would not let it log in, although the device would accept it.
    j.key("locked"); j.unum(httpPeerIsAp(req) ? 0u : httpLoginLockedS());
    // Panel language stored in the device (no login: the login screen opens in it). null = English.
    char lang[SET_UI_LANG_CAP];
    j.key("lang");
    if (storedLang(lang, sizeof(lang))) j.str(lang); else j.null();
    j.endObject();
    return httpSendJson(req, j);
}

// ---------- GET /api/status ----------

static esp_err_t handleStatus(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::READ)) return ret;
    if (!loadSnapshot(req, ret)) return ret;
    uint32_t logHead = 0, logOldest = 0, trHead = 0, trOldest = 0;
    if (!diagHeads(logHead, logOldest, trHead, trOldest)) {
        return httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
    }
    const StatusSnapshot& s = s_snap;
    const uint32_t now = millis();          // AFTER copying the data
    const FwIdent& id = fwIdent();

    JsonWriter j(httpBuf(), HTTP_BUF_SIZE);
    j.beginObject();

    j.key("sys");
    j.beginObject();
    j.key("version"); jsonField(j, id.version, sizeof(id.version));
    j.key("build"); jsonField(j, id.build, sizeof(id.build));
    j.key("elf"); j.str(otaRunningElf());
    j.key("mode"); j.str(fwModeName(id.mode));
    j.key("canTx"); j.boolean(id.canTx != 0);
    j.key("boot"); j.unum(s.bootId);
    j.key("uptimeMs"); j.unum(now);
    j.key("resetReason"); j.str(resetReasonName(s.resetReason));
    j.key("heapFree"); j.unum(ESP.getFreeHeap());
    j.key("heapMin"); j.unum(ESP.getMinFreeHeap());
    j.key("heapLargest"); j.unum(ESP.getMaxAllocHeap());
    j.key("loopBusyPct"); j.unum(s.loopBusyPct);
    j.key("cpu0Pct"); j.unum(s.cpu0Pct);
    j.key("loopMaxUs"); j.unum(s.loopMaxUs);
    j.key("loopsPerSec"); j.unum(s.loopsPerSec);
    // On this port the result of uxTaskGetStackHighWaterMark is in BYTES.
    j.key("stackLoopFree"); j.unum(loopTaskHandle != nullptr ? (uint32_t)uxTaskGetStackHighWaterMark(loopTaskHandle) : 0u);
    j.key("stackHttpFree"); j.unum((uint32_t)uxTaskGetStackHighWaterMark(nullptr));
    j.key("stackWifiFree"); j.unum(s.wifi.stackFree);
    j.key("tempC"); j.fixed(temperatureRead(), 1);   // NAN (sensor error) -> null
    j.key("otaState"); j.str(otaStateName(otaRunningState()));
    j.key("otaBusy"); j.boolean(otaBusy());
    j.key("restartPending"); j.boolean(otaRestartPending());
    j.key("hubDrops"); j.unum(diagDrops());
    // Whether THIS client is in the device's own network (by the connection addresses): only such a client may
    // arm CAN transmission and change the item mask from the panel.
    j.key("clientAp"); j.boolean(httpPeerIsAp(req));
    j.endObject();

    j.key("can");
    j.beginObject();
    j.key("ok"); j.boolean(s.canOk);
    j.key("rx"); j.unum(s.can.received);
    j.key("missed"); j.unum(s.can.missed);
    j.key("overrun"); j.unum(s.can.overrun);
    j.key("busErrors"); j.unum(s.can.busErrors);
    j.key("framesPerSec"); j.unum(s.framesPerSec);
    j.key("busAlive"); j.boolean(s.busAlive);
    // Controller mode chosen at start (a firmware setting, not a measurement): false only in the
    // transmit-capable build after an armed start (controller NORMAL, transmitter closed outside the frame window).
    j.key("listenOnly"); j.boolean(s.canListenOnly);
    // CAN transmit gate (since 0.9.0). In a build without transmit hardware "built" = false,
    // "state" = "none" and all zeros. "armNext": arming saved, takes effect after a restart.
    {
        const TxState txs = (TxState)s.canTxState;
        j.key("tx");
        j.beginObject();
        j.key("built"); j.boolean(txs != TxState::NOT_BUILT);
        j.key("armed"); j.boolean(txs != TxState::NOT_BUILT && txs != TxState::DISARMED);
        j.key("armNext"); j.boolean(s.canTxArmNext);
        j.key("state"); j.str(txStateName(txs));
        j.key("fault");
        if (s.canTxFault != 0) j.str(txFaultName((TxFault)s.canTxFault)); else j.null();
        j.key("sent"); j.unum(s.canTxSent);
        j.key("acked"); j.unum(s.canTxAcked);
        j.key("failed"); j.unum(s.canTxFailed);
        j.key("refused"); j.unum(s.canTxRefused);
        j.key("tec"); j.unum(s.canTxTec);
        j.key("rec"); j.unum(s.canTxRec);
        j.endObject();
    }
    j.endObject();

    j.key("car");
    j.beginObject();
    j.key("ignition"); j.boolean(s.ignition);
    j.key("ignitionRaw");
    if (s.ignitionFresh) j.unum(s.ignitionRaw); else j.null();      // a stale byte is not a reading
    j.key("acc"); j.boolean(s.acc);
    j.key("reverse"); j.boolean(s.reverse);
    j.key("lights"); j.boolean(s.lights);
    j.key("doors");
    if (s.doorsKnown) j.unum(s.doors); else j.null();               // no door frame: unknown
    j.key("batteryV");
    if (s.batteryV >= NET_VBAT_VALID_MIN_V) j.fixed(s.batteryV, 2); else j.null();
    j.key("outsideTempC");
    if (s.haveOutsideTemp) j.fixed(s.outsideTempC, 1); else j.null();
    // Vehicle settings (since 0.8.0, read only): last frame 260, state sent to the radio in frame 38,
    // last radio command 80. In the build without transmission "frame" is always null, "tookMs" always 0.
    j.key("settings");
    j.beginObject();
    j.key("raw260");
    if (s.set260Seen) j.hex(s.set260, s.set260Len <= 8 ? s.set260Len : 8); else j.null();
    j.key("ageMs");
    if (s.set260Seen) j.unum(ageMs(now, s.set260Ms)); else j.null();
    j.key("state38"); j.hex(s.set38, sizeof(s.set38));
    j.key("last");
    if (s.setHaveLast) {
        j.beginObject();
        j.key("id"); j.unum(s.setLastId);
        j.key("value"); j.unum(s.setLastValue);
        j.key("result"); j.str(settingCmdResultName((SettingCmdResult)s.setLastResult));
        j.key("tookMs"); j.unum(s.setLastTookMs);
        j.key("ageMs"); j.unum(ageMs(now, s.setLastMs));
        j.key("frame");
        if (s.setLastHaveFrame) j.hex(s.setLastFrame, sizeof(s.setLastFrame)); else j.null();
        j.endObject();
    } else {
        j.null();
    }
    j.key("accepted"); j.unum(s.setAccepted);
    j.key("notAccepted"); j.unum(s.setNotAccepted);
    j.key("refused"); j.unum(s.setRefused);
    j.key("mask"); j.unum(s.setMask);     // mask of enabled items (bit = item number)
    j.endObject();
    j.endObject();

    j.key("park");
    j.beginObject();
    j.key("fresh"); j.boolean(s.parkFresh);
    j.key("rearStatus"); j.unum(s.park.rearStatus);
    j.key("frontStatus"); j.unum(s.park.frontStatus);
    j.key("sound"); j.boolean(s.park.soundRequest);
    j.key("soundFront"); j.boolean(s.park.soundFront);
    j.key("interval"); j.unum(s.park.intervalRaw);
    j.key("rear");
    j.beginArray(); j.unum(s.park.rearLeft); j.unum(s.park.rearCenter); j.unum(s.park.rearRight); j.endArray();
    j.key("front");
    j.beginArray(); j.unum(s.park.frontLeft); j.unum(s.park.frontCenter); j.unum(s.park.frontRight); j.endArray();
    j.key("beeping"); j.boolean(s.beeping);
    j.key("continuous"); j.boolean(s.continuous);
    j.endObject();

    j.key("radio");
    j.beginObject();
    j.key("ok"); j.boolean(s.radioOk);
    j.key("txEnabled"); j.boolean(s.radioTxEnabled);
    j.key("heard"); j.boolean(s.radio.everHeard);
    j.key("ageMs");
    if (s.radio.everHeard) j.unum(ageMs(now, s.radio.lastFrameMs)); else j.null();
    j.key("on"); j.boolean(s.radio.radioOn);
    j.key("volume"); j.unum(s.radio.volume);
    j.key("muted"); j.boolean(s.radio.muted);
    j.key("source");
    j.beginArray();
    for (int i = 0; i < 8; i++) j.unum(s.radio.source[i]);
    j.endArray();
    j.key("time");
    if (s.radio.haveTime) {
        char t[24];
        snprintf(t, sizeof(t), "20%02u-%02u-%02u %02u:%02u", s.radio.year % 100u, s.radio.month % 100u,
                 s.radio.day % 100u, s.radio.hour % 100u, s.radio.minute % 100u);
        j.str(t);
    } else {
        j.null();
    }
    j.key("rxFrames"); j.unum(s.radioRx.frames);
    j.key("rxAcks"); j.unum(s.radioRx.acks);
    j.key("rxBadChecksum"); j.unum(s.radioRx.badChecksum);
    j.key("rxBadLength"); j.unum(s.radioRx.badLength);
    j.key("rxTimeouts"); j.unum(s.radioRx.timeouts);
    j.key("rxStray"); j.unum(s.radioRx.strayBytes);
    j.key("txFrames"); j.unum(s.radioTx.framesSent);
    j.key("txAcks"); j.unum(s.radioTx.acksSent);
    j.key("txSuppressed"); j.unum(s.radioTx.framesSuppressed);
    j.key("txQueue"); j.unum(s.txQueue);
    j.key("txQueueCap"); j.unum(s.txQueueCap);
    j.key("txDropped"); j.unum(s.txDropped);
    j.key("keysDropped"); j.unum(s.keysDropped);
    j.key("txGated"); j.unum(s.txGated);
    j.key("slotsFull"); j.unum(s.slotsFull);
    j.key("settingCount"); j.unum(s.radio.settingCount);
    j.key("lastSetting");
    j.beginArray(); j.unum(s.radio.lastSettingId); j.unum(s.radio.lastSettingValue); j.endArray();
    j.key("airCount"); j.unum(s.radio.airCount);
    j.key("lastAir");
    j.beginArray(); j.unum(s.radio.lastAirId); j.unum(s.radio.lastAirValue); j.endArray();
    j.key("unknownCount"); j.unum(s.radio.unknownCount);
    j.endObject();

    j.key("audio");
    j.beginObject();
    j.key("ok"); j.boolean(s.audioOk);
    j.key("volume"); j.unum(s.audioVolume);
    j.key("volumeMax"); j.unum((uint32_t)AUDIO_VOLUME_MAX);
    j.endObject();

    const WifiStatus& w = s.wifi;
    j.key("wifi");
    j.beginObject();
    j.key("on"); j.boolean(w.phase == WifiPhase::RUNNING);
    j.key("phase"); j.str(wifiPhaseName(w.phase));
    j.key("apSsid"); jsonField(j, w.apSsid, sizeof(w.apSsid));
    j.key("apIp"); jsonIp(j, w.apIp);
    j.key("apChannel"); j.unum(w.apChannel);
    j.key("apStations"); j.unum(w.apStations);
    j.key("staConfigured"); j.boolean(w.staConfigured);
    j.key("staSsid"); jsonField(j, w.staSsid, sizeof(w.staSsid));
    j.key("staConnected"); j.boolean(w.staConnected);
    j.key("staIp");
    if (w.staConnected) jsonIp(j, w.staIp); else j.null();
    j.key("staRssi");
    if (w.staConnected) j.num(w.staRssi); else j.null();
    j.key("staLastReason"); j.unum(w.staLastReason);
    j.key("hostname"); j.str(NET_HOSTNAME);
    j.key("maxStepMs"); j.unum(w.maxStepMs);
    j.key("startFails"); j.unum(w.startFails);
    j.key("gaveUp"); j.boolean(w.gaveUp);
    j.key("blocked"); j.boolean(w.blockedByFaults);
    j.key("lanAdmin"); j.boolean(settingsLanAdmin());
    j.endObject();

    j.key("log");
    j.beginObject(); j.key("head"); j.unum(logHead); j.key("oldest"); j.unum(logOldest); j.endObject();
    j.key("trace");
    j.beginObject(); j.key("head"); j.unum(trHead); j.key("oldest"); j.unum(trOldest); j.endObject();

    j.endObject();
    return httpSendJson(req, j);
}

// ---------- GET /api/log ----------

static esp_err_t handleLog(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::READ)) return ret;
    uint32_t cursor = 0;
    bool present = false;
    if (!httpQueryU32(req, "c", cursor, present) && present) {
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "c");
    }
    if (!present) cursor = 0;               // from the oldest line in memory
    if (!loadBoot(req, ret)) return ret;

    uint32_t dropped = 0;
    bool ok = false;
    size_t n = diagReadLog(cursor, s_logEntries, HTTP_LOG_ENTRIES, s_logText, sizeof(s_logText), dropped, ok);
    if (!ok) return httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
    if (!present) dropped = 0;              // without a cursor nothing was "lost" (PANEL_API 1.5)
    uint32_t logHead = 0, logOldest = 0, trHead = 0, trOldest = 0;
    if (!diagHeads(logHead, logOldest, trHead, trOldest)) {
        return httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
    }
    // Lines are consecutive: the first copied one has the number (cursor after the read) - n.
    const uint32_t firstSeq = cursor - (uint32_t)n;

    JsonWriter j(httpBuf(), HTTP_BUF_SIZE);
    j.beginObject();
    j.key("boot"); j.unum(s_bootId);
    // Fields next / head / dropped are appended at the end: 'next' depends on how many lines fit.
    j.key("lines");
    j.beginArray();
    size_t used = 0;
    for (size_t i = 0; i < n; i++) {
        const LineLog::Entry& e = s_logEntries[i];
        // Worst case of text encoding: 6 characters per byte (\u00XX). A line that might not
        // fit waits for the next request (next < head), instead of spoiling the whole response.
        if (HTTP_BUF_SIZE - j.length() < (size_t)e.len * 6 + 32 + JSON_TAIL_RESERVE) break;
        if ((size_t)e.offset + e.len > sizeof(s_logText)) break;   // should not happen
        j.beginArray();
        j.unum(e.ms);
        j.strN(s_logText + e.offset, e.len);
        j.endArray();
        used++;
    }
    j.endArray();
    j.key("next"); j.unum(firstSeq + (uint32_t)used);
    j.key("head"); j.unum(logHead);
    j.key("dropped"); j.unum(dropped);
    j.endObject();
    return httpSendJson(req, j);
}

// ---------- GET /api/frames ----------

// ID filter: 1..3 hex digits, value up to 7FF. false = invalid notation.
static bool parseIdFilter(const char* v, uint16_t& out) {
    size_t n = strlen(v);
    if (n == 0 || n > 3) return false;
    uint32_t acc = 0;
    for (size_t i = 0; i < n; i++) {
        char c = v[i];
        uint32_t d;
        if (c >= '0' && c <= '9') d = (uint32_t)(c - '0');
        else if (c >= 'A' && c <= 'F') d = (uint32_t)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') d = (uint32_t)(c - 'a' + 10);
        else return false;
        acc = acc * 16 + d;
    }
    if (acc > 0x7FF) return false;
    out = (uint16_t)acc;
    return true;
}

// One frame as [ms,"ID","DATA"].
static void jsonFrame(JsonWriter& j, const TraceRecord& r) {
    j.beginArray();
    j.unum(r.ms);
    j.hexId(r.id);
    j.hex(r.data, r.len <= 8 ? r.len : 8);
    j.endArray();
}

static esp_err_t handleFrames(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::READ)) return ret;

    uint32_t cursor = 0;
    bool cPresent = false;
    if (!httpQueryU32(req, "c", cursor, cPresent) && cPresent) {
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "c");
    }
    if (!cPresent) cursor = 0;

    uint16_t idFilter = FrameTrace::NO_FILTER;
    {
        char v[8];
        bool got = httpQueryValue(req, "id", v, sizeof(v));
        // httpQueryValue does not distinguish a missing parameter from an invalid notation. The mere presence is reported by
        // httpQueryU32 ('present'); its numeric result does not matter here (the ID is hexadecimal).
        uint32_t unused = 0;
        bool idPresent = false;
        (void)httpQueryU32(req, "id", unused, idPresent);
        if (idPresent && (!got || !parseIdFilter(v, idFilter))) {
            return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "id");
        }
    }

    uint32_t maxFrames = HTTP_FRAMES_DEFAULT;
    bool mPresent = false;
    uint32_t m = 0;
    if (httpQueryU32(req, "max", m, mPresent)) {
        if (m == 0) return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "max");
        maxFrames = m > HTTP_FRAMES_MAX ? HTTP_FRAMES_MAX : m;
    } else if (mPresent) {
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "max");
    }
    if (!loadBoot(req, ret)) return ret;

    JsonWriter j(httpBuf(), HTTP_BUF_SIZE);
    j.beginObject();
    j.key("boot"); j.unum(s_bootId);
    j.key("frames");
    j.beginArray();

    // Frames read in portions: the hub lock is held only while copying one portion,
    // the JSON text is assembled without the lock.
    uint32_t total = 0;
    uint32_t droppedSum = 0;
    bool first = true;
    while (total < maxFrames) {
        uint32_t want = maxFrames - total;
        if (want > HTTP_FRAME_BATCH) want = HTTP_FRAME_BATCH;
        uint32_t dropped = 0;
        bool ok = false;
        size_t n = diagReadFrames(cursor, s_frames, want, dropped, idFilter, ok);
        if (!ok) {
            if (first) return httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
            break;                              // what we already have is a valid response anyway
        }
        if (first && !cPresent) dropped = 0;   // no cursor: from the oldest, no losses
        droppedSum += dropped;
        first = false;
        for (size_t i = 0; i < n; i++) jsonFrame(j, s_frames[i]);
        total += (uint32_t)n;
        if (n < want) break;                    // everything up to the current end of the ring has been scanned
    }
    j.endArray();

    uint32_t logHead = 0, logOldest = 0, trHead = 0, trOldest = 0;
    if (!diagHeads(logHead, logOldest, trHead, trOldest)) trHead = cursor;
    j.key("next"); j.unum(cursor);
    j.key("head"); j.unum(trHead);
    j.key("dropped"); j.unum(droppedSum);
    j.endObject();
    return httpSendJson(req, j);
}

// ---------- GET /api/table ----------

static esp_err_t handleTable(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::READ)) return ret;
    uint32_t overflow = 0;
    bool ok = false;
    uint8_t n = diagReadTable(s_table, FrameTable::MAX_IDS, overflow, ok);
    if (!ok) return httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
    const uint32_t now = millis();          // AFTER copying the table: age never negative

    JsonWriter j(httpBuf(), HTTP_BUF_SIZE);
    j.beginObject();
    j.key("nowMs"); j.unum(now);
    j.key("overflow"); j.unum(overflow);
    j.key("ids");
    j.beginArray();
    for (uint8_t i = 0; i < n; i++) {
        const TableEntry& e = s_table[i];
        j.beginArray();
        j.hexId(e.id);
        j.unum(e.count);
        j.unum(e.periodMs);
        j.unum(ageMs(now, e.lastMs));
        j.hex(e.data, e.len <= 8 ? e.len : 8);
        j.endArray();
    }
    j.endArray();
    j.endObject();
    return httpSendJson(req, j);
}

// ---------- GET /api/keymap ----------

static esp_err_t handleKeymapGet(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::READ)) return ret;
    if (!loadSnapshot(req, ret)) return ret;
    const StatusSnapshot& s = s_snap;
    const uint32_t now = millis();

    JsonWriter j(httpBuf(), HTTP_BUF_SIZE);
    j.beginObject();
    j.key("max"); j.unum(xp::KeyCustomizer::MAP_SIZE);
    j.key("map");
    j.beginArray();
    uint8_t cnt = s.keyMapCount <= xp::KeyCustomizer::MAP_SIZE ? s.keyMapCount : xp::KeyCustomizer::MAP_SIZE;
    for (uint8_t i = 0; i < cnt; i++) {
        j.beginArray(); j.hexByte(s.keyMap[i].from); j.hexByte(s.keyMap[i].to); j.endArray();
    }
    j.endArray();
    j.key("defaults");
    j.beginArray();
    // Defaults of the car profile built in (const data; the same table the Decoder got in decoder.begin).
    for (uint8_t i = 0; i < CAR_PROFILE_SELECTED.keyRemapDefaultCount && i < xp::KeyCustomizer::MAP_SIZE; i++) {
        const vehicle::KeyRemapDefault& d = CAR_PROFILE_SELECTED.keyRemapDefaults[i];
        if (d.from == d.to) continue;
        j.beginArray(); j.hexByte(d.from); j.hexByte(d.to); j.endArray();
    }
    j.endArray();
    j.key("lastKey");
    if (s.haveLastKey) {
        j.beginObject();
        j.key("code"); j.hexByte(s.lastKeyCode);
        j.key("state"); j.unum(s.lastKeyState);
        j.key("ageMs"); j.unum(ageMs(now, s.lastKeyMs));
        j.endObject();
    } else {
        j.null();
    }
    j.endObject();
    return httpSendJson(req, j);
}

// ---------- GET /api/trace.log ----------

// Sends the collected text as a response chunk. false = send error (connection to be closed).
static bool flushChunk(httpd_req_t* req, size_t& fill) {
    if (fill == 0) return true;
    esp_err_t e = httpd_resp_send_chunk(req, httpBuf(), (ssize_t)fill);
    fill = 0;
    vTaskDelay(1);                          // yield the processor (core 0 idle task watchdog)
    return e == ESP_OK;
}

static esp_err_t handleTraceLog(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::READ)) return ret;
    bool mask = true;                       // VIN masking enabled by default
    {
        char v[4];
        if (httpQueryValue(req, "mask", v, sizeof(v)) && strcmp(v, "0") == 0) mask = false;
    }
    // End of file = number of the next frame at the moment of starting.
    uint32_t logHead = 0, logOldest = 0, endSeq = 0, trOldest = 0;
    if (!diagHeads(logHead, logOldest, endSeq, trOldest)) {
        return httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
    }

    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"" PROJECT_SLUG "_can.log\"");  // version.h -> project_identity.h
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");

    char* out = httpBuf();
    size_t fill = 0;
    const FwIdent& id = fwIdent();
    int w = snprintf(out, HTTP_BUF_SIZE, "# %.*s %.*s %s; time in seconds since the device started\n",
                     (int)strnlen(id.product, sizeof(id.product)), id.product,
                     (int)strnlen(id.version, sizeof(id.version)), id.version, fwModeName(id.mode));
    if (w > 0) fill = (size_t)w;

    // From this moment every error ends with the connection being dropped (the headers may already have gone out).
    uint32_t cursor = trOldest;
    // Longest line: "(4294967.295) can0 7FF#0011223344556677\n" = 41 characters; "# lost N frames\n" < 40.
    const size_t LINE_MAX_LEN = 48;
    while ((int32_t)(endSeq - cursor) > 0) {
        if (httpStopRequested()) return ESP_FAIL;
        uint32_t want = endSeq - cursor;
        if (want > HTTP_FRAME_BATCH) want = HTTP_FRAME_BATCH;
        uint32_t dropped = 0;
        bool ok = false;
        size_t n = diagReadFrames(cursor, s_frames, want, dropped, FrameTrace::NO_FILTER, ok);
        if (!ok) return ESP_FAIL;
        if (dropped > 0) {
            if (HTTP_BUF_SIZE - fill < LINE_MAX_LEN && !flushChunk(req, fill)) return ESP_FAIL;
            w = snprintf(out + fill, HTTP_BUF_SIZE - fill, "# lost %lu frames\n", (unsigned long)dropped);
            if (w > 0) fill += (size_t)w;
        }
        for (size_t i = 0; i < n; i++) {
            const TraceRecord& r = s_frames[i];
            if (HTTP_BUF_SIZE - fill < LINE_MAX_LEN && !flushChunk(req, fill)) return ESP_FAIL;
            if (mask && isVinFrameId(r.id)) {
                w = snprintf(out + fill, HTTP_BUF_SIZE - fill, "# masked\n");
            } else {
                char hex[17];
                uint8_t len = r.len <= 8 ? r.len : 8;
                static const char digits[] = "0123456789ABCDEF";
                for (uint8_t k = 0; k < len; k++) {
                    hex[2 * k] = digits[r.data[k] >> 4];
                    hex[2 * k + 1] = digits[r.data[k] & 0x0F];
                }
                hex[2 * len] = 0;
                w = snprintf(out + fill, HTTP_BUF_SIZE - fill, "(%lu.%03lu) can0 %03X#%s\n",
                             (unsigned long)(r.ms / 1000), (unsigned long)(r.ms % 1000), (unsigned)(r.id & 0x7FF), hex);
            }
            if (w > 0) fill += (size_t)w;
        }
        if (n < want) break;                // the ring has no more (should not happen before endSeq)
    }
    if (!flushChunk(req, fill)) return ESP_FAIL;
    // An empty chunk ends the response.
    return httpd_resp_send_chunk(req, nullptr, 0);
}

// ---------- POST /api/cmd ----------

// Whether the first word of the line is "wifi" (case-insensitive, after leading whitespace).
static bool startsWithWifi(const char* line) {
    while (*line == ' ') line++;
    const char* w = "wifi";
    for (int i = 0; i < 4; i++) {
        char c = line[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != w[i]) return false;
    }
    return line[4] == 0 || line[4] == ' ';
}

static esp_err_t sendQueued(httpd_req_t* req, const char* extraKey = nullptr, uint32_t extraVal = 0) {
    char b[64];
    JsonWriter j(b, sizeof(b));
    j.beginObject();
    j.key("queued"); j.boolean(true);
    if (extraKey != nullptr) {
        j.key(extraKey);
        j.unum(extraVal);
    }
    j.endObject();
    return httpSendJson(req, j);
}

static esp_err_t handleCmd(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::WRITE)) return ret;
    const char* body = nullptr;
    size_t len = 0;
    if (!httpReadForm(req, body, len, ret)) return ret;

    memset(&s_req, 0, sizeof(s_req));
    char* line = s_req.text;
    if (!webauth::formValue(body, len, "line", line, CON_LINE_MAX)) {
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "line");
    }
    size_t n = strlen(line);
    if (n == 0 || n > CON_LINE_MAX - 1) {
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "line");
    }
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)line[i];
        if (c < 32 || c > 126) return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "line");
    }
    // Wi-Fi commands contain passwords: USB only. The line reaches neither the queue nor the log.
    if (startsWithWifi(line)) {
        memset(&s_req, 0, sizeof(s_req));
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "usb_only");
    }
    // CAN transmission: arming ("cantx on|off") and the item mask ("vset on|off NN") from the panel only from
    // a client in the device's OWN network (address of the other side of the connection, not headers), never via
    // the home network, even with "wifi lan on" enabled. "cantx reset" and "set NN VV": USB only.
    // The main loop (console_cmds.cpp) checks the same again by the fromAp marker.
    const bool fromAp = httpPeerIsAp(req);
    const cantx::Parsed txc = cantx::parse(line);
    if (cantx::changesState(txc.cmd)) {
        if (!cantx::allowed(txc.cmd, cantx::Source::PANEL_AP)) {
            memset(&s_req, 0, sizeof(s_req));
            return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "usb_only");
        }
        if (!fromAp) {
            memset(&s_req, 0, sizeof(s_req));
            conLogf("[www] refused: CAN transmit command from outside the device's own network");
            return httpSendError(req, HttpStatus::FORBIDDEN, "ap_only");
        }
    }
    s_req.fromAp = fromAp;
    s_req.kind = NetReqKind::CONSOLE_LINE;
    if (!diagPostRequest(s_req)) return httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
    return sendQueued(req);
}

// ---------- POST /api/keymap ----------

static esp_err_t handleKeymapPost(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::WRITE)) return ret;
    const char* body = nullptr;
    size_t len = 0;
    if (!httpReadForm(req, body, len, ret)) return ret;

    memset(&s_req, 0, sizeof(s_req));
    char v[4];
    if (webauth::formValue(body, len, "reset", v, sizeof(v)) && strcmp(v, "1") == 0) {
        s_req.kind = NetReqKind::RESET_KEYMAP;
    } else {
        // An empty value ("map=") is valid: a table without remaps. The NET_REQ_TEXT_MAX buffer also holds
        // 16 pairs with a semicolon at the end (96 characters).
        if (!webauth::formValue(body, len, "map", s_req.text, sizeof(s_req.text))) {
            return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "map");
        }
        // Trial parseMap on our own instance: an invalid table does not reach the queue.
        if (!s_keyCheck.parseMap(s_req.text, strlen(s_req.text))) {
            return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "map");
        }
        s_req.kind = NetReqKind::SET_KEYMAP;
    }
    if (!diagPostRequest(s_req)) return httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
    return sendQueued(req);
}

// ---------- POST /api/lang ----------

// Panel language stored in the device. Body: lang=<code from the image's dictionaries> or lang= (forget).
static esp_err_t handleLang(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::WRITE)) return ret;
    const char* body = nullptr;
    size_t len = 0;
    if (!httpReadForm(req, body, len, ret)) return ret;
    memset(&s_req, 0, sizeof(s_req));
    if (!webauth::formValue(body, len, "lang", s_req.text, SET_UI_LANG_CAP) ||
        (s_req.text[0] != 0 && !uilang::inList(s_req.text, netUiLangs()))) {
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "lang");
    }
    s_req.kind = NetReqKind::SET_LANG;
    if (!diagPostRequest(s_req)) return httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
    return sendQueued(req);
}

// ---------- GET /api/settings (backup export) ----------

static esp_err_t handleSettingsGet(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::READ)) return ret;
    static backup::Values v;                     // web task only; ~110 bytes, not on the stack
    settingsExport(v);
    if ((v.present & backup::F_LANG) && !uilang::inList(v.lang, netUiLangs())) v.present &= (uint8_t)~backup::F_LANG;
    static char text[backup::TEXT_MAX];
    JsonWriter j(text, sizeof(text));
    backup::write(v, j);
    return httpSendJson(req, j);                 // checks j.ok() (overflow = 500, a firmware bug)
}

// ---------- POST /api/settings (backup import) ----------

// Body: json=<object of lib/core/settings_backup.h>. Everything is checked HERE, before the queue: an import
// with one bad field changes nothing. "net" and "lan" are ignored (USB only: they decide who can reach
// the device). The main loop writes the fields one by one (console_cmds.cpp).
static esp_err_t handleSettingsPost(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::WRITE)) return ret;
    const char* body = nullptr;
    size_t len = 0;
    if (!httpReadForm(req, body, len, ret)) return ret;
    static char text[256];                       // longer than any export (TEXT_MAX), white space allowed
    static backup::Values v;
    if (!webauth::formValue(body, len, "json", text, sizeof(text))) {
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "json");
    }
    const backup::ParseError pe = backup::parse(text, strlen(text), v);
    if (pe != backup::ParseError::OK) return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", backup::errorName(pe));
    const uint8_t given = (uint8_t)(v.present & (uint8_t)~v.isNull);
    if ((given & backup::F_LANG) && !uilang::inList(v.lang, netUiLangs())) {
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "lang");
    }
    // The device stores a volume only from AUDIO_VOLUME_MIN_SAVED (a stored "vol 0" would mute the sensors).
    if ((given & backup::F_VOL) && v.vol < AUDIO_VOLUME_MIN_SAVED) {
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "vol");
    }
    if ((given & backup::F_KEYS) && !s_keyCheck.parseMap(v.keys, strlen(v.keys))) {
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "keys");
    }
    memset(&s_req, 0, sizeof(s_req));
    s_req.kind = NetReqKind::IMPORT_SETTINGS;
    s_req.setPresent = (uint8_t)(v.present & (backup::F_LANG | backup::F_VOL | backup::F_KEYS));
    s_req.setNull = (uint8_t)(v.isNull & s_req.setPresent);
    s_req.setVol = v.vol;
    memcpy(s_req.text, v.keys, sizeof(v.keys));                  // 96 <= NET_REQ_TEXT_MAX
    memcpy(s_req.text2, v.lang, sizeof(v.lang));                 // 9 <= NET_REQ_TEXT2_MAX
    if (!diagPostRequest(s_req)) return httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
    return sendQueued(req);
}

// ---------- POST /api/wifi ----------

static esp_err_t handleWifi(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::WRITE)) return ret;
    const char* body = nullptr;
    size_t len = 0;
    if (!httpReadForm(req, body, len, ret)) return ret;

    memset(&s_req, 0, sizeof(s_req));
    char* ssid = s_req.text;
    char* pass = s_req.text2;
    if (!webauth::formValue(body, len, "ssid", ssid, SET_SSID_MAX + 1)) {
        return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "ssid");
    }
    if (ssid[0] == 0) {
        s_req.kind = NetReqKind::CLEAR_STA;     // empty name = forget the home network (password ignored)
    } else {
        for (const char* p = ssid; *p; p++) {
            if ((unsigned char)*p < 0x20) return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "ssid");
        }
        // Password mandatory, 8..63 printable characters or 64 hex digits: an open network cannot be saved.
        bool okPass = webauth::formValue(body, len, "pass", pass, NET_REQ_TEXT2_MAX);
        if (!okPass || !settingsStaAcceptable(ssid, pass)) {
            memset(&s_req, 0, sizeof(s_req));
            return httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "pass");
        }
        s_req.kind = NetReqKind::SET_STA;
    }
    bool posted = diagPostRequest(s_req);
    memset(&s_req, 0, sizeof(s_req));            // the password does not stay in the web task buffer
    if (!posted) return httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
    return sendQueued(req, "applyInMs", NET_APPLY_DELAY_MS);
}

// ---------- POST /api/password ----------

static esp_err_t handlePassword(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::WRITE)) return ret;
    const char* body = nullptr;
    size_t len = 0;
    if (!httpReadForm(req, body, len, ret)) return ret;

    memset(&s_req, 0, sizeof(s_req));
    char oldPass[webauth::PASSWORD_MAX + 1];
    char cur[webauth::PASSWORD_MAX + 1];
    esp_err_t result;
    if (!webauth::formValue(body, len, "old", oldPass, sizeof(oldPass))) {
        result = httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "old");
    } else if (!webauth::formValue(body, len, "new", s_req.text, sizeof(s_req.text))) {
        result = httpSendError(req, HttpStatus::BAD_REQUEST, "bad_request", "new");
    } else if (!settingsGetDevicePassword(cur, sizeof(cur))) {
        result = httpSendError(req, HttpStatus::UNAVAILABLE, "not_ready");
    } else if (!webauth::constantTimeEquals(oldPass, cur)) {
        // Does not count towards the login lockout: the request has already passed the login with the same password.
        result = httpSendError(req, HttpStatus::FORBIDDEN, "wrong_password");
    } else if (!webauth::passwordAcceptable(s_req.text)) {
        result = httpSendError(req, HttpStatus::BAD_REQUEST, "weak_password");
    } else {
        s_req.kind = NetReqKind::SET_PASSWORD;
        if (diagPostRequest(s_req)) {
            conLogf("[www] device password change requested from the panel");
            result = sendQueued(req, "applyInMs", NET_APPLY_DELAY_MS);
        } else {
            result = httpSendError(req, HttpStatus::UNAVAILABLE, "busy");
        }
    }
    // Passwords do not stay in the web task memory (zeroed through a volatile pointer).
    volatile char* p1 = oldPass;
    volatile char* p2 = cur;
    for (size_t i = 0; i < sizeof(oldPass); i++) { p1[i] = 0; p2[i] = 0; }
    memset(&s_req, 0, sizeof(s_req));
    return result;
}

// ---------- POST /api/restart ----------

static esp_err_t handleRestart(httpd_req_t* req) {
    esp_err_t ret = ESP_OK;
    if (!httpRequireAuth(req, ret, HttpAccess::WRITE)) return ret;
    // The body, if any, is skipped. When it is too long or too slow, the connection is
    // closed after the response.
    bool keepConn = true;
    if (req->content_len > 0) {
        keepConn = req->content_len <= HTTP_FORM_DRAIN_MAX &&
                   httpDiscardBody(req, req->content_len, millis() + HTTP_DRAIN_DEADLINE_MS);
    }
    esp_err_t e;
    if (!loadSnapshot(req, ret)) {
        e = ret;
    } else if (otaRunningState() == OtaImgState::PENDING) {
        // A restart before confirmation would roll back the new firmware; /api/ota/rollback is for that.
        e = httpSendError(req, HttpStatus::CONFLICT, "ota_pending");
    } else if (carNeedsDevice(s_snap)) {
        e = httpSendError(req, HttpStatus::CONFLICT, "car_busy");
    } else if (!otaTryClaim()) {
        // Restart already scheduled, or a rollback / upload to the other slot is in progress.
        e = httpSendError(req, HttpStatus::CONFLICT, "restart_pending");
    } else if (!otaScheduleRestart(RestartReason::USER, OTA_RESTART_DELAY_MS)) {
        otaRelease();
        e = httpSendError(req, HttpStatus::CONFLICT, "restart_pending");
    } else {
        // The claim stays until the restart: during this time no upload or rollback will start.
        conLogf("[www] restart requested from the panel");
        e = sendQueued(req, "restartInMs", OTA_RESTART_DELAY_MS);
    }
    return (keepConn && e == ESP_OK) ? ESP_OK : ESP_FAIL;
}

// ---------- registration ----------

bool apiRegisterRoutes(httpd_handle_t server) {
    return httpRegister(server, "/api/hello", HTTP_GET, handleHello) &&
           httpRegister(server, "/api/status", HTTP_GET, handleStatus) &&
           httpRegister(server, "/api/log", HTTP_GET, handleLog) &&
           httpRegister(server, "/api/frames", HTTP_GET, handleFrames) &&
           httpRegister(server, "/api/table", HTTP_GET, handleTable) &&
           httpRegister(server, "/api/keymap", HTTP_GET, handleKeymapGet) &&
           httpRegister(server, "/api/settings", HTTP_GET, handleSettingsGet) &&
           httpRegister(server, "/api/trace.log", HTTP_GET, handleTraceLog) &&
           httpRegister(server, "/api/cmd", HTTP_POST, handleCmd) &&
           httpRegister(server, "/api/keymap", HTTP_POST, handleKeymapPost) &&
           httpRegister(server, "/api/lang", HTTP_POST, handleLang) &&
           httpRegister(server, "/api/settings", HTTP_POST, handleSettingsPost) &&
           httpRegister(server, "/api/wifi", HTTP_POST, handleWifi) &&
           httpRegister(server, "/api/password", HTTP_POST, handlePassword) &&
           httpRegister(server, "/api/restart", HTTP_POST, handleRestart);
}

#endif  // NET_ENABLED
