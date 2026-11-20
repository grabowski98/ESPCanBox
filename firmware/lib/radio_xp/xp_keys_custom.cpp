#include "xp_keys_custom.h"

namespace xp {

// Default entries come from the car profile (setDefaults); order = order in the table.
KeyCustomizer::KeyCustomizer() {
    resetMap();
}

void KeyCustomizer::setDefaults(const vehicle::KeyRemapDefault* table, uint8_t count) {
    defaults_ = table;
    defaultCount_ = table ? (count <= MAP_SIZE ? count : MAP_SIZE) : 0;
    resetMap();
}

void KeyCustomizer::resetMap() {
    for (uint8_t i = 0; i < MAP_SIZE; i++) map_[i] = Entry();
    for (uint8_t i = 0; i < defaultCount_; i++) setMap(defaults_[i].from, defaults_[i].to);
}

KeyCustomizer::Entry KeyCustomizer::defaultEntry(uint8_t i) const {
    Entry e;
    if (i < defaultCount_) {
        e.used = defaults_[i].from != defaults_[i].to;   // "XX=XX" = no remapping, as in setMap
        e.from = defaults_[i].from;
        e.to = defaults_[i].to;
    }
    return e;
}

uint8_t KeyCustomizer::mapCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MAP_SIZE; i++) {
        if (map_[i].used) n++;
    }
    return n;
}

bool KeyCustomizer::setMap(uint8_t from, uint8_t to) {
    Entry* free = nullptr;
    for (uint8_t i = 0; i < MAP_SIZE; i++) {
        Entry& e = map_[i];
        if (e.used && e.from == from) {
            if (to == from) e.used = false;
            else e.to = to;
            return true;
        }
        if (!e.used && !free) free = &e;
    }
    if (to == from) return true;          // nothing to remove
    if (!free) return false;
    free->used = true;
    free->from = from;
    free->to = to;
    return true;
}

uint8_t KeyCustomizer::mapped(uint8_t code) const {
    for (uint8_t i = 0; i < MAP_SIZE; i++) {
        if (map_[i].used && map_[i].from == code) return map_[i].to;
    }
    return code;
}

// --- table as text ---

static const char HEX_DIGITS[] = "0123456789ABCDEF";

// One hex digit (lower or upper case); -1 = any other character (also zero and space).
static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Two digits from text[pos]; false = text too short or bad character.
static bool parseByte(const char* text, size_t len, size_t pos, uint8_t& out) {
    if (pos + 2 > len) return false;
    int hi = hexValue(text[pos]);
    int lo = hexValue(text[pos + 1]);
    if (hi < 0 || lo < 0) return false;
    out = (uint8_t)((hi << 4) | lo);
    return true;
}

bool KeyCustomizer::serializeMap(char* out, size_t cap) const {
    if (!out || cap == 0) return false;
    uint8_t n = mapCount();
    // 5 characters per entry, semicolons between entries, zero at the end.
    size_t need = (n == 0) ? 1 : (size_t)n * 5 + (n - 1) + 1;
    if (cap < need) {
        out[0] = 0;
        return false;
    }
    size_t p = 0;
    for (uint8_t i = 0; i < MAP_SIZE; i++) {
        const Entry& e = map_[i];
        if (!e.used) continue;
        if (p > 0) out[p++] = ';';
        out[p++] = HEX_DIGITS[e.from >> 4];
        out[p++] = HEX_DIGITS[e.from & 0x0F];
        out[p++] = '=';
        out[p++] = HEX_DIGITS[e.to >> 4];
        out[p++] = HEX_DIGITS[e.to & 0x0F];
    }
    out[p] = 0;
    return true;
}

bool KeyCustomizer::parseMap(const char* text, size_t len) {
    // First the whole text goes into a working table; the current table changes only when everything is
    // valid (a write from the panel or from settings storage must not leave half a table).
    Entry tmp[MAP_SIZE];
    uint8_t count = 0;
    bool seen[256] = {};          // button codes already used in the text (also in "XX=XX" entries)
    if (len > 0 && !text) return false;

    size_t pos = 0;
    while (pos < len) {
        uint8_t from = 0, to = 0;
        if (!parseByte(text, len, pos, from)) return false;
        if (pos + 2 >= len || text[pos + 2] != '=') return false;
        if (!parseByte(text, len, pos + 3, to)) return false;
        pos += 5;
        if (pos < len) {
            if (text[pos] != ';') return false;
            pos++;                // semicolon; if it was the last character, the loop ends
        }
        if (seen[from]) return false;          // two entries for the same button
        seen[from] = true;
        if (to == from) continue;              // "XX=XX" = no remapping
        if (count >= MAP_SIZE) return false;   // more entries than slots in the table
        tmp[count].used = true;
        tmp[count].from = from;
        tmp[count].to = to;
        count++;
    }

    for (uint8_t i = 0; i < MAP_SIZE; i++) map_[i] = tmp[i];   // unused: used = false
    return true;
}

void KeyCustomizer::setLatchKeys(const uint8_t* codes, uint8_t count) {
    latchCount_ = 0;
    latched_ = 0;
    for (uint8_t i = 0; codes && i < count && latchCount_ < LATCH_MAX; i++)
        if (codes[i] != 0) latch_[latchCount_++] = codes[i];
}

bool KeyCustomizer::onEvent(const KeyEvent& in, KeyEvent& out) {
    out = in;
    for (uint8_t i = 0; i < latchCount_; i++) {
        if (in.code != latch_[i]) continue;
        if (in.state != KEY_STATE_PRESSED) return false;   // hold and release are skipped
        latched_ ^= (uint8_t)(1u << i);
        out.state = (latched_ & (1u << i)) ? KEY_STATE_PRESSED : KEY_STATE_RELEASED;
        return true;
    }
    out.code = mapped(in.code);
    return true;
}

bool KeyCustomizer::onIgnitionOff(KeyEvent& out) {
    for (uint8_t i = 0; i < latchCount_; i++) {
        if (!(latched_ & (1u << i))) continue;
        latched_ &= (uint8_t)~(1u << i);
        out.code = latch_[i];
        out.state = KEY_STATE_RELEASED;
        return true;
    }
    return false;
}

}  // namespace xp
