// Ring of text lines: a copy of the console for the web panel.
// No hardware and no heap: a fixed byte array, the oldest lines give way to new ones.
// Every line has a sequence number (seq). The reader remembers the number from which it wants to continue
// (the "cursor"), so the device keeps no client state: the browser asks
// "give me everything from number N" and gets what is still there plus the number of lines that were lost.
//
// The class is NOT safe for several tasks at once: locking is provided by the owner (src/net/diag_hub).
#pragma once
#include <stddef.h>
#include <stdint.h>

class LineLog {
public:
    static const size_t STORAGE = 8192;    // bytes for lines including headers (about 200 typical lines)
    static const size_t MAX_LINE = 200;    // a longer line is truncated to this length

    struct Entry {
        uint32_t ms = 0;        // time the line was appended (millis)
        uint16_t offset = 0;    // start of the text in the buffer given to read()
        uint16_t len = 0;       // text length (without the terminating zero)
    };

    // Appends one line. 'text' need not be zero-terminated; trailing end-of-line characters are cut off.
    // An empty line is skipped.
    void append(const char* text, size_t len, uint32_t ms);

    uint32_t head() const { return head_; }      // number the next appended line will get
    uint32_t oldest() const { return oldest_; }  // number of the oldest line in the ring (== head when empty)

    // Copies consecutive lines from number 'cursor'. The text goes to 'text' (each line zero-terminated),
    // the line descriptions to 'entries'. Returns the number of copied lines; stops when space runs out
    // in 'entries' or in 'text' (a line is never split).
    // On return 'cursor' = number of the first line that was NOT copied.
    // 'dropped' = how many lines from the requested number have already been lost (0 when nothing was lost).
    // A cursor "from the future" (greater than head) is treated as head: nothing to return.
    size_t read(uint32_t& cursor, Entry* entries, size_t maxEntries, char* text, size_t textCap,
                uint32_t& dropped) const;

    void clear();

private:
    // Record in the ring: 2 bytes of text length, 4 bytes of time, the text. Records lie one after another
    // and may wrap around the end of the array.
    static const size_t HEADER = 6;
    uint8_t buf_[STORAGE] = {0};
    size_t tail_ = 0;          // position of the oldest record
    size_t used_ = 0;          // bytes in use
    uint32_t head_ = 0;
    uint32_t oldest_ = 0;

    uint8_t at(size_t pos) const { return buf_[pos % STORAGE]; }
    void put(size_t pos, uint8_t v) { buf_[pos % STORAGE] = v; }
    void dropOldest();
};
