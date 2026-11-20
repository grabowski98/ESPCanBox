// Host test: frame 38 DATA[1..5] and frame 3C computed from CAN (lib/car_psa_aee2010/vehicle_settings.h).
//
// Without arguments:
//   - vectors S1-S8 (frame layout: vehicles/peugeot-508-sw-2015/settings-map.md; S1-S7 from measurement, compared with the mask
//     of bits read by the radio 9E F8 FE FF F7; S8 = car state of 17.11.2026, result of the rule) and V1-V2;
//   - too short frames and "unchanged" fields;
//   - Decoder class: start without 260 (the first 38 with zeros, the second with content), pair 38 + 3C after 81 01,
//     81 01 before 361 does not send 3C, 260 with length 6 does not change the state.
// With captures as arguments (one or more CAN + UART files): replays the capture through Decoder
// and at every 38 / 3C frame of the Simple Soft box compares its content with what Decoder computes
// at the same moment; also counts the pairs 38 + 3C in our stream after every radio command 81 01.
// For the three captures of 11.11.2026 together the expected result is 38 of 38 frames 38 and 9 of 9 frames 3C.
//
// Run: bash tools/run_host_tests.sh (README.md, section on tests on recordings).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "can_ids.h"
#include "candump.h"
#include "car_state.h"
#include "decoder.h"
#include "vehicle_settings.h"
#include "xp_protocol.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

// Bits of DATA[1..5] of frame 38 that the radio decoder reads (vehicles/peugeot-508-sw-2015/settings-map.md, XP frame 38).
static const uint8_t READ_MASK[5] = {0x9E, 0xF8, 0xFE, 0xFF, 0xF7};

static CanFrame frame(uint32_t id, std::initializer_list<uint8_t> d) {
    CanFrame f;
    f.id = id;
    f.len = (uint8_t)d.size();
    uint8_t i = 0;
    for (uint8_t b : d) f.data[i++] = b;
    return f;
}

// "A43497BA554000" -> CAN frame with the given ID; empty string = no frame (false).
static bool hexFrame(uint32_t id, const char* hex, CanFrame& f) {
    size_t n = std::strlen(hex);
    if (n == 0) return false;
    f.id = id;
    f.len = (uint8_t)(n / 2);
    for (uint8_t i = 0; i < f.len; i++) {
        char b[3] = {hex[2 * i], hex[2 * i + 1], 0};
        f.data[i] = (uint8_t)std::strtoul(b, nullptr, 16);
    }
    return true;
}

static bool sameMasked(const uint8_t got[5], const uint8_t want[5]) {
    for (int i = 0; i < 5; i++)
        if ((got[i] ^ want[i]) & READ_MASK[i]) return false;
    return true;
}

struct Vec {
    const char* name;
    const char* f260;
    const char* f227;
    const char* f3E1;
    const char* f217;
    uint8_t want[5];
    bool exact;          // true: all bits (result of the rule); false: with the radio bit mask (measurement)
};

static void testVectors() {
    static const Vec V[] = {
        {"S1 _190551 75,233 s", "A43497BA554000", "1000110000", "0000004000", "F040000000000BFF", {0x02, 0xE0, 0xA9, 0x98, 0x40}, false},
        {"S2 _191045 391,846 s", "A43497BA554000", "0000110000", "0000004000", "F080000000FF0BFF", {0x02, 0xE1, 0xA9, 0x98, 0x40}, false},
        {"S3 _191045 412,925 s (reverse)", "A43497BA554000", "0000100000", "0000004000", "F080000000000BFF", {0x02, 0xE7, 0xA9, 0x18, 0x40}, false},
        {"S4 _191045 562,912 s (parking assist)", "A43497BAD54000", "0000110000", "0000004000", "F080000000000BFF", {0x02, 0xF1, 0xA9, 0x98, 0x40}, false},
        {"S5 _191045 746,416 s (3E1 = 80)", "A43497BA554000", "0000110000", "0000008000", "F080000000FF0BFF", {0x02, 0xE1, 0xA9, 0xD8, 0x40}, false},
        {"S6 _191045 746,436 s", "A43497BA554000", "0000100000", "0000008000", "F080000000FF0BFF", {0x02, 0xE1, 0xA9, 0x58, 0x40}, false},
        {"S7 no frames: zeros only", "", "", "", "", {0x00, 0x00, 0x00, 0x00, 0x00}, true},
        {"S8 state of 17.11.2026 -> 02 90 A9 B8 40", "A41C8FBBC55000", "0000110000", "0000004000", "F080000000000BFF", {0x02, 0x90, 0xA9, 0xB8, 0x40}, true},
    };
    for (const Vec& v : V) {
        CarModel car;
        CanFrame f;
        if (hexFrame(psa::ID_PERSONALIZATION, v.f260, f)) car.onCanFrame(f, 1, true);
        if (hexFrame(psa::ID_BUTTON_LEDS, v.f227, f)) car.onCanFrame(f, 2, true);
        if (hexFrame(psa::ID_STOP_START, v.f3E1, f)) car.onCanFrame(f, 3, true);
        if (hexFrame(psa::ID_FRAME_217, v.f217, f)) car.onCanFrame(f, 4, true);
        vset::State38 s;
        vset::update38(car.state(), s);
        bool ok = v.exact ? std::memcmp(s.d, v.want, 5) == 0 : sameMasked(s.d, v.want);
        char what[160];
        std::snprintf(what, sizeof(what), "%s: DATA[1..5] = %02X %02X %02X %02X %02X (%s)", v.name, s.d[0], s.d[1],
                      s.d[2], s.d[3], s.d[4], v.exact ? "all bits" : "with the radio bit mask");
        check(ok, what);
    }

    // Frame 3C from 361.
    {
        CarModel car;
        xp::Frame f;
        f.cmd = 0;
        check(!vset::build3C(car.state(), f), "3C: without frame 361 nothing");
        car.onCanFrame(frame(0x361, {0xA0, 0x66, 0xC8}), 1, true);
        check(!vset::build3C(car.state(), f), "3C: 361 with length 3 nothing");
        car.onCanFrame(frame(0x361, {0xA0, 0x66, 0xC8, 0x0A, 0x00, 0x00}), 2, true);
        check(vset::build3C(car.state(), f) && f.cmd == 0x3C && f.len == 2 && f.data[0] == 0x9E && f.data[1] == 0x80,
              "V1: 361 = A0 66 C8 0A 00 00 -> 3C = 9E 80 (measured)");
        car.onCanFrame(frame(0x361, {0xA0, 0x60, 0xC8, 0x0A, 0x00, 0x00}), 3, true);
        check(vset::build3C(car.state(), f) && f.len == 2 && f.data[0] == 0x9E && f.data[1] == 0x00,
              "V2: 361 = A0 60 C8 0A 00 00 -> 3C = 9E 00 (rule, not measured)");
        car.onCanFrame(frame(0x361, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}), 4, true);
        check(vset::build3C(car.state(), f) && f.data[0] == 0xFF && f.data[1] == 0xC0, "3C: all bits of 361 -> FF C0");
        car.onCanFrame(frame(0x361, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}), 5, true);
        check(vset::build3C(car.state(), f) && f.data[0] == 0x00 && f.data[1] == 0x00, "3C: 361 zeros only -> 00 00");
    }
}

static void testShortAndKeep() {
    const uint8_t S2[5] = {0x02, 0xE0, 0xA9, 0x98, 0x40};   // result of the rule for the state of 11.11 (no mask)
    CarModel car;
    vset::State38 s;
    car.onCanFrame(frame(0x260, {0xA4, 0x34, 0x97, 0xBA, 0x55, 0x40}), 1, true);
    vset::update38(car.state(), s);
    const uint8_t zero[5] = {0, 0, 0, 0, 0};
    check(std::memcmp(s.d, zero, 5) == 0, "260 with length 6 as the first: the state stays zero");
    car.onCanFrame(frame(0x260, {0xA4, 0x34, 0x97, 0xBA, 0x55, 0x40, 0x00}), 2, true);
    car.onCanFrame(frame(0x227, {0x00, 0x00, 0x11, 0x00, 0x00}), 2, true);
    car.onCanFrame(frame(0x3E1, {0x00, 0x00, 0x00, 0x40, 0x00}), 2, true);
    car.onCanFrame(frame(0x217, {0xF0, 0x80, 0, 0, 0, 0, 0x0B, 0xFF}), 2, true);
    vset::update38(car.state(), s);
    check(std::memcmp(s.d, S2, 5) == 0, "state of 11.11: 02 E0 A9 98 40");
    car.onCanFrame(frame(0x260, {0xA4, 0x34, 0x97, 0xBA, 0xD5, 0x40}), 3, true);   // change of 5.7, but 6 bytes
    vset::update38(car.state(), s);
    check(std::memcmp(s.d, S2, 5) == 0, "260 with length 6 does not change the state");
    car.onCanFrame(frame(0x227, {0x40, 0x00}), 4, true);
    vset::update38(car.state(), s);
    check(std::memcmp(s.d, S2, 5) == 0, "227 with length 2 does not change the state");
    car.onCanFrame(frame(0x3E1, {0x00, 0x00, 0x00}), 5, true);
    vset::update38(car.state(), s);
    check(std::memcmp(s.d, S2, 5) == 0, "3E1 with length 3 does not change the state");
    car.onCanFrame(frame(0x217, {0xF0, 0x80, 0, 0, 0, 0, 0x0B}), 6, true);
    vset::update38(car.state(), s);
    check(std::memcmp(s.d, S2, 5) == 0, "217 with length 7 does not change the state");

    // Start/Stop from 3E1 4.7-4.6: 10 -> 1, 01 -> 0, 00 and 11 unchanged.
    car.onCanFrame(frame(0x3E1, {0, 0, 0, 0x80, 0}), 7, true);
    vset::update38(car.state(), s);
    bool on = (s.d[3] & 0x40) != 0;
    car.onCanFrame(frame(0x3E1, {0, 0, 0, 0x00, 0}), 8, true);
    vset::update38(car.state(), s);
    bool keep1 = (s.d[3] & 0x40) != 0;
    car.onCanFrame(frame(0x3E1, {0, 0, 0, 0xC0, 0}), 9, true);
    vset::update38(car.state(), s);
    bool keep2 = (s.d[3] & 0x40) != 0;
    car.onCanFrame(frame(0x3E1, {0, 0, 0, 0x40, 0}), 10, true);
    vset::update38(car.state(), s);
    check(on && keep1 && keep2 && (s.d[3] & 0x40) == 0, "Start/Stop: 3E1 80 -> 1, 00 and C0 unchanged, 40 -> 0");

    // Traction control (DATA[1] bit 3) and boot (bit 4): always 0, also with 260 3.4 = 1.
    check((s.d[0] & 0x18) == 0, "DATA[1] bits 4 and 3 (boot, traction control) = 0");
    // Language outside the table (code 7) and units outside the table (1.1-1.0 = 10): fields unchanged.
    car.onCanFrame(frame(0x260, {0x1E, 0x34, 0x97, 0xBA, 0x55, 0x40, 0x00}), 11, true);
    vset::update38(car.state(), s);
    check((s.d[3] & 0x0F) == 0x08 && (s.d[4] & 0xC0) == 0x40 && (s.d[2] & 0x01) == 0x01,
          "language and units outside the table: the previous ones stay (Polish, L/100 km)");
    // Units: km/L (1.1-1.0 = 01, 2.7 = 0) and MPG (11, 2.7 = 1). Language code 1 (English per the box firmware) is
    // not in the table since 19.11.2026 (only Polish is measured): the language field keeps Polish (8).
    car.onCanFrame(frame(0x260, {0x05, 0x34, 0x97, 0xBA, 0x55, 0x40, 0x00}), 12, true);
    vset::update38(car.state(), s);
    bool kml = (s.d[4] & 0xC0) == 0x00 && (s.d[2] & 0x01) == 0 && (s.d[3] & 0x0F) == 0x08;
    car.onCanFrame(frame(0x260, {0x07, 0xB4, 0x97, 0xBA, 0x55, 0x40, 0x00}), 13, true);
    vset::update38(car.state(), s);
    check(kml && (s.d[4] & 0xC0) == 0x80, "units: km/L -> 0, MPG -> 2; language code 1 not in the table: Polish stays");
    // Multi-bit fields off and single bits: 260 = all zeros except 2.2 (sound valid) and 3.6.
    car.onCanFrame(frame(0x260, {0x24, 0x07, 0x40, 0x00, 0x20, 0x00, 0x10}), 14, true);
    vset::update38(car.state(), s);
    // DATA[1]: wiper (5.5) + fatigue (7.4) + dial colour (217) = 86; DATA[2]: ambient 0,
    // assistant 0 = 00; DATA[3]: FMH 0, welcome 0, daytime 0, sound 3 (bits 2-1), bit 0 = 1 -> 07;
    // DATA[4]: 227 3.0 stays (80), Start/Stop 0, greeting 0, doors: 3.6 = 1 -> 0, Polish 8 -> 88.
    check(s.d[0] == 0x86 && s.d[1] == 0x00 && s.d[2] == 0x07 && s.d[3] == 0x88 && s.d[4] == 0x40,
          "fields off, wiper, fatigue, sound 3, driver door only: 86 00 07 88 40");
}

// ---------- Decoder class: short simulation of the main loop ----------
struct Tx {
    uint32_t ms;
    uint8_t cmd;
    uint8_t len;
    uint8_t data[xp::MAX_DATA];
};

struct Sim {
    Decoder d;
    uint32_t now = 1000;
    std::vector<Tx> tx;
    Sim() {
        DecoderConfig cfg;
        cfg.decoderMode = true;
        d.begin(cfg, nullptr);
    }
    void step() {
        d.tick(now, false);
        uint8_t buf[xp::MAX_DATA + 4];
        size_t n = d.nextRadioTx(now, true, buf, sizeof(buf));
        if (n >= 4) {
            Tx t;
            t.ms = now;
            t.cmd = buf[1];
            t.len = buf[2];
            std::memcpy(t.data, buf + 3, t.len);
            tx.push_back(t);
        }
    }
    // Time passing; frame 0F6 (ignition on) every 500 ms, so that ACC does not go off.
    void run(uint32_t ms) {
        for (uint32_t i = 0; i < ms; i++) {
            now++;
            if (now % 500 == 0) d.onCanFrame(frame(0x0F6, {0x88, 0, 0, 0, 0, 0, 0x60, 0}), now, true);
            step();
        }
    }
    void can(const CanFrame& f) { d.onCanFrame(f, now, true); }
    void radio(std::initializer_list<uint8_t> cmdAndData) {
        xp::Frame f;
        uint8_t i = 0;
        f.len = 0;
        for (uint8_t b : cmdAndData) {
            if (i == 0) f.cmd = b;
            else f.data[f.len++] = b;
            i++;
        }
        d.onRadioFrame(f, now);
    }
    // How many frames with this number from position 'from'.
    size_t count(uint8_t cmd, size_t from = 0) const {
        size_t n = 0;
        for (size_t i = from; i < tx.size(); i++)
            if (tx[i].cmd == cmd) n++;
        return n;
    }
    const Tx* nth(uint8_t cmd, size_t n) const {
        for (const Tx& t : tx)
            if (t.cmd == cmd && n-- == 0) return &t;
        return nullptr;
    }
};

static bool is38(const Tx* t, uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4, uint8_t d5) {
    return t && t->len == 6 && t->data[0] == d0 && t->data[1] == d1 && t->data[2] == d2 && t->data[3] == d3 &&
           t->data[4] == d4 && t->data[5] == d5;
}

static void testDecoder() {
    // --- start without 260: the first 38 with zeros, the second with content ---
    Sim s;
    s.run(600);                                               // 0F6 = 88 -> ACC
    check(s.d.power().acc, "simulation: ACC on after frame 0F6");
    s.can(frame(0x128, {0, 0, 0, 0, 0x00, 0, 0, 0}));         // door warning lamp off -> frame 38
    s.run(100);
    check(s.count(0x38) == 1 && is38(s.nth(0x38, 0), 0x04, 0, 0, 0, 0, 0), "start without 260: the first 38 = 04 00 00 00 00 00");
    s.can(frame(0x227, {0x00, 0x00, 0x11, 0x00, 0x00}));
    s.can(frame(0x3E1, {0x00, 0x00, 0x00, 0x40, 0x00}));
    s.can(frame(0x217, {0xF0, 0x80, 0, 0, 0, 0, 0x0B, 0xFF}));
    s.run(100);
    check(s.count(0x38) == 1, "227, 3E1, 217 before the first 260: no new 38 (still zeros, like the box)");
    s.can(frame(0x260, {0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}));
    s.run(100);
    check(s.count(0x38) == 2 && is38(s.nth(0x38, 1), 0x04, 0x02, 0x90, 0xA9, 0xB8, 0x40),
          "after 227, 3E1, 217, 260 (state 17.11): one new 38 = 04 02 90 A9 B8 40");
    s.can(frame(0x260, {0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}));
    s.run(100);
    check(s.count(0x38) == 2, "the same 260 once more: no new 38");

    // --- 81 01 before 361: only 38, no 3C ---
    size_t mark = s.tx.size();
    s.radio({0x81, 0x01});
    s.run(100);
    check(s.count(0x38, mark) == 1 && s.count(0x3C, mark) == 0 && s.tx.size() == mark + 1,
          "81 01 before 361: one 38, no 3C");

    // --- 361 -> 3C on change; 81 01 -> pair 38 + 3C in this order ---
    mark = s.tx.size();
    s.can(frame(0x361, {0xA0, 0x66, 0xC8, 0x0A, 0x00, 0x00}));
    s.run(100);
    check(s.tx.size() == mark + 1 && s.tx[mark].cmd == 0x3C && s.tx[mark].len == 2 && s.tx[mark].data[0] == 0x9E &&
              s.tx[mark].data[1] == 0x80, "the first 361: one 3C = 9E 80");
    s.can(frame(0x361, {0xA0, 0x66, 0xC8, 0x0A, 0x00, 0x00}));
    s.run(100);
    check(s.tx.size() == mark + 1, "the same 361 once more: no new 3C");
    mark = s.tx.size();
    s.radio({0x81, 0x01});
    s.run(100);
    check(s.tx.size() == mark + 2 && s.tx[mark].cmd == 0x38 && s.tx[mark + 1].cmd == 0x3C &&
              s.tx[mark + 1].ms - s.tx[mark].ms == 10 &&
              is38(&s.tx[mark], 0x04, 0x02, 0x90, 0xA9, 0xB8, 0x40) && s.tx[mark + 1].data[0] == 0x9E,
          "81 01: pair 38 + 3C (in this order, gap 10 ms), current content");
    mark = s.tx.size();
    s.radio({0x81, 0x00});
    s.radio({0x81});
    s.run(100);
    check(s.tx.size() == mark, "81 00 and 81 without data: nothing");

    // --- 260 with length 6 does not change the state ---
    mark = s.tx.size();
    s.can(frame(0x260, {0xA4, 0x34, 0x97, 0xBA, 0x55, 0x40}));
    s.run(100);
    const uint8_t want[5] = {0x02, 0x90, 0xA9, 0xB8, 0x40};
    check(s.tx.size() == mark && std::memcmp(s.d.settings38(), want, 5) == 0,
          "260 with length 6: no new 38, state unchanged");

    // --- change of one setting (parking assistant 260 5.7: 1 -> 0) ---
    s.can(frame(0x260, {0xA4, 0x1C, 0x8F, 0xBB, 0x45, 0x50, 0x00}));
    s.run(100);
    check(s.tx.size() == mark + 1 && is38(&s.tx[mark], 0x04, 0x02, 0x80, 0xA9, 0xB8, 0x40),
          "260 5.7 = 0: one 38 with DATA[2] bit 4 = 0");

    // --- doors still in DATA[0], the settings stay ---
    mark = s.tx.size();
    s.can(frame(0x128, {0, 0, 0, 0, 0x40, 0, 0, 0}));
    s.can(frame(0x1A1, {0x00, 0xDE, 0xA6, 0x40, 0, 0, 0, 0}));
    s.run(100);
    check(s.tx.size() == mark + 1 && is38(&s.tx[mark], 0x84, 0x02, 0x80, 0xA9, 0xB8, 0x40),
          "driver door: 38 = 84 + the same settings states");

    // --- 90 71: version and all state frames, including 38 and 3C (known deviation from the box) ---
    mark = s.tx.size();
    s.radio({0x90, 0x71, 0x00});
    s.run(300);
    check(s.count(0x71, mark) == 1 && s.count(0x38, mark) == 1 && s.count(0x3C, mark) == 1,
          "90 71: frame 71 and one 38 and one 3C (3C here too: a deviation from the box)");

    // --- without ACC: 81 01 sends nothing, the content waits ---
    {
        Sim off;
        off.can(frame(0x260, {0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}));
        off.can(frame(0x361, {0xA0, 0x66, 0xC8, 0x0A, 0x00, 0x00}));
        off.radio({0x81, 0x01});
        for (int i = 0; i < 100; i++) {
            off.now++;
            off.step();
        }
        bool quiet = off.tx.empty();
        off.run(700);                                         // 0F6 -> ACC
        check(quiet && off.d.power().acc && off.count(0x38) == 1 && off.count(0x3C) == 1 &&
                  is38(off.nth(0x38, 0), 0x04, 0x00, 0x90, 0xA9, 0x38, 0x40),
              "without ACC: 81 01 sends nothing; after ACC is on, one 38 and one 3C");
    }
}

// ---------- replay of a CAN + UART capture ----------
static uint32_t toMs(double sec) { return (uint32_t)(std::llround(sec * 1e6) / 1000); }

// Line "(time) uartA|uartB BYTES"; which = 0 for uartA, 1 for uartB.
static bool parseUartLine(const char* line, double& ts, int& which, std::vector<uint8_t>& out) {
    if (line[0] != '(') return false;
    char name[16];
    char hex[400];
    if (std::sscanf(line, "(%lf) %15s %399s", &ts, name, hex) != 3) return false;
    if (std::strcmp(name, "uartA") == 0) which = 0;
    else if (std::strcmp(name, "uartB") == 0) which = 1;
    else return false;
    out.clear();
    size_t n = std::strlen(hex);
    for (size_t i = 0; i + 1 < n; i += 2) {
        char b[3] = {hex[i], hex[i + 1], 0};
        out.push_back((uint8_t)std::strtoul(b, nullptr, 16));
    }
    return true;
}

struct Totals {
    unsigned ok38 = 0, bad38 = 0, early38 = 0, ok3C = 0, bad3C = 0, early3C = 0;
    unsigned on81 = 0, pairs = 0, only38 = 0, noAcc = 0, missing = 0;
};

static const char* baseName(const char* path) {
    const char* b = path;
    for (const char* p = path; *p; p++)
        if (*p == '/' || *p == '\\') b = p + 1;
    return b;
}

static bool replay(const char* path, Totals& sum) {
    std::FILE* fp = std::fopen(path, "r");
    if (!fp) {
        std::printf("FAIL  cannot open %s\n", path);
        return false;
    }
    Decoder d;
    DecoderConfig cfg;
    cfg.decoderMode = true;
    d.begin(cfg, nullptr);
    xp::Parser parser[2];
    Totals t;
    struct Sent {
        uint32_t ms;
        uint8_t cmd;
    };
    std::vector<Sent> sent;
    struct Ask {              // radio command 81 01: moment, position in 'sent', whether frame 361 had already arrived
        uint32_t ms;
        size_t from;
        bool have361;
    };
    std::vector<Ask> asks;
    char line[1024];
    while (std::fgets(line, sizeof(line), fp)) {
        CanFrame cf;
        double ts;
        int which = 0;
        std::vector<uint8_t> bytes;
        uint32_t now;
        bool isCan = parseCandumpLine(line, cf, &ts) && ts >= 0;
        if (isCan) {
            now = toMs(ts);
            d.onCanFrame(cf, now, true);
            d.tick(now, false);
        } else if (parseUartLine(line, ts, which, bytes)) {
            now = toMs(ts);
            d.tick(now, false);
            for (uint8_t b : bytes) {
                xp::Frame f;
                if (parser[which].feed(b, now, f) != xp::Parser::FRAME) continue;
                if (f.cmd >= 0x80) {                         // radio -> box
                    bool on = f.cmd == xp::RADIO_STATE && f.len >= 1 && f.data[0] == 0x01;
                    if (on) {
                        t.on81++;
                        if (!d.power().acc) t.noAcc++;
                        else asks.push_back({now, sent.size(), d.car().f361.seen});
                    }
                    d.onRadioFrame(f, now);
                } else if (f.cmd == 0x38 && f.len >= 6) {    // box -> radio: settings states
                    if (!d.car().f260.seen) {
                        t.early38++;                         // from before the first 260 (all zeros)
                        continue;
                    }
                    const uint8_t* m = d.settings38();
                    if (sameMasked(m, f.data + 1)) {
                        t.ok38++;
                    } else {
                        t.bad38++;
                        std::printf("  DIFFERENCE 38 %.3f s: box %02X %02X %02X %02X %02X, rule %02X %02X %02X %02X %02X\n",
                                    ts, f.data[1], f.data[2], f.data[3], f.data[4], f.data[5], m[0], m[1], m[2], m[3], m[4]);
                    }
                } else if (f.cmd == 0x3C && f.len >= 2) {    // box -> radio: which settings exist
                    xp::Frame our;
                    if (!vset::build3C(d.car(), our)) {
                        t.early3C++;
                        continue;
                    }
                    if (our.data[0] == f.data[0] && our.data[1] == f.data[1]) {
                        t.ok3C++;
                    } else {
                        t.bad3C++;
                        std::printf("  DIFFERENCE 3C %.3f s: box %02X %02X, rule %02X %02X\n", ts, f.data[0], f.data[1],
                                    our.data[0], our.data[1]);
                    }
                }
            }
        } else {
            continue;
        }
        uint8_t buf[xp::MAX_DATA + 4];
        size_t n = d.nextRadioTx(now, true, buf, sizeof(buf));
        if (n >= 4) sent.push_back({now, buf[1]});
    }
    std::fclose(fp);

    // After every 81 01 (with ACC): in our stream 38, immediately followed by 3C, within 300 ms.
    for (const Ask& a : asks) {
        bool pair = false, alone = false;
        for (size_t i = a.from; i < sent.size() && sent[i].ms - a.ms <= 300; i++) {
            if (sent[i].cmd != 0x38) continue;
            if (i + 1 < sent.size() && sent[i + 1].cmd == 0x3C && sent[i + 1].ms - a.ms <= 300) pair = true;
            else alone = true;
            break;
        }
        if (pair && a.have361) t.pairs++;
        else if (alone && !a.have361) t.only38++;
        else t.missing++;
    }

    std::printf("%s: box frame 38 matching %u, different %u, before the first 260 %u; frame 3C matching %u, different %u, "
                "before the first 361 %u\n",
                baseName(path), t.ok38, t.bad38, t.early38, t.ok3C, t.bad3C, t.early3C);
    std::printf("  radio commands 81 01: %u; after them pair 38 + 3C: %u, 38 alone (before 361): %u, without ACC: %u, none: %u\n",
                t.on81, t.pairs, t.only38, t.noAcc, t.missing);
    sum.ok38 += t.ok38;
    sum.bad38 += t.bad38;
    sum.early38 += t.early38;
    sum.ok3C += t.ok3C;
    sum.bad3C += t.bad3C;
    sum.early3C += t.early3C;
    sum.on81 += t.on81;
    sum.pairs += t.pairs;
    sum.only38 += t.only38;
    sum.noAcc += t.noAcc;
    sum.missing += t.missing;
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        testVectors();
        testShortAndKeep();
        testDecoder();
    } else {
        Totals sum;
        bool opened = true;
        for (int i = 1; i < argc; i++) opened = replay(argv[i], sum) && opened;
        check(opened, "all captures opened");
        char what[200];
        std::snprintf(what, sizeof(what), "frame 38: %u of %u match the box (radio bit mask 9E F8 FE FF F7)",
                      sum.ok38, sum.ok38 + sum.bad38);
        check(sum.bad38 == 0 && sum.ok38 > 0, what);
        std::snprintf(what, sizeof(what), "frame 3C: %u of %u match the box", sum.ok3C, sum.ok3C + sum.bad3C);
        check(sum.bad3C == 0 && sum.ok3C > 0, what);
        std::snprintf(what, sizeof(what), "after every 81 01 with ACC pair 38 + 3C: %u of %u (without ACC %u)",
                      sum.pairs + sum.only38, sum.on81 - sum.noAcc, sum.noAcc);
        check(sum.missing == 0, what);
        if (argc == 4) {
            // The three captures of 11.11.2026 together: 38 of 38 and 9 of 9.
            check(sum.ok38 == 38 && sum.bad38 == 0, "three captures of 11.11: 38 of 38 frames 38");
            check(sum.ok3C == 9 && sum.bad3C == 0, "three captures of 11.11: 9 of 9 frames 3C");
        }
    }
    std::printf("\n%d checks, %d errors\n", checks, failures);
    return failures ? 1 : 0;
}
