// Host test: console line ring (line_log) and the frame view and ID table (frame_trace).
// Checks the next / head / dropped cursors according to docs/PANEL_API.md, chapter 1.5.
//
// Build (from the firmware directory):
//   uvx --from ziglang python -m ziglang c++ -std=c++17 -O1 -Wall -I lib/core test_host/net_rings.cpp
//       lib/core/line_log.cpp lib/core/frame_trace.cpp -o .build_host/net_rings.exe
#include <cstdio>
#include <cstring>

#include "frame_trace.h"
#include "line_log.h"

static int failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) failures++;
}

// Large objects statically (FrameTrace is 32 KB), as in the device.
static LineLog lg;
static FrameTrace tr;
static FrameTable tab;

static void addLine(LineLog& l, unsigned n, uint32_t ms) {
    char t[32];
    int len = std::snprintf(t, sizeof(t), "entry %u\r\n", n);
    l.append(t, (size_t)len, ms);
}

static void testLineLog() {
    LineLog::Entry e[64];
    static char text[4096];
    uint32_t cur, dropped;

    lg.clear();
    check(lg.head() == 0 && lg.oldest() == 0, "log: empty after startup");
    cur = 0;
    check(lg.read(cur, e, 64, text, sizeof(text), dropped) == 0 && cur == 0 && dropped == 0, "log: reading an empty one");

    lg.append("\r\n", 2, 1);
    lg.append("", 0, 1);
    check(lg.head() == 0, "log: empty lines skipped");

    for (unsigned i = 0; i < 10; i++) addLine(lg, i, 1000 + i);
    cur = 0;
    size_t n = lg.read(cur, e, 64, text, sizeof(text), dropped);
    check(n == 10 && cur == 10 && dropped == 0 && lg.head() == 10, "log: 10 lines, next == head");
    check(e[3].ms == 1003 && e[3].len == 7 && strcmp(text + e[3].offset, "entry 3") == 0,
          "log: text without end of line, line time");

    cur = 7;
    n = lg.read(cur, e, 64, text, sizeof(text), dropped);
    check(n == 3 && cur == 10 && strcmp(text + e[0].offset, "entry 7") == 0, "log: reading from cursor 7");

    cur = 500;
    n = lg.read(cur, e, 64, text, sizeof(text), dropped);
    check(n == 0 && cur == 10 && dropped == 0, "log: cursor from the future = head");

    // entry count limit: at most 4, the rest in the next read (next < head)
    cur = 0;
    n = lg.read(cur, e, 4, text, sizeof(text), dropped);
    check(n == 4 && cur == 4 && cur < lg.head(), "log: entry limit, next < head");

    // byte budget: "entry N" + zero = 8 bytes; 20 bytes fit 2 lines, a line is not split
    cur = 0;
    n = lg.read(cur, e, 64, text, 20, dropped);
    check(n == 2 && cur == 2, "log: text byte budget, a line is not split");
    cur = 0;
    n = lg.read(cur, e, 64, text, 7, dropped);
    check(n == 0 && cur == 0, "log: a line that does not fit at all -> nothing, cursor unchanged");

    // truncation of a long line
    static char longLine[300];
    memset(longLine, 'x', sizeof(longLine));
    lg.append(longLine, sizeof(longLine), 5000);
    cur = 10;
    n = lg.read(cur, e, 64, text, sizeof(text), dropped);
    check(n == 1 && e[0].len == LineLog::MAX_LINE && strlen(text + e[0].offset) == 200, "log: line > 200 cut");

    // ring overflow: lines of 6 + 200 bytes, 8192 bytes is 39 records
    lg.clear();
    check(lg.head() == 11 && lg.oldest() == 11, "log: clear does not reset the numbering");
    uint32_t start = lg.head();
    for (unsigned i = 0; i < 100; i++) {
        longLine[0] = (char)('A' + (i % 26));
        lg.append(longLine, 200, 10000 + i);
    }
    uint32_t kept = lg.head() - lg.oldest();
    check(kept == LineLog::STORAGE / (6 + 200), "log: the ring keeps as many whole lines as fit");
    cur = start;
    n = lg.read(cur, e, 64, text, sizeof(text), dropped);
    check(dropped == lg.oldest() - start && dropped == 100 - kept, "log: dropped = lines that fell out");
    check(n == 4096 / 201 && cur == lg.oldest() + n, "log: reading after an overflow from the oldest");
    check(e[0].ms == 10000 + (100 - kept) && text[e[0].offset] == (char)('A' + ((100 - kept) % 26)),
          "log: the oldest kept line is correct");

    // exact fill with wrap-around through the end of the array: mixed lengths
    lg.clear();
    uint32_t base = lg.head();
    bool good = true;
    for (unsigned i = 0; i < 3000; i++) {
        char t[64];
        int len = std::snprintf(t, sizeof(t), "m%u %.*s", i, (int)(i % 40), "abcdefghijklmnopqrstuvwxyz0123456789ABCD");
        lg.append(t, (size_t)len, i);
    }
    // we read everything that remains, in chunks, and check the numbering is continuous
    cur = base;
    uint32_t expect = lg.oldest();
    bool first = true;
    while (cur != lg.head()) {
        n = lg.read(cur, e, 64, text, sizeof(text), dropped);
        if (first) {
            if (dropped != lg.oldest() - base) good = false;
            first = false;
        } else if (dropped != 0) good = false;
        for (size_t k = 0; k < n; k++) {
            char t[64];
            unsigned idx = expect - base;
            int len = std::snprintf(t, sizeof(t), "m%u %.*s", idx, (int)(idx % 40), "abcdefghijklmnopqrstuvwxyz0123456789ABCD");
            if (e[k].len != (uint16_t)len || memcmp(text + e[k].offset, t, (size_t)len) != 0 || e[k].ms != idx) good = false;
            expect++;
        }
        if (n == 0) {
            good = false;
            break;
        }
    }
    check(good && expect == lg.head(), "log: 3000 lines of different length, wrapping, continuous numbers");
}

static CanFrame fr(uint32_t id, uint8_t len, uint8_t b0) {
    CanFrame f;
    f.id = id;
    f.len = len;
    for (uint8_t i = 0; i < 8; i++) f.data[i] = (uint8_t)(b0 + i);
    return f;
}

static void testTrace() {
    static TraceRecord out[400];
    uint32_t cur, dropped;

    tr.clear();
    check(tr.head() == 0 && tr.oldest() == 0, "trace: empty");
    for (uint32_t i = 0; i < 100; i++) tr.push(fr(i % 2 ? 0x0E1 : 0x21F, (uint8_t)(i % 9), (uint8_t)i), 1000 + i);
    cur = 0;
    size_t n = tr.read(cur, out, 400, dropped);
    check(n == 100 && cur == 100 && dropped == 0, "trace: 100 frames, next == head");
    check(out[5].id == 0x0E1 && out[5].len == 5 && out[5].data[0] == 5 && out[5].data[4] == 9 && out[5].data[5] == 0 &&
              out[5].ms == 1005,
          "trace: frame content, bytes past the length zeroed");

    cur = 0;
    n = tr.read(cur, out, 10, dropped);
    check(n == 10 && cur == 10, "trace: max limit, next < head");

    cur = 0;
    n = tr.read(cur, out, 10, dropped, 0x0E1);
    check(n == 10 && cur == 20 && out[0].id == 0x0E1 && out[9].id == 0x0E1, "trace: the ID filter moves the cursor over other frames");
    cur = 0;
    n = tr.read(cur, out, 400, dropped, 0x123);
    check(n == 0 && cur == 100, "trace: a filter without hits goes to head");

    cur = 9999;
    n = tr.read(cur, out, 400, dropped);
    check(n == 0 && cur == 100 && dropped == 0, "trace: cursor from the future = head");

    CanFrame bad = fr(0x7E1, 200, 0);    // a bad length and ID > 11 bits do not corrupt the record
    bad.id = 0x1FFFF0E1;
    tr.push(bad, 5);
    cur = 100;
    n = tr.read(cur, out, 400, dropped);
    check(n == 1 && out[0].len == 8 && out[0].id == 0x0E1, "trace: length cut to 8, ID 11 bits");

    // overflow: 5000 frames into a ring of 2048
    tr.clear();
    uint32_t base = tr.head();
    for (uint32_t i = 0; i < 5000; i++) tr.push(fr(0x100 + (i & 0xFF), 8, (uint8_t)i), i);
    check(tr.head() == base + 5000 && tr.oldest() == base + 5000 - FrameTrace::CAPACITY, "trace: oldest after an overflow");
    cur = base;
    n = tr.read(cur, out, 400, dropped);
    check(dropped == 5000 - FrameTrace::CAPACITY && n == 400 && out[0].ms == 5000 - FrameTrace::CAPACITY &&
              cur == tr.oldest() + 400,
          "trace: dropped and reading from the oldest");
    n = tr.read(cur, out, 400, dropped);
    check(dropped == 0 && n == 400 && out[0].ms == 5000 - FrameTrace::CAPACITY + 400, "trace: the next read without losses");

    // the client falls behind while reading (frames appended between reads)
    cur = tr.head() - 10;
    for (uint32_t i = 0; i < FrameTrace::CAPACITY; i++) tr.push(fr(0x200, 1, 0), 9000 + i);
    n = tr.read(cur, out, 400, dropped);
    check(dropped == 10 && n == 400 && out[0].ms == 9000, "trace: a client lagging behind loses only what fell out");

    // 32-bit numbering wrap-around
    tr.clear();
    for (uint32_t i = 0; i < 300; i++) tr.push(fr(0x300, 1, 0), i);    // head = something + 300
    // head_ cannot be set from outside; we check that differences are computed correctly for cursor 0xFFFFFF00
    cur = 0xFFFFFF00u;    // "long ago" relative to head (before zero after the wrap-around)
    n = tr.read(cur, out, 400, dropped);
    check(cur <= tr.head() && dropped > 0 && n > 0, "trace: a cursor from before the wrap is treated as old");
}

static void testTable() {
    tab.clear();
    tab.onFrame(fr(0x0E1, 8, 1), 100);
    tab.onFrame(fr(0x036, 8, 2), 105);
    tab.onFrame(fr(0x21F, 3, 3), 110);
    tab.onFrame(fr(0x0E1, 8, 4), 200);
    tab.onFrame(fr(0x0E1, 8, 5), 290);
    check(tab.size() == 3 && tab.at(0).id == 0x036 && tab.at(1).id == 0x0E1 && tab.at(2).id == 0x21F,
          "table: sorted by ascending ID");
    const TableEntry& e = tab.at(1);
    check(e.count == 3 && e.lastMs == 290 && e.periodMs == 90 && e.data[0] == 5, "table: counter, time, interval, data");
    check(tab.at(0).periodMs == 0 && tab.at(0).count == 1, "table: one frame -> interval 0");
    check(tab.at(2).len == 3 && tab.at(2).data[3] == 0, "table: length and bytes past the length");

    // insertion in random order and overflow of 128 IDs
    tab.clear();
    uint32_t x = 12345;
    bool sorted = true;
    for (int i = 0; i < 2000; i++) {
        x = x * 1103515245u + 12345u;
        uint16_t id = (uint16_t)((x >> 16) % 300);    // 300 different IDs > 128
        tab.onFrame(fr(id, 1, 0), (uint32_t)i);
    }
    for (uint8_t i = 1; i < tab.size(); i++)
        if (tab.at((uint8_t)(i - 1)).id >= tab.at(i).id) sorted = false;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < tab.size(); i++) sum += tab.at(i).count;
    check(tab.size() == FrameTable::MAX_IDS && sorted, "table: 128 IDs, still sorted");
    check(tab.overflow() > 0 && sum + tab.overflow() == 2000, "table: frames of extra IDs count overflow");
    tab.clear();
    check(tab.size() == 0 && tab.overflow() == 0, "table: clear");
}

int main() {
    testLineLog();
    testTrace();
    testTable();
    if (failures == 0) std::printf("\nALL OK\n");
    else std::printf("\nERRORS: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
