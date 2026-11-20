// Diagnostic hub: line and frame rings, ID table, state snapshot, request queue.
// Description and rules: diag_hub.h. Everything static, no heap.
#include "config.h"

#if NET_ENABLED

#include "diag_hub.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// ---------- data under the lock ----------
static LineLog s_log;                 // 8 KB
static FrameTrace s_trace;            // 2048 x 16 B = 32 KB
static FrameTable s_table;            // 128 x 24 B = approx. 3 KB
static StatusSnapshot s_status;
static bool s_haveStatus = false;

// Lock: FreeRTOS mutex with priority inheritance. Static descriptor, no heap.
// In this configuration only the task that took it may release it (CHECK_MUTEX_GIVEN_BY_OWNER).
static StaticSemaphore_t s_lockBuf;
static SemaphoreHandle_t s_lock = nullptr;

// Request queue panel -> main loop: FreeRTOS's own protection, the hub lock is not taken.
static StaticQueue_t s_queueBuf;
static uint8_t s_queueStorage[NET_REQ_QUEUE_LEN * sizeof(NetRequest)];
static QueueHandle_t s_queue = nullptr;

// The main loop task (the one that called diagBegin in setup()). By it diagLogLine recognises
// how long it may wait for the lock.
static TaskHandle_t s_loopTask = nullptr;

static volatile bool s_ready = false;

// ---------- pending-frame buffer: main loop ONLY (diagOnCanFrame, diagPublishStatus) ----------
struct PendingFrame {
    CanFrame f;
    uint32_t ms;
};
static PendingFrame s_pending[DIAG_PENDING_FRAMES];
static uint8_t s_pendingCount = 0;

// ---------- counters and client activity (without the hub lock) ----------
static portMUX_TYPE s_dropMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t s_drops = 0;
// The time is written by the web task and read by the main loop. A 32-bit write is atomic on this
// processor; time first, then the flag, so the flag never points to an empty time.
static volatile uint32_t s_clientMs = 0;
static volatile bool s_clientSeen = false;

static void countDrop() {
    portENTER_CRITICAL(&s_dropMux);   // a few instructions; counted by different tasks on two cores
    s_drops = s_drops + 1;
    portEXIT_CRITICAL(&s_dropMux);
}

static bool lockFor(uint32_t ms) { return xSemaphoreTake(s_lock, pdMS_TO_TICKS(ms)) == pdTRUE; }
static void unlock() { xSemaphoreGive(s_lock); }

// Under the lock: copies the pending frames in reception order (each with its own timestamp).
static void flushPendingLocked() {
    for (uint8_t i = 0; i < s_pendingCount; i++) {
        s_trace.push(s_pending[i].f, s_pending[i].ms);
        s_table.onFrame(s_pending[i].f, s_pending[i].ms);
    }
    s_pendingCount = 0;
}

// ---------- start ----------

bool diagBegin() {
    if (s_ready) return true;
    s_lock = xSemaphoreCreateMutexStatic(&s_lockBuf);
    if (s_lock == nullptr) return false;
    s_queue = xQueueCreateStatic(NET_REQ_QUEUE_LEN, sizeof(NetRequest), s_queueStorage, &s_queueBuf);
    if (s_queue == nullptr) return false;
    s_loopTask = xTaskGetCurrentTaskHandle();
    s_ready = true;
    return true;
}

bool diagReady() { return s_ready; }

// ---------- writing: main loop ----------

void diagOnCanFrame(const CanFrame& f, uint32_t ms) {
    if (!s_ready) return;
    // Timeout 0: the main loop never waits for the web task (priority: CAN reception and the radio).
    if (xSemaphoreTake(s_lock, 0) == pdTRUE) {
        flushPendingLocked();
        s_trace.push(f, ms);
        s_table.onFrame(f, ms);
        unlock();
        return;
    }
    // Lock taken: the frame waits in the private buffer; the next successful lock take copies it.
    if (s_pendingCount < DIAG_PENDING_FRAMES) {
        s_pending[s_pendingCount].f = f;
        s_pending[s_pendingCount].ms = ms;
        s_pendingCount++;
    } else {
        countDrop();   // the frame is lost only for the preview; the car logic received it normally
    }
}

void diagPublishStatus(const StatusSnapshot& s) {
    if (!s_ready) return;
    if (!lockFor(DIAG_LOCK_WAIT_MAIN_MS)) {
        countDrop();   // next snapshot in STATUS_PERIOD_MS
        return;
    }
    flushPendingLocked();
    s_status = s;      // copy of approx. 400 bytes
    s_haveStatus = true;
    unlock();
}

// ---------- writing: any task ----------

void diagLogLine(const char* text, size_t len, uint32_t ms) {
    if (!s_ready || text == nullptr) return;
    uint32_t wait = (xTaskGetCurrentTaskHandle() == s_loopTask) ? DIAG_LOCK_WAIT_MAIN_MS : DIAG_LOCK_WAIT_HTTP_MS;
    if (!lockFor(wait)) {
        countDrop();
        return;
    }
    s_log.append(text, len, ms);   // a line > 200 characters is truncated in LineLog
    unlock();
}

// ---------- reading: web task ----------

bool diagGetStatus(StatusSnapshot& out) {
    if (!s_ready) return false;
    if (!lockFor(DIAG_LOCK_WAIT_HTTP_MS)) return false;
    bool have = s_haveStatus;
    if (have) out = s_status;
    unlock();
    return have;
}

size_t diagReadLog(uint32_t& cursor, LineLog::Entry* entries, size_t maxEntries, char* text, size_t textCap,
                   uint32_t& dropped, bool& ok) {
    ok = false;
    if (!s_ready) return 0;
    if (!lockFor(DIAG_LOCK_WAIT_HTTP_MS)) return 0;   // cursor and dropped unchanged
    size_t n = s_log.read(cursor, entries, maxEntries, text, textCap, dropped);
    unlock();
    ok = true;
    return n;
}

size_t diagReadFrames(uint32_t& cursor, TraceRecord* out, size_t max, uint32_t& dropped, uint16_t idFilter,
                      bool& ok) {
    ok = false;
    if (!s_ready) return 0;
    if (!lockFor(DIAG_LOCK_WAIT_HTTP_MS)) return 0;
    size_t n = s_trace.read(cursor, out, max, dropped, idFilter);
    unlock();
    ok = true;
    return n;
}

uint8_t diagReadTable(TableEntry* out, uint8_t max, uint32_t& overflow, bool& ok) {
    ok = false;
    if (!s_ready || out == nullptr) return 0;
    if (!lockFor(DIAG_LOCK_WAIT_HTTP_MS)) return 0;
    uint8_t n = s_table.size();
    if (n > max) n = max;
    for (uint8_t i = 0; i < n; i++) out[i] = s_table.at(i);   // the table is already sorted by ID
    overflow = s_table.overflow();
    unlock();
    ok = true;
    return n;
}

bool diagHeads(uint32_t& logHead, uint32_t& logOldest, uint32_t& traceHead, uint32_t& traceOldest) {
    if (!s_ready) return false;
    if (!lockFor(DIAG_LOCK_WAIT_HTTP_MS)) return false;
    logHead = s_log.head();
    logOldest = s_log.oldest();
    traceHead = s_trace.head();
    traceOldest = s_trace.oldest();
    unlock();
    return true;
}

// ---------- request queue ----------

bool diagPostRequest(const NetRequest& r) {
    if (!s_ready) return false;
    return xQueueSend(s_queue, &r, 0) == pdTRUE;   // no waiting for space: full = 503 "busy"
}

bool diagTakeRequest(NetRequest& out) {
    if (!s_ready) return false;
    return xQueueReceive(s_queue, &out, 0) == pdTRUE;
}

// Peek at the oldest request without removing it (main loop, does not wait). Needed in net.cpp to
// hold back flash-writing requests while the car needs the device (net.h, netTakeRequest).
// NOTE: the function is not in diag_hub.h (the contract header); net.cpp declares it locally.
// To be added to diag_hub.h at the next contract change.
bool diagPeekRequest(NetRequest& out) {
    if (!s_ready) return false;
    return xQueuePeek(s_queue, &out, 0) == pdTRUE;
}

// ---------- client activity ----------

void diagTouchClient(uint32_t nowMs) {
    s_clientMs = nowMs;
    s_clientSeen = true;
}

bool diagClientActivity(uint32_t& lastMs) {
    bool seen = s_clientSeen;
    lastMs = s_clientMs;
    return seen;
}

// ---------- counters ----------

uint32_t diagDrops() { return s_drops; }

#endif  // NET_ENABLED
