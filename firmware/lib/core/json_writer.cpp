#include "json_writer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char HEX_DIGITS[] = "0123456789ABCDEF";

JsonWriter::JsonWriter(char* buf, size_t cap) : buf_(buf), cap_(cap) { reset(); }

void JsonWriter::reset() {
    len_ = 0;
    depth_ = 0;
    afterKey_ = false;
    ok_ = (buf_ != nullptr && cap_ > 0);
    if (ok_) buf_[0] = '\0';
}

// The only place that writes to the buffer. A write is "all or nothing": when a piece does not fit,
// the ok_ flag drops and the buffer is zero-terminated after the last complete piece.
void JsonWriter::raw(const char* s, size_t n) {
    if (!ok_) return;
    if (n >= cap_ - len_) {    // n bytes are needed plus one more for the zero
        ok_ = false;
        return;
    }
    memcpy(buf_ + len_, s, n);
    len_ += n;
    buf_[len_] = '\0';
}

void JsonWriter::rawChar(char c) { raw(&c, 1); }

// Before every value: after a field name nothing; in an array a comma if it is not the first element;
// a value in an object without a field name or a second value at the top level is a nesting error.
void JsonWriter::beforeValue() {
    if (!ok_) return;
    if (depth_ == 0) {
        if (len_ > 0) ok_ = false;
        return;
    }
    if (afterKey_) {
        afterKey_ = false;
        return;
    }
    if (isObject_[depth_ - 1]) {
        ok_ = false;
        return;
    }
    if (!first_[depth_ - 1]) rawChar(',');
    first_[depth_ - 1] = false;
}

void JsonWriter::beginObject() {
    beforeValue();
    if (!ok_) return;
    if (depth_ >= MAX_DEPTH) {
        ok_ = false;
        return;
    }
    rawChar('{');
    first_[depth_] = true;
    isObject_[depth_] = true;
    depth_++;
}

void JsonWriter::endObject() {
    if (!ok_) return;
    if (depth_ == 0 || !isObject_[depth_ - 1] || afterKey_) {
        ok_ = false;
        return;
    }
    rawChar('}');
    depth_--;
}

void JsonWriter::beginArray() {
    beforeValue();
    if (!ok_) return;
    if (depth_ >= MAX_DEPTH) {
        ok_ = false;
        return;
    }
    rawChar('[');
    first_[depth_] = true;
    isObject_[depth_] = false;
    depth_++;
}

void JsonWriter::endArray() {
    if (!ok_) return;
    if (depth_ == 0 || isObject_[depth_ - 1]) {
        ok_ = false;
        return;
    }
    rawChar(']');
    depth_--;
}

void JsonWriter::key(const char* name) {
    if (!ok_) return;
    if (depth_ == 0 || !isObject_[depth_ - 1] || afterKey_ || name == nullptr) {
        ok_ = false;
        return;
    }
    if (!first_[depth_ - 1]) rawChar(',');
    first_[depth_ - 1] = false;
    rawChar('"');
    escaped(name, strlen(name));
    rawChar('"');
    rawChar(':');
    afterKey_ = true;
}

// Length of a valid UTF-8 sequence starting at s[0] (1..4), or 0 when the byte does not start
// a valid sequence, the sequence is truncated, overlong (e.g. C0 80), or encodes a UTF-16 surrogate
// (ED A0..BF) or a value above U+10FFFF.
static size_t utf8Len(const uint8_t* s, size_t avail) {
    const uint8_t c = s[0];
    size_t n;
    uint8_t lo = 0x80, hi = 0xBF;    // allowed range of the SECOND byte
    if (c < 0x80) return 1;
    if (c >= 0xC2 && c <= 0xDF) n = 2;
    else if (c >= 0xE0 && c <= 0xEF) {
        n = 3;
        if (c == 0xE0) lo = 0xA0;
        if (c == 0xED) hi = 0x9F;
    } else if (c >= 0xF0 && c <= 0xF4) {
        n = 4;
        if (c == 0xF0) lo = 0x90;
        if (c == 0xF4) hi = 0x8F;
    } else return 0;
    if (avail < n) return 0;
    if (s[1] < lo || s[1] > hi) return 0;
    for (size_t i = 2; i < n; i++)
        if (s[i] < 0x80 || s[i] > 0xBF) return 0;
    return n;
}

// Quoted string content (without the quotes themselves). Ordinary characters go to the buffer in whole runs.
void JsonWriter::escaped(const char* s, size_t n) {
    const uint8_t* p = (const uint8_t*)s;
    size_t runStart = 0;
    size_t i = 0;
    while (i < n && ok_) {
        const uint8_t c = p[i];
        if (c >= 0x20 && c != '"' && c != '\\' && c < 0x80) {
            i++;
            continue;
        }
        if (c >= 0x80) {
            size_t len = utf8Len(p + i, n - i);
            if (len > 0) {    // a valid sequence passes unchanged
                i += len;
                continue;
            }
        }
        raw(s + runStart, i - runStart);
        if (c == '"') raw("\\\"", 2);
        else if (c == '\\') raw("\\\\", 2);
        else if (c < 0x20) {
            char e[6] = {'\\', 'u', '0', '0', HEX_DIGITS[c >> 4], HEX_DIGITS[c & 0x0F]};
            raw(e, 6);
        } else raw("\\ufffd", 6);    // a byte outside a valid UTF-8 sequence
        i++;
        runStart = i;
    }
    raw(s + runStart, i - runStart);
}

void JsonWriter::str(const char* s) {
    if (s == nullptr) {
        null();
        return;
    }
    strN(s, strlen(s));
}

void JsonWriter::strN(const char* s, size_t n) {
    if (s == nullptr) {
        null();
        return;
    }
    beforeValue();
    rawChar('"');
    escaped(s, n);
    rawChar('"');
}

void JsonWriter::boolean(bool v) {
    beforeValue();
    if (v) raw("true", 4);
    else raw("false", 5);
}

void JsonWriter::num(int32_t v) {
    char t[12];
    int n = snprintf(t, sizeof(t), "%ld", (long)v);
    beforeValue();
    raw(t, (size_t)n);
}

void JsonWriter::unum(uint32_t v) {
    char t[12];
    int n = snprintf(t, sizeof(t), "%lu", (unsigned long)v);
    beforeValue();
    raw(t, (size_t)n);
}

void JsonWriter::fixed(float v, uint8_t decimals) {
    if (isnan(v) || isinf(v)) {
        null();
        return;
    }
    if (decimals > 3) decimals = 3;
    char t[48];    // the largest float (3.4e38) is 39 digits, sign, point and 3 places
    int n = snprintf(t, sizeof(t), "%.*f", (int)decimals, (double)v);
    beforeValue();
    if (n <= 0 || (size_t)n >= sizeof(t)) {
        ok_ = false;
        return;
    }
    raw(t, (size_t)n);
}

void JsonWriter::null() {
    beforeValue();
    raw("null", 4);
}

void JsonWriter::hex(const uint8_t* data, size_t n) {
    if (data == nullptr && n > 0) {
        null();
        return;
    }
    beforeValue();
    rawChar('"');
    char t[32];
    size_t k = 0;
    for (size_t i = 0; i < n && ok_; i++) {
        t[k++] = HEX_DIGITS[data[i] >> 4];
        t[k++] = HEX_DIGITS[data[i] & 0x0F];
        if (k == sizeof(t)) {
            raw(t, k);
            k = 0;
        }
    }
    raw(t, k);
    rawChar('"');
}

void JsonWriter::hexByte(uint8_t v) { hex(&v, 1); }

void JsonWriter::hexId(uint16_t id) {
    char t[8];
    int n = snprintf(t, sizeof(t), "\"%03X\"", (unsigned)id);    // 3 digits; an ID above FFF would give 4
    beforeValue();
    raw(t, (size_t)n);
}
