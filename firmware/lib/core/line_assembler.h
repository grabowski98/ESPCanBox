// Assembles console command lines from single characters (USB console, src/console_cmds.cpp).
// No hardware, no allocation: the caller gives a static buffer.
//
// End of line: '\n' or '\r' (browser terminals send a bare '\r' on Enter; with "\r\n" the second half gives an
// empty line, which the command handler skips). A line longer than the buffer is NOT cut and executed: it is
// reported as TOO_LONG and dropped as a whole (found in a code review: a truncated "settings import {...}" or a
// truncated frame line used to be executed). Test on a PC: test_host/console_alias.cpp.
#pragma once
#include <stddef.h>

class LineAssembler {
public:
    enum Result : unsigned char {
        NONE,       // character stored (or ignored), no line yet
        LINE,       // a complete line in buffer() (terminated with zero)
        TOO_LONG,   // a line longer than cap - 1 characters ended: dropped, nothing to execute
    };

    // buf: room for the longest line plus the terminating zero (cap >= 1).
    LineAssembler(char* buf, size_t cap) : buf_(buf), cap_(cap) {}

    Result feed(char c) {
        if (c == '\r' || c == '\n') {
            const bool over = overflow_;
            buf_[over ? 0 : n_] = 0;
            n_ = 0;
            overflow_ = false;
            return over ? TOO_LONG : LINE;
        }
        if (overflow_) return NONE;              // rest of an overlong line: skipped up to its end
        if (n_ + 1 < cap_) {
            buf_[n_++] = c;
        } else {
            overflow_ = true;
            n_ = 0;
        }
        return NONE;
    }

    const char* buffer() const { return buf_; }
    char* buffer() { return buf_; }
    size_t capacity() const { return cap_; }

private:
    char* buf_;
    size_t cap_;
    size_t n_ = 0;
    bool overflow_ = false;
};
