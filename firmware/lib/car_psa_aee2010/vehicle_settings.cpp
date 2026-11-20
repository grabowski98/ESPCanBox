#include "vehicle_settings.h"

namespace vset {

// Language: code from frame 260 (bits 1.6-1.2) -> number in the radio list (settings-map.md).
// Only Polish is measured (9 -> 8, radios/xp-simple-soft/provenance.md). Any other code is not translated:
// 0xFF = no change, the radio keeps the language it had. A row is added only with a recording of that language.
uint8_t radioLanguage(uint8_t psa) {
    switch (psa) {
        case 9: return 8;    // Polish
        default: return 0xFF;
    }
}

// ====================== Settings table and pure logic of frame 15B ======================

namespace {

// Table of items of the radio screen with the feature settings (command 80 nn vv), in number order.
// Every number was recorded on 2026-11-19 while the owner went through the radio screen "Function settings" row
// by row; the names are the rows of that screen (radios/xp-simple-soft/provenance.md). The radio did not send
// 04, 08 or 0E, so they are not in the table (a command with such a number is an unknown setting).
// Groups, basis and 361 bits: radios/xp-simple-soft/provenance.md and vehicles/peugeot-508-sw-2015/settings-map.md.
const SettingDef TABLE[] = {
    // id    name                            kind                 min max  361: byte, mask   basis
    {0x01, "parking assist",          Kind::SENDABLE,      0, 1,  0, 0x00, "measured"},
    {0x02, "rear wiper in reverse",     Kind::SENDABLE,      0, 1,  2, 0x40, "state 17.11"},   // 361 2.6
    {0x03, "boot unlocking",       Kind::NO_FRAME,      0, 1,  0, 0x00, "no frame"},
    {0x05, "radar switch",           Kind::KNOWN_BLOCKED, 0, 1,  0, 0x00, "frame 1A9"},
    {0x06, "follow me home",         Kind::SENDABLE,      0, 3,  2, 0x20, "state 17.11"},   // 361 2.5
    {0x07, "welcome lighting",        Kind::SENDABLE,      0, 3,  1, 0x20, "state 17.11"},   // 361 1.5
    {0x09, "sound",                       Kind::SENDABLE,      1, 4,  3, 0x08, "code+description"},     // 361 3.3
    {0x0A, "fuel units",             Kind::FIXED_REFUSE,  0, 2,  0, 0x00, "code+description"},
    {0x0B, "language",                        Kind::FIXED_REFUSE,  0, 12, 0, 0x00, "code+description"},
    {0x0C, "blind spot",                  Kind::KNOWN_BLOCKED, 0, 1,  0, 0x00, "frame 1A9"},
    {0x0D, "Start/Stop",                   Kind::NO_FRAME,      0, 1,  0, 0x00, "no frame"},      // 1A9 7.7 never recorded
    {0x0F, "door opening",               Kind::SENDABLE,      0, 1,  2, 0x02, "code+description"},     // 361 2.1
    {0x10, "tyre calibration",              Kind::NO_FRAME,      0, 1,  0, 0x00, "no frame"},
    {0x12, "headlamp delay",       Kind::NO_FRAME,      0, 3,  0, 0x00, "no frame"},
    {0x14, "traction control",             Kind::NO_FRAME,      0, 1,  0, 0x00, "no frame"},
    {0x15, "fatigue detection",         Kind::FIXED_REFUSE,  0, 1,  0, 0x00, "code+description"},
    {0x16, "dial colour",                Kind::NO_FRAME,      0, 1,  0, 0x00, "no frame"},
    {0x18, "automatic lights",         Kind::FIXED_REFUSE,  0, 1,  0, 0x00, "description only"},
    {0x19, "automatic locking",      Kind::NO_FRAME,      0, 1,  0, 0x00, "no frame"},
    {0x1A, "adaptive headlamps",       Kind::FIXED_REFUSE,  0, 1,  0, 0x00, "description only"},
    {0x1B, "Collision risk Activation",    Kind::NO_FRAME,      0, 1,  0, 0x00, "no frame"},
    {0x1C, "Collision risk Sensitivity",   Kind::NO_FRAME,      0, 2,  0, 0x00, "no frame"},
    {0x1D, "Speed recommendation",         Kind::NO_FRAME,      0, 1,  0, 0x00, "no frame"},
    {0x1E, "Directional headlamps",        Kind::NO_FRAME,      0, 1,  0, 0x00, "no frame"},
    {0x1F, "door mirror folding",          Kind::NO_FRAME,      0, 1,  0, 0x00, "no frame"},
};
const uint8_t TABLE_N = (uint8_t)(sizeof(TABLE) / sizeof(TABLE[0]));

bool full260(const RawFrame& f) { return f.seen && f.len == 7; }

}  // namespace

const SettingDef* find(uint8_t id) {
    for (uint8_t i = 0; i < TABLE_N; i++)
        if (TABLE[i].id == id) return &TABLE[i];
    return nullptr;
}

uint8_t count() { return TABLE_N; }

uint32_t sendableMask() {
    uint32_t m = 0;
    for (uint8_t i = 0; i < TABLE_N; i++)
        if ((TABLE[i].kind == Kind::SENDABLE || TABLE[i].kind == Kind::KNOWN_BLOCKED) && TABLE[i].id < 32) m |= 1UL << TABLE[i].id;
    return m;
}

MaskResult maskChange(uint32_t mask, uint8_t id, bool on, uint32_t& out) {
    out = mask;
    const SettingDef* d = find(id);
    if (!d) return MaskResult::UNKNOWN_ID;
    if ((d->kind != Kind::SENDABLE && d->kind != Kind::KNOWN_BLOCKED) || id >= 32) return MaskResult::NOT_SENDABLE;
    out = on ? (mask | (1UL << id)) : (mask & ~(1UL << id));
    return MaskResult::OK;
}

const SettingDef& at(uint8_t i) { return TABLE[i < TABLE_N ? i : 0]; }

const char* refuseName(Refuse r) {
    switch (r) {
        case Refuse::OK: return "OK";
        case Refuse::UNKNOWN_ID: return "unknown number";
        case Refuse::NO_FRAME_FOR_ID: return "no known frame";
        case Refuse::BLOCKED_OTHER_FRAME: return "button in frame 1A9";
        case Refuse::NO_STATE: return "no fresh state frame";
        case Refuse::FIXED_REFUSE: return "fixed refusal";
        case Refuse::DISABLED_BY_MASK: return "disabled in mask";
        case Refuse::BAD_VALUE: return "value out of range";
        case Refuse::NO_361: return "no frame 361";
        case Refuse::NO_FEATURE: return "function missing in the car";
        case Refuse::NO_260: return "no frame 260";
        case Refuse::STALE_260: return "stale frame 260";
        case Refuse::BAD_LEN_260: return "wrong length 260";
        case Refuse::INVALID_260: return "260 in a state not seen in captures";
    }
    return "?";
}

void fieldMask(uint8_t id, uint8_t mask[5]) {
    for (int i = 0; i < 5; i++) mask[i] = 0;
    const SettingDef* def = find(id);
    if (!def || def->kind != Kind::SENDABLE) return;
    switch (id) {
        case 0x01: mask[3] = 0x80; break;   // 5.7 parking assistant
        case 0x02: mask[3] = 0x20; break;   // 5.5 rear wiper in reverse
        case 0x06: mask[2] = 0xE0; break;   // 4.7 + duration 4.6-4.5
        case 0x07: mask[2] = 0x0E; break;   // 4.3 + duration 4.2-4.1
        case 0x09: mask[0] = 0x03; break;   // 2.1-2.0 sound (validity bit 2.2 must already be 1)
        case 0x0F: mask[1] = 0x40; break;   // 3.6 unlocking selectivity
        default: break;
    }
}

bool stateOf(const RawFrame& f260, uint8_t id, uint8_t& value) {
    if (!full260(f260)) return false;
    const uint8_t B2 = f260.d[1], B3 = f260.d[2], B4 = f260.d[3], B5 = f260.d[4], B7 = f260.d[6];
    switch (id) {
        case 0x01: value = (B5 & 0x80) ? 1 : 0; return true;
        case 0x02: value = (B5 & 0x20) ? 1 : 0; return true;
        case 0x06: value = (B4 & 0x80) ? (uint8_t)(((B4 >> 5) & 3) + 1) : 0; return true;   // 1 = 15 s, 2 = 30 s, 3 = 60 s
        case 0x07: value = (B4 & 0x08) ? (uint8_t)(((B4 >> 1) & 3) + 1) : 0; return true;
        case 0x09: value = (uint8_t)((B2 & 3) + 1); return true;                            // command 1-4 = field 0-3
        case 0x0F: value = (B3 & 0x40) ? 0 : 1; return true;                                // inverted: 1 = all doors
        case 0x15: value = (B7 & 0x10) ? 1 : 0; return true;                                // state at 7.4
        case 0x18: value = (B4 & 0x01) ? 1 : 0; return true;
        case 0x1A: value = (B3 & 0x04) ? 1 : 0; return true;
        default: return false;
    }
}

bool accepted(const RawFrame& f260, uint8_t id, uint8_t value) {
    const SettingDef* def = find(id);
    if (!def || def->kind != Kind::SENDABLE) return false;
    uint8_t now = 0;
    return stateOf(f260, id, now) && now == value;
}

Refuse build15B(const RawFrame& f260, const RawFrame& f361, uint32_t nowMs, uint8_t id, uint8_t value,
                uint32_t enabledMask, uint8_t out[8]) {
    for (int i = 0; i < 8; i++) out[i] = 0;
    const SettingDef* def = find(id);
    if (!def) return Refuse::UNKNOWN_ID;
    if (def->kind == Kind::NO_FRAME) return Refuse::NO_FRAME_FOR_ID;
    if (def->kind == Kind::KNOWN_BLOCKED) return Refuse::BLOCKED_OTHER_FRAME;
    if (def->kind != Kind::SENDABLE) return Refuse::FIXED_REFUSE;
    if (id >= 32 || !(enabledMask & (1UL << id))) return Refuse::DISABLED_BY_MASK;
    if (value < def->minValue || value > def->maxValue) return Refuse::BAD_VALUE;
    // Whether the car has this feature: bit of frame 361 (BSI, every 500 ms). No bit assigned: no condition.
    if (def->featByte != 0) {
        if (!f361.seen || f361.pasted || f361.len < def->featByte) return Refuse::NO_361;
        if (!(f361.d[def->featByte - 1] & def->featMask)) return Refuse::NO_FEATURE;
    }
    if (!f260.seen || f260.pasted) return Refuse::NO_260;   // frame pasted from the panel = same as no frame
    if (!fresh(f260, nowMs, FRESH_260_MS)) return Refuse::STALE_260;
    if (f260.len != 7) return Refuse::BAD_LEN_260;
    // A 260 state never seen in any recording: configuration validity bit 2.2 = 0, byte 7 != 0,
    // bits 6.1-6.0 != 0 (in frame 15B they mean something different than in 260, settings-map.md).
    if (!(f260.d[1] & 0x04) || f260.d[6] != 0 || (f260.d[5] & 0x03)) return Refuse::INVALID_260;

    uint8_t b2 = f260.d[1], b3 = f260.d[2], b4 = f260.d[3], b5 = f260.d[4];
    switch (id) {
        case 0x01: b5 = (uint8_t)((b5 & 0x7F) | (value ? 0x80 : 0)); break;
        case 0x02: b5 = (uint8_t)((b5 & 0xDF) | (value ? 0x20 : 0)); break;
        case 0x06:   // 0 = switch off (duration kept), 1-3 = switch on with duration 15 / 30 / 60 s
            if (value == 0) b4 &= 0x7F;
            else b4 = (uint8_t)((b4 & 0x1F) | 0x80 | ((value - 1) << 5));
            break;
        case 0x07:
            if (value == 0) b4 &= 0xF7;
            else b4 = (uint8_t)((b4 & 0xF1) | 0x08 | ((value - 1) << 1));
            break;
        case 0x09: b2 = (uint8_t)((b2 & 0xFC) | (value - 1)); break;        // bit 2.2 is already 1 (condition above)
        case 0x0F: b3 = (uint8_t)((b3 & 0xBF) | (value ? 0 : 0x40)); break;  // 1 = all doors = bit 3.6 cleared
        default: return Refuse::FIXED_REFUSE;   // SENDABLE item without a formula: should not happen
    }
    out[0] = 0x00;
    out[1] = b2;
    out[2] = b3;
    out[3] = b4;
    out[4] = b5;
    out[5] = (uint8_t)(f260.d[5] & 0xF4);   // without 6.3 (tyre calibration request) and 6.1-6.0
    out[6] = 0x00;
    out[7] = 0x00;
    return Refuse::OK;
}

bool isButton(uint8_t id) {
    const SettingDef* d = find(id);
    return d && d->kind == Kind::KNOWN_BLOCKED;
}

bool buttonState(const CarState& car, uint32_t nowMs, uint8_t id, uint8_t& value) {
    const RawFrame& l = car.f227;   // button LEDs (BSI, every 500 ms)
    switch (id) {
        case 0x05:   // as psa::decodeSettings: bit "radar" = 227 byte 1 bit 6
            if (!l.seen || l.pasted || l.len < 3 || !fresh(l, nowMs, FRESH_227_MS)) return false;
            value = (l.d[0] & 0x40) ? 1 : 0;
            return true;
        case 0x0C:   // as psa::decodeSettings: bit "blind spot" = 227 byte 3 bit 0
            if (!l.seen || l.pasted || l.len < 3 || !fresh(l, nowMs, FRESH_227_MS)) return false;
            value = (l.d[2] & 0x01) ? 1 : 0;
            return true;
        default: return false;
    }
}

Refuse buildButton(const CarState& car, uint32_t nowMs, uint8_t id, uint8_t value, uint32_t enabledMask,
                   uint8_t out[8]) {
    for (int i = 0; i < 8; i++) out[i] = 0;
    const SettingDef* def = find(id);
    if (!def) return Refuse::UNKNOWN_ID;
    if (def->kind != Kind::KNOWN_BLOCKED) return Refuse::FIXED_REFUSE;
    if (id >= 32 || !(enabledMask & (1UL << id))) return Refuse::DISABLED_BY_MASK;
    if (value < def->minValue || value > def->maxValue) return Refuse::BAD_VALUE;
    uint8_t st = 0;
    if (!buttonState(car, nowMs, id, st)) return Refuse::NO_STATE;
    // Pattern recorded from the box: 00 7F FF xx FF 00 10 00 with one bit in byte 4.
    uint8_t b4 = 0x00;
    switch (id) {
        case 0x05: b4 = 0x04; break;   // 4.2 parking sensors button
        case 0x0C: b4 = 0x20; break;   // 4.5 blind spot button
        default: return Refuse::FIXED_REFUSE;
    }
    out[0] = 0x00;
    out[1] = 0x7F;
    out[2] = 0xFF;
    out[3] = b4;
    out[4] = 0xFF;
    out[5] = 0x00;
    out[6] = 0x10;
    out[7] = 0x00;
    return Refuse::OK;
}

}  // namespace vset
