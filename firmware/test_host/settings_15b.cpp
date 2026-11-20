// Host test: the vehicle settings table and the pure logic of frame 15B (lib/car_psa_aee2010/vehicle_settings.h)
// (table as of 17.11.2026, sources in radios/xp-simple-soft/provenance.md).
//
// TRANSMITS NOTHING: only checks which bytes the build15B function would compute.
//
//   - vectors T1-T17 (frame layout: vehicles/peugeot-508-sw-2015/settings-map.md; T1, T2 = measurement of 11.11.2026; the rest computed);
//     T13 (80 0E) and T15 (80 04) have been in group D since 17.11.2026: a refusal is expected, not a frame;
//   - property: for every sendable item, every value and 10 000 random 260 frames
//     (generator with a fixed seed) frame 15B differs from 260 only in the bits of fieldMask(id);
//   - closure: 15B taken as a new 260 gives accepted() = true and every change can be undone;
//   - refusals: mask, groups D / E, items with frame 1A9, bit of frame 361 ("function not present in the car"),
//     260 stale / bad length / state outside the captures, value out of range;
//   - item table and lines of the "vset" command (Decoder class) for the car state of 17.11.2026.
//
// Run: bash tools/run_host_tests.sh (README.md, section on tests on recordings).
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "can_ids.h"
#include "car_state.h"
#include "decoder.h"
#include "vehicle_settings.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

static RawFrame raw(std::initializer_list<uint8_t> d, uint32_t ms) {
    RawFrame r;
    r.len = (uint8_t)d.size();
    uint8_t i = 0;
    for (uint8_t b : d) r.d[i++] = b;
    r.seen = true;
    r.ms = ms;
    return r;
}

static const uint32_t ALL = 0xFFFFFFFFUL;
static const uint32_t NOW = 10000;

// 361 from the car (11.11 and 17.11.2026) and a version with bit 1.0 set (daytime running lights in the menu).
static RawFrame car361() { return raw({0xA0, 0x66, 0xC8, 0x0A, 0x00, 0x00}, NOW); }
static RawFrame all361() { return raw({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, NOW); }

static bool allZero(const uint8_t a[8]) {
    for (int i = 0; i < 8; i++)
        if (a[i]) return false;
    return true;
}

static const char* kindName(vset::Kind k) {
    switch (k) {
        case vset::Kind::SENDABLE: return "SENDABLE";
        case vset::Kind::FIXED_REFUSE: return "FIXED_REFUSE";
        case vset::Kind::KNOWN_BLOCKED: return "KNOWN_BLOCKED";
        case vset::Kind::NO_FRAME: return "NO_FRAME";
    }
    return "?";
}

struct TVec {
    const char* name;
    RawFrame f260;
    RawFrame f361;
    uint8_t id, value;
    uint8_t want[8];
};

static void testVectors() {
    std::printf("--- vectors T1-T17 (settings map, item 7.3) ---\n");
    const RawFrame a = raw({0xA4, 0x34, 0x97, 0xBA, 0x55, 0x40, 0x00}, NOW);   // 260 from 11.11.2026
    const RawFrame b = raw({0xA4, 0x34, 0x97, 0xBA, 0xD5, 0x40, 0x00}, NOW);
    const RawFrame c = raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}, NOW);   // 260 from 17.11.2026
    const TVec V[] = {
        {"T1 measured: 80 01 01", a, car361(), 0x01, 1, {0x00, 0x34, 0x97, 0xBA, 0xD5, 0x40, 0x00, 0x00}},
        {"T2 measured: 80 01 00", b, car361(), 0x01, 0, {0x00, 0x34, 0x97, 0xBA, 0x55, 0x40, 0x00, 0x00}},
        {"T3 80 01 00 on 260 of 17.11", c, car361(), 0x01, 0, {0x00, 0x1C, 0x8F, 0xBB, 0x45, 0x50, 0x00, 0x00}},
        {"T4 80 01 01 (state unchanged)", c, car361(), 0x01, 1, {0x00, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00, 0x00}},
        {"T5 80 02 01 wiper", c, car361(), 0x02, 1, {0x00, 0x1C, 0x8F, 0xBB, 0xE5, 0x50, 0x00, 0x00}},
        {"T6 80 06 00 FMH off", c, car361(), 0x06, 0, {0x00, 0x1C, 0x8F, 0x3B, 0xC5, 0x50, 0x00, 0x00}},
        {"T7 80 06 01 FMH 15 s", c, car361(), 0x06, 1, {0x00, 0x1C, 0x8F, 0x9B, 0xC5, 0x50, 0x00, 0x00}},
        {"T8 80 06 03 FMH 60 s", c, car361(), 0x06, 3, {0x00, 0x1C, 0x8F, 0xDB, 0xC5, 0x50, 0x00, 0x00}},
        {"T9 80 07 00 welcome off", c, car361(), 0x07, 0, {0x00, 0x1C, 0x8F, 0xB3, 0xC5, 0x50, 0x00, 0x00}},
        {"T10 80 07 03 welcome 60 s", c, car361(), 0x07, 3, {0x00, 0x1C, 0x8F, 0xBD, 0xC5, 0x50, 0x00, 0x00}},
        {"T12 80 0F 00 driver (byte 1 = 00)", c, car361(), 0x0F, 0, {0x00, 0x1C, 0xCF, 0xBB, 0xC5, 0x50, 0x00, 0x00}},
        {"T14 80 09 02 sound (byte 1 = 00)", c, car361(), 0x09, 2, {0x00, 0x1D, 0x8F, 0xBB, 0xC5, 0x50, 0x00, 0x00}},
    };
    for (const TVec& v : V) {
        uint8_t out[8];
        vset::Refuse r = vset::build15B(v.f260, v.f361, NOW + 10, v.id, v.value, ALL, out);
        char what[200];
        std::snprintf(what, sizeof(what), "%s -> %s: %02X %02X %02X %02X %02X %02X %02X %02X", v.name,
                      vset::refuseName(r), out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7]);
        check(r == vset::Refuse::OK && std::memcmp(out, v.want, 8) == 0, what);
    }
    uint8_t out[8];
    // T11, T13, T15: 80 08, 0E, 04 were removed from the table on 19.11.2026 (the radio never sent them).
    check(vset::build15B(c, all361(), NOW, 0x08, 0, ALL, out) == vset::Refuse::UNKNOWN_ID && allZero(out) &&
              vset::build15B(c, all361(), NOW, 0x0E, 0, ALL, out) == vset::Refuse::UNKNOWN_ID && allZero(out) &&
              vset::build15B(c, all361(), NOW, 0x04, 4, ALL, out) == vset::Refuse::UNKNOWN_ID && allZero(out),
          "T11, T13, T15 80 08, 0E, 04: not in the table, unknown number, no frame");
    // T16: 260 older than 1.5 s or missing.
    RawFrame none;
    check(vset::build15B(none, car361(), NOW, 0x01, 1, ALL, out) == vset::Refuse::NO_260 && allZero(out),
          "T16 no 260: no frame");
    check(vset::build15B(a, car361(), NOW + 1500, 0x01, 1, ALL, out) == vset::Refuse::STALE_260 && allZero(out),
          "T16 260 from 1500 ms ago: no frame");
    check(vset::build15B(a, car361(), NOW + 1499, 0x01, 1, ALL, out) == vset::Refuse::OK, "260 from 1499 ms ago: frame");
    // T17: items without a 15B frame.
    bool ok = true;
    for (uint8_t id : {0x10, 0x14, 0x16, 0x12, 0x19, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x03, 0x0D})
        for (uint8_t v = 0; v <= 1; v++)
            if (vset::build15B(c, all361(), NOW, id, v, ALL, out) != vset::Refuse::NO_FRAME_FOR_ID || !allZero(out)) ok = false;
    check(ok, "T17 80 10, 14, 16, 12, 19, 1B-1F, 03, 0D: no known frame, no frame");
    ok = true;
    for (uint8_t id : {0x05, 0x0C})
        for (uint8_t v = 0; v <= 1; v++)
            if (vset::build15B(c, all361(), NOW, id, v, ALL, out) != vset::Refuse::BLOCKED_OTHER_FRAME || !allZero(out)) ok = false;
    check(ok, "T17 80 05, 0C: button in frame 1A9, build15B builds no frame");
    ok = true;
    for (uint8_t id : {0x18, 0x1A, 0x15, 0x0A, 0x0B})
        if (vset::build15B(c, all361(), NOW, id, 1, ALL, out) != vset::Refuse::FIXED_REFUSE || !allZero(out)) ok = false;
    check(ok, "group D (80 18, 1A, 15, 0A, 0B): fixed refusal, no frame");
    ok = true;
    for (int id = 0; id < 256; id++)
        if (!vset::find((uint8_t)id) &&
            (vset::build15B(c, all361(), NOW, (uint8_t)id, 1, ALL, out) != vset::Refuse::UNKNOWN_ID || !allZero(out)))
            ok = false;
    check(ok, "every number outside the table (also 00, 11, 13, 17, 20-FF): unknown number, no frame");
}

static void testRefusals() {
    std::printf("--- refusals ---\n");
    const RawFrame c = raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}, NOW);
    uint8_t out[8];
    check(vset::ENABLED_DEFAULT == 0x00000002UL, "default mask = only the bit of item 01 (0x00000002)");
    check(vset::build15B(c, car361(), NOW, 0x01, 0, vset::ENABLED_DEFAULT, out) == vset::Refuse::OK,
          "default mask: 80 01 sendable");
    bool ok = true;
    for (uint8_t id : {0x02, 0x06, 0x07, 0x09, 0x0F})
        if (vset::build15B(c, all361(), NOW, id, 1, vset::ENABLED_DEFAULT, out) != vset::Refuse::DISABLED_BY_MASK || !allZero(out))
            ok = false;
    check(ok, "default mask: 80 02, 06, 07, 09, 0F disabled in mask");
    check(vset::build15B(c, car361(), NOW, 0x01, 0, 0, out) == vset::Refuse::DISABLED_BY_MASK, "mask 0: 80 01 disabled");
    // Groups D and E cannot be enabled by the mask.
    ok = true;
    for (uint8_t i = 0; i < vset::count(); i++) {
        const vset::SettingDef& d = vset::at(i);
        if (d.kind == vset::Kind::SENDABLE) continue;
        vset::Refuse r = vset::build15B(c, all361(), NOW, d.id, d.minValue, ALL, out);
        if (r == vset::Refuse::OK || !allZero(out)) ok = false;
    }
    check(ok, "items other than SENDABLE: refused also with the full mask");
    // Length of 260.
    check(vset::build15B(raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50}, NOW), car361(), NOW, 0x01, 0, ALL, out) ==
              vset::Refuse::BAD_LEN_260, "260 with length 6: refused");
    check(vset::build15B(raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00, 0x00}, NOW), car361(), NOW, 0x01, 0, ALL, out) ==
              vset::Refuse::BAD_LEN_260, "260 with length 8: refused");
    // 260 state outside the captures.
    check(vset::build15B(raw({0xA4, 0x18, 0x8F, 0xBB, 0xC5, 0x50, 0x00}, NOW), car361(), NOW, 0x01, 0, ALL, out) ==
              vset::Refuse::INVALID_260, "260 with bit 2.2 = 0: refused");
    check(vset::build15B(raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x10}, NOW), car361(), NOW, 0x01, 0, ALL, out) ==
              vset::Refuse::INVALID_260, "260 with byte 7 = 10: refused");
    check(vset::build15B(raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x01}, NOW), car361(), NOW, 0x01, 0, ALL, out) ==
              vset::Refuse::INVALID_260, "260 with byte 7 = 01: refused");
    check(vset::build15B(raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x51, 0x00}, NOW), car361(), NOW, 0x01, 0, ALL, out) ==
              vset::Refuse::INVALID_260, "260 with bit 6.0 = 1: refused");
    check(vset::build15B(raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x52, 0x00}, NOW), car361(), NOW, 0x01, 0, ALL, out) ==
              vset::Refuse::INVALID_260, "260 with bit 6.1 = 1: refused");
    // Bit 6.3 in 260 (tyre calibration state) is not a reason for refusal, but in 15B it is always 0.
    check(vset::build15B(raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x5C, 0x00}, NOW), car361(), NOW, 0x01, 0, ALL, out) ==
              vset::Refuse::OK && out[5] == 0x54, "260 with bit 6.3 = 1: frame with byte 6 = 54 (bit 6.3 cleared, 6.2 copied)");
    // Value out of range.
    check(vset::build15B(c, car361(), NOW, 0x01, 2, ALL, out) == vset::Refuse::BAD_VALUE, "80 01 02: value out of range");
    check(vset::build15B(c, car361(), NOW, 0x06, 4, ALL, out) == vset::Refuse::BAD_VALUE, "80 06 04: value out of range");
    check(vset::build15B(c, car361(), NOW, 0x07, 0xFF, ALL, out) == vset::Refuse::BAD_VALUE, "80 07 FF: value out of range");
    check(vset::build15B(c, car361(), NOW, 0x09, 0, ALL, out) == vset::Refuse::BAD_VALUE, "80 09 00: value out of range (the radio sends 1-4)");
    check(vset::build15B(c, car361(), NOW, 0x09, 5, ALL, out) == vset::Refuse::BAD_VALUE, "80 09 05: value out of range");
    // Condition from frame 361.
    RawFrame none;
    check(vset::build15B(c, none, NOW, 0x01, 0, ALL, out) == vset::Refuse::OK, "80 01 without frame 361: frame (item without a 361 condition)");
    ok = true;
    for (uint8_t id : {0x02, 0x06, 0x07, 0x09, 0x0F})
        if (vset::build15B(c, none, NOW, id, 1, ALL, out) != vset::Refuse::NO_361 || !allZero(out)) ok = false;
    check(ok, "80 02, 06, 07, 09, 0F without frame 361: refused (no frame 361)");
    check(vset::build15B(c, raw({0xA0, 0x66}, NOW), NOW, 0x09, 1, ALL, out) == vset::Refuse::NO_361,
          "80 09 with 361 of length 2 (bit in byte 3): no frame 361");
    check(vset::build15B(c, raw({0xA0, 0x66}, NOW), NOW, 0x02, 1, ALL, out) == vset::Refuse::OK,
          "80 02 with 361 of length 2 (bit in byte 2): frame");
    struct Feat { uint8_t id; uint8_t byte; uint8_t mask; const char* what; };
    const Feat F[] = {
        {0x02, 2, 0x40, "80 02: 361 bit 2.6"}, {0x06, 2, 0x20, "80 06: 361 bit 2.5"}, {0x07, 1, 0x20, "80 07: 361 bit 1.5"},
        {0x09, 3, 0x08, "80 09: 361 bit 3.3"}, {0x0F, 2, 0x02, "80 0F: 361 bit 2.1"},
    };
    for (const Feat& f : F) {
        RawFrame only;                    // only this bit set: sendable
        only.seen = true;
        only.len = 6;
        only.d[f.byte - 1] = f.mask;
        RawFrame without = all361();      // all except this one: function not present
        without.d[f.byte - 1] = (uint8_t)~f.mask;
        bool good = vset::build15B(c, only, NOW, f.id, 1, ALL, out) == vset::Refuse::OK &&
                    vset::build15B(c, without, NOW, f.id, 1, ALL, out) == vset::Refuse::NO_FEATURE && allZero(out);
        char what[120];
        std::snprintf(what, sizeof(what), "%s = 1 frame, = 0 function missing in the car", f.what);
        check(good, what);
    }
    // 361 from the car: which items the car has.
    ok = true;
    for (uint8_t id : {0x02, 0x06, 0x07, 0x09, 0x0F})
        if (vset::build15B(c, car361(), NOW, id, 1, ALL, out) != vset::Refuse::OK) ok = false;
    check(ok, "361 from the car (A0 66 C8 0A 00 00): 80 02, 06, 07, 09, 0F sendable with the full mask");
    // Millisecond counter wrapping through zero.
    RawFrame late = raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}, 0xFFFFFF00UL);
    check(vset::build15B(late, car361(), 0x00000100UL, 0x01, 0, ALL, out) == vset::Refuse::OK,
          "260 from 512 ms ago with the counter wrapping through zero: frame");
    check(vset::build15B(late, car361(), 0x00000600UL, 0x01, 0, ALL, out) == vset::Refuse::STALE_260,
          "260 from 1792 ms ago with the counter wrapping through zero: stale");
}

static void testAccepted() {
    std::printf("--- accepted() and stateOf() ---\n");
    const RawFrame c = raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}, NOW);   // 17.11: 3.6 = 0, FMH 30 s, welcome 30 s
    uint8_t v = 0xEE;
    check(vset::accepted(c, 0x01, 1) && !vset::accepted(c, 0x01, 0), "80 01: state 1 (260 5.7 = 1)");
    check(vset::accepted(c, 0x0F, 1) && !vset::accepted(c, 0x0F, 0), "80 0F inverted: 260 3.6 = 0 -> state 1 (all doors)");
    const RawFrame drv = raw({0xA4, 0x1C, 0xCF, 0xBB, 0xC5, 0x50, 0x00}, NOW);
    check(vset::accepted(drv, 0x0F, 0) && !vset::accepted(drv, 0x0F, 1), "80 0F inverted: 260 3.6 = 1 -> state 0 (driver)");
    check(vset::accepted(c, 0x06, 2) && !vset::accepted(c, 0x06, 0) && !vset::accepted(c, 0x06, 1), "80 06: state 2 (30 s)");
    const RawFrame off = raw({0xA4, 0x1C, 0x8F, 0x3B, 0xC5, 0x50, 0x00}, NOW);   // 4.7 = 0, the 30 s time stays
    check(vset::accepted(off, 0x06, 0) && !vset::accepted(off, 0x06, 2), "80 06 with v = 0: only bit 4.7 counts (the time stays)");
    check(vset::accepted(c, 0x07, 2) && vset::accepted(c, 0x09, 1) && vset::accepted(c, 0x02, 0),
          "state 17.11: 80 07 = 2, 80 09 = 1, 80 02 = 0");
    check(!vset::accepted(c, 0x0A, 1) && !vset::accepted(c, 0x15, 1) && !vset::accepted(c, 0x05, 0) && !vset::accepted(c, 0x77, 0),
          "accepted() for items other than SENDABLE always false");
    check(vset::stateOf(c, 0x18, v) && v == 1 && vset::stateOf(c, 0x1A, v) && v == 1 && vset::stateOf(c, 0x15, v) && v == 0,
          "stateOf 17.11: automatic lights 1, adaptive 1, fatigue 0");
    check(!vset::stateOf(c, 0x0D, v) && !vset::stateOf(c, 0x05, v) && !vset::stateOf(c, 0x0B, v), "stateOf: items without a field in 260 -> false");
    RawFrame none;
    check(!vset::stateOf(none, 0x01, v) && !vset::accepted(none, 0x01, 0), "without frame 260: stateOf and accepted false");
    check(!vset::accepted(raw({0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50}, NOW), 0x01, 1), "260 with length 6: accepted false");
    uint8_t m[5];
    vset::fieldMask(0x0A, m);
    check(!(m[0] | m[1] | m[2] | m[3] | m[4]), "fieldMask of items of group D: zeros only");
}

// Generator with a fixed seed (xorshift32): the same frames on every run.
static uint32_t rngState = 0x508C0DE5UL;
static uint32_t rnd() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

static void testProperty() {
    std::printf("--- property and closure (10 000 random 260 per item) ---\n");
    const RawFrame f361 = all361();
    for (uint8_t i = 0; i < vset::count(); i++) {
        const vset::SettingDef& d = vset::at(i);
        if (d.kind != vset::Kind::SENDABLE) continue;
        uint8_t mask[5];
        vset::fieldMask(d.id, mask);
        long built = 0, badField = 0, badFixed = 0, badAccept = 0, badUndo = 0, refused = 0, noUndo = 0;
        for (int n = 0; n < 10000; n++) {
            RawFrame f;
            f.seen = true;
            f.len = 7;
            f.ms = NOW;
            uint32_t r1 = rnd(), r2 = rnd();
            f.d[0] = (uint8_t)r1;
            f.d[1] = (uint8_t)((r1 >> 8) | 0x04);      // 2.2 = 1
            f.d[2] = (uint8_t)(r1 >> 16);
            f.d[3] = (uint8_t)(r1 >> 24);
            f.d[4] = (uint8_t)r2;
            f.d[5] = (uint8_t)((r2 >> 8) & 0xFC);      // 6.1-6.0 = 0
            f.d[6] = 0;                                // byte 7 = 0
            uint8_t before = 0;
            vset::stateOf(f, d.id, before);
            for (uint8_t v = d.minValue; v <= d.maxValue; v++) {
                uint8_t out[8];
                if (vset::build15B(f, f361, NOW, d.id, v, ALL, out) != vset::Refuse::OK) {
                    refused++;
                    continue;
                }
                built++;
                // Difference against 260 (byte 6 with mask F4) only in the bits of the field.
                for (int k = 0; k < 5; k++) {
                    uint8_t src = k == 4 ? (uint8_t)(f.d[5] & 0xF4) : f.d[k + 1];
                    if ((out[k + 1] ^ src) & ~mask[k]) badField++;
                }
                if (out[0] != 0 || out[6] != 0 || out[7] != 0 || (out[5] & 0x08)) badFixed++;
                // Closure: 15B as a new 260 (bytes 1 and 7 unchanged).
                RawFrame g = f;
                for (int k = 1; k <= 5; k++) g.d[k] = out[k];
                if (!vset::accepted(g, d.id, v)) badAccept++;
                // Undo: a command with the value from before the change returns to the same state.
                // Exception counted separately: 260 with time code 3 while the function is on (80 06, 80 07;
                // the description only knows 0 = 15 s, 1 = 30 s, 2 = 60 s) gives state 4, which the radio command
                // (0-3) cannot express, so such a state cannot be restored by a command.
                if (before < d.minValue || before > d.maxValue) {
                    noUndo++;
                    continue;
                }
                uint8_t back[8];
                uint8_t after = 0xFF;
                RawFrame h = g;
                if (vset::build15B(g, f361, NOW, d.id, before, ALL, back) != vset::Refuse::OK) {
                    badUndo++;
                } else {
                    for (int k = 1; k <= 5; k++) h.d[k] = back[k];
                    if (!vset::stateOf(h, d.id, after) || after != before) badUndo++;
                }
            }
        }
        char what[220];
        std::snprintf(what, sizeof(what),
                      "80 %02X %-24s field mask %02X %02X %02X %02X %02X: %ld frames, outside the field %ld, fixed bytes %ld, "
                      "accepted %ld, undo %ld, refused %ld, state outside the command range %ld",
                      d.id, d.name, mask[0], mask[1], mask[2], mask[3], mask[4], built, badField, badFixed, badAccept,
                      badUndo, refused, noUndo);
        check(built == 10000L * (d.maxValue - d.minValue + 1) && badField == 0 && badFixed == 0 && badAccept == 0 &&
                  badUndo == 0 && refused == 0, what);
    }
}

static void testTable() {
    std::printf("--- table (design item 2.8, numbers and names 19.11.2026) ---\n");
    int n[4] = {0, 0, 0, 0};
    bool sorted = true;
    for (uint8_t i = 0; i < vset::count(); i++) {
        const vset::SettingDef& d = vset::at(i);
        n[(int)d.kind]++;
        if (i > 0 && vset::at((uint8_t)(i - 1)).id >= d.id) sorted = false;
        if (vset::find(d.id) != &d) sorted = false;
        std::printf("  80 %02X %-26s %-13s %u-%u 361:%u/%02X %s\n", d.id, d.name, kindName(d.kind), d.minValue, d.maxValue,
                    d.featByte, d.featMask, d.note);
    }
    char what[120];
    std::snprintf(what, sizeof(what), "%u items: SENDABLE %d, FIXED_REFUSE %d, KNOWN_BLOCKED %d, NO_FRAME %d", vset::count(),
                  n[0], n[1], n[2], n[3]);
    check(vset::count() == 25 && n[0] == 6 && n[1] == 5 && n[2] == 2 && n[3] == 12 && sorted, what);
}

static void testSettingCommand() {
    std::printf("--- command vset (Decoder), car state of 17.11.2026 ---\n");
    Decoder dec;
    DecoderConfig cfg;
    dec.begin(cfg, nullptr);
    char row[200];
    dec.formatSettingsHeader(1000, row, sizeof(row));
    std::printf("%s", row);
    dec.formatSettingRow(0, 1000, row, sizeof(row));
    std::printf("%s", row);
    CanFrame f;
    f.id = psa::ID_PERSONALIZATION;
    f.len = 7;
    const uint8_t d260[7] = {0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00};
    std::memcpy(f.data, d260, 7);
    dec.onCanFrame(f, 1000, true);
    f.id = psa::ID_OPTIONS;
    f.len = 6;
    const uint8_t d361[6] = {0xA0, 0x66, 0xC8, 0x0A, 0x00, 0x00};
    std::memcpy(f.data, d361, 6);
    dec.onCanFrame(f, 1010, true);
    dec.formatSettingsHeader(1212, row, sizeof(row));
    std::printf("%s", row);
    size_t longest = 0;
    for (uint8_t i = 0; i < dec.settingsCount(); i++) {
        dec.formatSettingRow(i, 1212, row, sizeof(row));
        std::printf("%s", row);
        if (std::strlen(row) > longest) longest = std::strlen(row);
    }
    char what[80];
    std::snprintf(what, sizeof(what), "longest line %u characters (buffer 200)", (unsigned)longest);
    check(dec.settingsCount() == vset::count() && longest < 199, what);
    dec.formatSettingRow(0, 1000 + 1500, row, sizeof(row));
    std::printf("%s", row);
    check(std::strstr(row, "stale frame 260") != nullptr, "260 from 1500 ms ago: the row shows 'stale frame 260'");
    dec.formatSettingRow(200, 0, row, sizeof(row));
    check(row[0] == 0, "row outside the table: empty string");
}

int main() {
    testVectors();
    testRefusals();
    testAccepted();
    testProperty();
    testTable();
    testSettingCommand();
    std::printf("settings_15b: %d checks, %d errors\n", checks, failures);
    return failures ? 1 : 0;
}
