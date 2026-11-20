// JSON writer into a fixed-size buffer, no heap.
// Takes care of commas and quotes; when space runs out it sets an error flag and stops writing
// (the buffer always ends with a zero). The caller checks ok() before sending.
//
// Example:
//   JsonWriter j(buf, sizeof(buf));
//   j.beginObject(); j.key("rx"); j.unum(123); j.key("ok"); j.boolean(true); j.endObject();
//   -> {"rx":123,"ok":true}
#pragma once
#include <stddef.h>
#include <stdint.h>

class JsonWriter {
public:
    JsonWriter(char* buf, size_t cap);

    void beginObject();
    void endObject();
    void beginArray();
    void endArray();
    void key(const char* name);                  // field name; only inside an object

    // Strings: a quote and a backslash get a backslash, control characters (below 0x20) are written as \u00XX.
    // Bytes 0x80..0xFF: a valid UTF-8 sequence passes unchanged, and a byte that does not belong to such
    // a sequence is written as U+FFFD. Thanks to this the result is always valid
    // JSON in UTF-8, even when the text came from outside (Wi-Fi network name, console line).
    // Worst case: 6 output characters per 1 input byte.
    void str(const char* s);                     // zero-terminated string (nullptr -> null)
    void strN(const char* s, size_t n);          // string of the given length
    void boolean(bool v);
    void num(int32_t v);
    void unum(uint32_t v);
    void fixed(float v, uint8_t decimals);       // number with a decimal point, 0..3 places; NaN / infinity -> null
    void null();
    void hex(const uint8_t* data, size_t n);     // bytes as the string "B0F000" (upper case, no spaces)
    void hexByte(uint8_t v);                     // one byte as the string "9F"
    void hexId(uint16_t id);                     // CAN identifier as a three-digit string: "0E1"

    bool ok() const { return ok_; }              // false = out of space or a nesting error
    size_t length() const { return len_; }
    const char* c_str() const { return buf_; }
    void reset();

    static const uint8_t MAX_DEPTH = 8;

private:
    char* buf_;
    size_t cap_;
    size_t len_ = 0;
    bool ok_ = true;
    uint8_t depth_ = 0;
    bool first_[MAX_DEPTH];      // nothing written yet in the current object / array
    bool isObject_[MAX_DEPTH];
    bool afterKey_ = false;

    void raw(const char* s, size_t n);
    void rawChar(char c);
    void beforeValue();          // comma, if needed
    void escaped(const char* s, size_t n);
};
