// Console: the only path through which the program writes to USB (rules: console.h).
//
// Without network (NET_ENABLED = 0) conPrintf / conPrintln send to USB exactly the bytes that
// Serial.printf / Serial.println would send: the text is formatted by the same vsnprintf and handed over
// in a single Serial.write. Only one difference: text longer than CON_PRINTF_MAX - 1 characters is truncated
// (Serial.printf allocated heap memory in that case). Current lines are shorter.
#include "console.h"

#include <Arduino.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"

#if NET_ENABLED
#include "esp_log.h"          // esp_log_set_vprintf
#include "line_log.h"         // LineLog::MAX_LINE
#include "net/diag_hub.h"     // diagLogLine
#endif

// The task that called consoleBegin (main loop). Only it writes to USB. Before consoleBegin
// (start of setup()) an empty handle = writing allowed.
static TaskHandle_t s_owner = nullptr;

static bool isOwner() { return s_owner == nullptr || xTaskGetCurrentTaskHandle() == s_owner; }

// Sending to USB. The port works with a timeout of 0 ms (Serial.setTxTimeoutMs(0) in setup()):
// without a computer the text is lost, the program does not wait.
static void usbWrite(const char* p, size_t n) {
    if (n > 0) Serial.write((const uint8_t*)p, n);
}

// Formatting into a buffer on the caller's stack. Returns the number of characters in the buffer (after truncation),
// 0 on a format error (Serial.printf did not send anything then either).
static size_t formatTo(char* buf, size_t cap, const char* fmt, va_list ap) {
    int n = vsnprintf(buf, cap, fmt, ap);
    if (n < 0) {
        buf[0] = 0;
        return 0;
    }
    return (size_t)n < cap ? (size_t)n : cap - 1;
}

#if NET_ENABLED

// ---------- line copy to the panel ----------

// Assembling a line from successive main loop calls (e.g. a frame from the radio printed byte by byte).
// Used ONLY by the owner task, so it needs no lock.
static char s_asm[LineLog::MAX_LINE];
static size_t s_asmLen = 0;

static void mirrorOwner(const char* p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c = p[i];
        if (c == '\r') continue;
        if (c == '\n') {
            if (s_asmLen > 0) diagLogLine(s_asm, s_asmLen, millis());   // empty line skipped
            s_asmLen = 0;
            continue;
        }
        if (s_asmLen < sizeof(s_asm)) s_asm[s_asmLen++] = c;   // excess beyond 200 characters is dropped
    }
}

// Text from a task other than the main loop: without the assembly buffer (it belongs to the main loop), every
// piece between '\n' characters is a separate line, also the last one without '\n' (like conLogf).
static void ringOnly(const char* p, size_t n) {
    size_t start = 0;
    for (size_t i = 0; i <= n; i++) {
        if (i == n || p[i] == '\n') {
            size_t len = i - start;
            while (len > 0 && p[start + len - 1] == '\r') len--;
            if (len > 0) diagLogLine(p + start, len > LineLog::MAX_LINE ? LineLog::MAX_LINE : len, millis());
            start = i + 1;
        }
    }
}

// ---------- error lines from the ESP-IDF libraries ----------
// The hook may run simultaneously in several tasks (Wi-Fi, lwIP, HTTP server),
// so the only shared state is a small ring protected by a portMUX critical section (spinlock: the other
// core waits at most the time of copying 128 bytes, nothing sleeps). No heap, no USB,
// no diag_hub lock and no logging of any kind (recursion).

static portMUX_TYPE s_idfMux = portMUX_INITIALIZER_UNLOCKED;
static char s_idfRing[CON_IDF_RING][CON_IDF_LINE_MAX];
static uint8_t s_idfTail = 0;              // oldest line
static volatile uint8_t s_idfCount = 0;    // how many lines are waiting
static volatile uint32_t s_idfDrops = 0;   // lines lost with a full ring
static uint32_t s_idfDropsShown = 0;       // main loop only

static int idfLogHook(const char* fmt, va_list ap) {
    char raw[CON_IDF_LINE_MAX];
    int n = vsnprintf(raw, sizeof(raw), fmt, ap);
    if (n < 0) return n;
    size_t len = (size_t)n < sizeof(raw) ? (size_t)n : sizeof(raw) - 1;

    // Stripping terminal colour codes (ESC '[' ... letter) and end-of-line characters.
    char line[CON_IDF_LINE_MAX];
    size_t out = 0;
    for (size_t i = 0; i < len; i++) {
        char c = raw[i];
        if (c == 0x1B) {
            i++;                                            // '[' (or whatever follows ESC)
            while (i + 1 < len) {
                char d = raw[i + 1];
                i++;
                if (d >= 0x40 && d <= 0x7E) break;          // letter ending the sequence
            }
            continue;
        }
        if (c == '\r' || c == '\n') continue;
        line[out++] = c;
    }
    if (out == 0) return n;
    line[out] = 0;

    portENTER_CRITICAL_SAFE(&s_idfMux);
    if (s_idfCount < CON_IDF_RING) {
        memcpy(s_idfRing[(s_idfTail + s_idfCount) % CON_IDF_RING], line, out + 1);
        s_idfCount = s_idfCount + 1;
    } else {
        s_idfDrops = s_idfDrops + 1;
    }
    portEXIT_CRITICAL_SAFE(&s_idfMux);
    return n;
}

#endif  // NET_ENABLED

// ---------- startup and handling in the loop ----------

void consoleBegin() {
    s_owner = xTaskGetCurrentTaskHandle();
#if NET_ENABLED
    // From now on lines from the ESP-IDF libraries go to the ring, and consolePoll() prints them to USB.
    esp_log_set_vprintf(idfLogHook);
#endif
}

void consolePoll() {
#if NET_ENABLED
    if (s_idfCount == 0 && s_idfDrops == s_idfDropsShown) return;   // the usual case: nothing to do
    for (uint8_t k = 0; k < CON_IDF_RING; k++) {
        char line[CON_IDF_LINE_MAX];
        bool have = false;
        portENTER_CRITICAL(&s_idfMux);
        if (s_idfCount > 0) {
            memcpy(line, s_idfRing[s_idfTail], sizeof(line));
            s_idfTail = (uint8_t)((s_idfTail + 1) % CON_IDF_RING);
            s_idfCount = s_idfCount - 1;
            have = true;
        }
        portEXIT_CRITICAL(&s_idfMux);
        if (!have) break;
        line[sizeof(line) - 1] = 0;
        conPrintf("# idf: %s\n", line);
    }
    uint32_t drops = s_idfDrops;
    if (drops != s_idfDropsShown) {
        conPrintf("# idf: lost lines: %lu\n", (unsigned long)(drops - s_idfDropsShown));
        s_idfDropsShown = drops;
    }
#endif
}

// ---------- printing ----------

void conPrintf(const char* fmt, ...) {
    char buf[CON_PRINTF_MAX];
    va_list ap;
    va_start(ap, fmt);
    size_t n = formatTo(buf, sizeof(buf), fmt, ap);
    va_end(ap);
#if NET_ENABLED
    if (!isOwner()) {          // programmer error: from another task only to the ring, USB untouched
        ringOnly(buf, n);
        return;
    }
    usbWrite(buf, n);
    mirrorOwner(buf, n);
#else
    usbWrite(buf, n);
#endif
}

void conPrintln(const char* text) {
    if (text == nullptr) text = "";
    size_t n = strlen(text);
#if NET_ENABLED
    if (!isOwner()) {
        ringOnly(text, n);
        return;
    }
    usbWrite(text, n);
    usbWrite("\r\n", 2);
    mirrorOwner(text, n);
    mirrorOwner("\n", 1);
#else
    // The same two writes as Serial.println(text): the text, then "\r\n".
    usbWrite(text, n);
    usbWrite("\r\n", 2);
#endif
}

void conUsbPrintf(const char* fmt, ...) {
    if (!isOwner()) return;
    char buf[CON_PRINTF_MAX];
    va_list ap;
    va_start(ap, fmt);
    size_t n = formatTo(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    usbWrite(buf, n);
}

void conUsbPrintln(const char* text) {
    if (!isOwner()) return;
    if (text == nullptr) text = "";
    usbWrite(text, strlen(text));
    usbWrite("\r\n", 2);
}

void conLogf(const char* fmt, ...) {
#if NET_ENABLED
    char buf[CON_LOG_MAX];
    va_list ap;
    va_start(ap, fmt);
    size_t n = formatTo(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) n--;
    // An end-of-line character in the middle of the text would split the ring record: replaced with a space.
    for (size_t i = 0; i < n; i++)
        if (buf[i] == '\n' || buf[i] == '\r') buf[i] = ' ';
    if (n > 0) diagLogLine(buf, n, millis());
#else
    (void)fmt;
#endif
}
