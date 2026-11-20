#include "xp_settings.h"

namespace vset {

void update38(const CarState& car, State38& s) {
    // --- 260: BSI personalisation (7 bytes, every 500 ms) ---
    const RawFrame& p = car.f260;
    const bool ok260 = p.seen && p.len >= 7;
    if (ok260) s.have260 = true;
    if (!s.have260) return;            // before the first 260 the radio gets all zeros, as from the box
    uint8_t d1 = s.d[0], d2 = s.d[1], d3 = s.d[2], d4 = s.d[3], d5 = s.d[4];

    if (ok260) {
        const uint8_t B1 = p.d[0], B2 = p.d[1], B3 = p.d[2], B4 = p.d[3], B5 = p.d[4], B7 = p.d[6];
        // DATA[1]: bit 7 rear wiper in reverse (260 5.5), bit 2 fatigue detection (260 7.4).
        // Bit 4 (boot) and bit 3 (traction control, in the box from frame 268, which this car
        // does not have) stay 0; bit 1 (instrument colour) is set by frame 217 below.
        d1 = (uint8_t)((d1 & 0x7B) | ((B5 & 0x20) ? 0x80 : 0) | ((B7 & 0x10) ? 0x04 : 0));
        // DATA[2]: bits 7-5 ambient (when 260 3.0 = 1: level from 2.5-2.3 plus 1), bit 4 parking
        // assistant (260 5.7, confirmed by measurement). Bit 3 (radar) is set by frame 227 below.
        uint8_t amb = (B3 & 0x01) ? (uint8_t)(((B2 >> 3) & 7) + 1) : 0;
        d2 = (uint8_t)((d2 & 0x0F) | ((amb & 7) << 5) | ((B5 & 0x80) ? 0x10 : 0));
        // DATA[3]: bits 7-6 "follow me home" (260 4.7 + duration 4.6-4.5), bits 5-4 welcome
        // lighting (260 4.3 + duration 4.2-4.1), bit 3 daytime running lights (260 3.1).
        uint8_t fmh = (B4 & 0x80) ? (uint8_t)(((B4 >> 5) & 3) + 1) : 0;
        uint8_t wel = (B4 & 0x08) ? (uint8_t)(((B4 >> 1) & 3) + 1) : 0;
        d3 = (uint8_t)((d3 & 0x07) | ((fmh & 3) << 6) | ((wel & 3) << 4) | ((B3 & 0x02) ? 0x08 : 0));
        // DATA[3] bits 2-1: sound (260 2.1-2.0), only when 260 2.2 = 1.
        if (B2 & 0x04) d3 = (uint8_t)((d3 & 0xF9) | ((B2 & 3) << 1));
        // DATA[4]: bit 5 welcome function (260 3.3), bit 4 door unlocking (NOT 260 3.6:
        // 1 = all doors), bits 3-0 language. Bits 7 and 6 are set by frames 227 and 3E1 below.
        uint8_t lang = radioLanguage((uint8_t)((B1 >> 2) & 0x1F));
        if (lang == 0xFF) lang = (uint8_t)(d4 & 0x0F);
        d4 = (uint8_t)((d4 & 0xC0) | ((B3 & 0x08) ? 0x20 : 0) | ((B3 & 0x40) ? 0 : 0x10) | lang);
        // Fuel consumption units from (260 1.1, 1.0, 2.7): DATA[5] bits 7-6 and DATA[3] bit 0
        // (the radio does not read this bit). Other bit combinations: no change.
        const uint8_t unitLow = (uint8_t)(B1 & 3), unitHigh = (uint8_t)(B2 >> 7);
        if (unitLow == 1 && unitHigh == 0) {            // km/L
            d3 &= 0xFE;
            d5 = (uint8_t)(d5 & 0x3F);
        } else if (unitLow == 0 && unitHigh == 0) {     // L/100 km
            d3 |= 0x01;
            d5 = (uint8_t)((d5 & 0x3F) | 0x40);
        } else if (unitLow == 3 && unitHigh == 1) {     // MPG
            d5 = (uint8_t)((d5 & 0x3F) | 0x80);
        }
        // DATA[5] bits 5-4 and 2-0 (headlamp delay, automatic lights, locking,
        // adaptive headlamps): always 0, as in the box (settings-map.md, section "XP frame `38`").
    }

    // --- 227: button LEDs (5 bytes) ---
    const RawFrame& l = car.f227;
    if (l.seen && l.len >= 3) {
        d2 = (uint8_t)((d2 & 0xF7) | ((l.d[0] & 0x40) ? 0x08 : 0));   // radar: 227 1.6
        d4 = (uint8_t)((d4 & 0x7F) | ((l.d[2] & 0x01) ? 0x80 : 0));   // blind spot: 227 3.0 (measurement)
    }

    // --- 3E1: Start/Stop from bits 4.7-4.6 (5 bytes; measurement) ---
    // 10 -> 1, 01 -> 0, others: no change. This bit follows the ignition (40 with ignition on, 80 after
    // switching off), not the button; kept for compatibility with the box (radios/xp-simple-soft/provenance.md).
    // Note: the measurement of 2026-11-18 (ECO OFF button) shows 3E1 byte 4 = 80 when Stop and Start is
    // switched off by the button and 40 when active (the same rule as psa::decodeSettings).
    const RawFrame& e = car.f3E1;
    if (e.seen && e.len >= 4) {
        const uint8_t v = (uint8_t)(e.d[3] & 0xC0);
        if (v == 0x80) d4 |= 0x40;
        else if (v == 0x40) d4 &= 0xBF;
    }

    // --- 217: instrument colour from bit 8.1 (8 bytes; in recordings byte 8 = FF in every frame) ---
    const RawFrame& c = car.f217;
    if (c.seen && c.len >= 8) d1 = (uint8_t)((d1 & 0xFD) | ((c.d[7] & 0x02) ? 0x02 : 0));

    s.d[0] = d1;
    s.d[1] = d2;
    s.d[2] = d3;
    s.d[3] = d4;
    s.d[4] = d5;
}

bool features361(const CarState& car, uint8_t out[2]) {
    const RawFrame& r = car.f361;
    if (!r.seen || r.len < 4) return false;
    const uint8_t B1 = r.d[0], B2 = r.d[1], B3 = r.d[2], B4 = r.d[3];
    // DATA[0]: bit 7 = 361 1.7 (the radio does not read it), bit 6 = 1.0 daytime running lights, bit 5 = 1.2 ambient,
    // bit 4 = 1.5 welcome lighting, bit 3 = 2.5 "follow me home", bit 2 = 2.6 rear wiper
    // in reverse, bit 1 = 3.6, bit 0 = 2.4 welcome function. Bit = 0: the radio hides the row.
    out[0] = (uint8_t)((B1 & 0x80) | ((B1 & 0x01) << 6) | ((B1 & 0x04) << 3) | ((B1 >> 1) & 0x10) |
                            ((B2 >> 2) & 0x08) | ((B2 >> 4) & 0x04) | ((B3 >> 5) & 0x02) | ((B2 >> 4) & 0x01));
    // DATA[1]: bit 7 = 361 2.1 door unlocking, bit 6 = 4.4.
    out[1] = (uint8_t)(((B2 & 0x02) << 6) | ((B4 & 0x10) << 2));
    return true;
}


bool build3C(const CarState& car, xp::Frame& out) {
    uint8_t b[2];
    if (!features361(car, b)) return false;
    out.cmd = CMD_FEATURES;
    out.len = 2;
    out.data[0] = b[0];
    out.data[1] = b[1];
    return true;
}

}  // namespace vset
