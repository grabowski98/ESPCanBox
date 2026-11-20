#include "line_log.h"

#include <string.h>

// Line numbers are unsigned 32-bit numbers; "older / newer" comparisons use the difference
// cast to int32_t, so counter wrap-around (after 4 billion lines) breaks nothing.
static bool before(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }

void LineLog::append(const char* text, size_t len, uint32_t ms) {
    if (text == nullptr) return;
    // Trailing end-of-line characters are cut off (the console appends "\r\n").
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) len--;
    if (len == 0) return;
    if (len > MAX_LINE) len = MAX_LINE;

    // Make room by removing the oldest records. A record has at most 6 + 200 bytes and the ring 8192,
    // so the loop always ends.
    const size_t need = HEADER + len;
    while (used_ + need > STORAGE) dropOldest();

    size_t pos = tail_ + used_;
    put(pos + 0, (uint8_t)(len & 0xFF));
    put(pos + 1, (uint8_t)(len >> 8));
    put(pos + 2, (uint8_t)(ms & 0xFF));
    put(pos + 3, (uint8_t)(ms >> 8));
    put(pos + 4, (uint8_t)(ms >> 16));
    put(pos + 5, (uint8_t)(ms >> 24));
    for (size_t i = 0; i < len; i++) put(pos + HEADER + i, (uint8_t)text[i]);
    used_ += need;
    head_++;
}

void LineLog::dropOldest() {
    if (used_ == 0) return;
    size_t len = (size_t)at(tail_) | ((size_t)at(tail_ + 1) << 8);
    size_t rec = HEADER + len;
    tail_ = (tail_ + rec) % STORAGE;
    used_ -= rec;
    oldest_++;
}

size_t LineLog::read(uint32_t& cursor, Entry* entries, size_t maxEntries, char* text, size_t textCap,
                     uint32_t& dropped) const {
    dropped = 0;
    // A cursor "from the future" = head (nothing to return).
    if (before(head_, cursor)) cursor = head_;
    // Lines that have already fallen out of the ring.
    if (before(cursor, oldest_)) {
        dropped = oldest_ - cursor;
        cursor = oldest_;
    }
    if (entries == nullptr || text == nullptr || maxEntries == 0 || textCap == 0) return 0;

    // Walk to the record with number 'cursor' (at most about 1300 records in 8 KB).
    size_t pos = tail_;
    for (uint32_t n = oldest_; n != cursor; n++) {
        size_t len = (size_t)at(pos) | ((size_t)at(pos + 1) << 8);
        pos = (pos + HEADER + len) % STORAGE;
    }

    size_t count = 0;
    size_t textUsed = 0;
    while (cursor != head_ && count < maxEntries) {
        size_t len = (size_t)at(pos) | ((size_t)at(pos + 1) << 8);
        // The line with its terminating zero must fit entirely; offset in Entry has 16 bits.
        if (textUsed + len + 1 > textCap || textUsed + len + 1 > 0xFFFF) break;
        Entry& e = entries[count];
        e.ms = (uint32_t)at(pos + 2) | ((uint32_t)at(pos + 3) << 8) | ((uint32_t)at(pos + 4) << 16) |
               ((uint32_t)at(pos + 5) << 24);
        e.offset = (uint16_t)textUsed;
        e.len = (uint16_t)len;
        for (size_t i = 0; i < len; i++) text[textUsed + i] = (char)at(pos + HEADER + i);
        text[textUsed + len] = '\0';
        textUsed += len + 1;
        pos = (pos + HEADER + len) % STORAGE;
        count++;
        cursor++;
    }
    return count;
}

// Clears the content but does NOT reset the numbering: browser cursors stay valid (they get "nothing new").
void LineLog::clear() {
    tail_ = 0;
    used_ = 0;
    oldest_ = head_;
}
