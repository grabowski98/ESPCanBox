// Host test: custom buttons (TEL, moon) and doors (frame 38).
// Doors are also checked on a capture from the car: pass the capture file as an argument,
// and the test prints the successive door states computed from frames 1A1 and 128.
//
// Build: see README.md (section on tests on recordings).
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "candump.h"
#include "xp_car.h"
#include "radio_commands.h"
#include "xp_keys_custom.h"

static int failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) failures++;
}

// Default remaps of the reference profile (lib/car_psa_aee2010/profile_psa_aee2010.cpp, KEY_REMAP_DEFAULTS).
// A copy, because this test does not link the PSA profile; test_host/key_tables.cpp checks that the profile
// has exactly these rows.
static const vehicle::KeyRemapDefault PSA_DEFAULTS[] = {{0x9F, 0x9B}, {0x21, 0x1F}};

static void psaDefaults(xp::KeyCustomizer& k) { k.setDefaults(PSA_DEFAULTS, 2); }

static xp::KeyEvent ev(uint8_t code, uint8_t state) {
    xp::KeyEvent e;
    e.code = code;
    e.state = state;
    return e;
}

static CanFrame frame(uint32_t id, std::initializer_list<uint8_t> d) {
    CanFrame f;
    f.id = id;
    f.len = (uint8_t)d.size();
    uint8_t i = 0;
    for (uint8_t b : d) f.data[i++] = b;
    return f;
}

int main(int argc, char** argv) {
    xp::KeyEvent out;

    // --- TEL (PSA defaults): code 9F replaced with 9B, state unchanged ---
    {
        xp::KeyCustomizer k;
        psaDefaults(k);
        check(k.onEvent(ev(0x9F, 1), out) && out.code == 0x9B && out.state == 1, "TEL pressed -> code 9B state 1");
        check(k.onEvent(ev(0x9F, 0), out) && out.code == 0x9B && out.state == 0, "TEL released -> code 9B state 0");
    }
    // --- other buttons unchanged ---
    {
        xp::KeyCustomizer k;
        psaDefaults(k);
        check(k.onEvent(ev(0x14, 1), out) && out.code == 0x14 && out.state == 1, "volume + unchanged");
        check(k.onEvent(ev(0x9B, 1), out) && out.code == 0x9B, "DRIVE unchanged");
        check(k.onEvent(ev(0x21, 1), out) && out.code == 0x1F, "key 21 (between SRC and LIST) -> 1F (Siri)");
        check(k.setMap(0x9B, 0x1F) && k.onEvent(ev(0x9B, 1), out) && out.code == 0x1F, "swap from the console 9B -> 1F");
        check(k.setMap(0x9B, 0x9B) && k.onEvent(ev(0x9B, 1), out) && out.code == 0x9B, "swap undone");
    }
    // --- no defaults (constructor alone; generic and example profiles): every code unchanged ---
    {
        char buf[xp::KeyCustomizer::MAP_TEXT_MAX];
        xp::KeyCustomizer k;
        check(k.mapCount() == 0 && k.defaultCount() == 0 && k.serializeMap(buf, sizeof(buf)) && buf[0] == 0,
              "constructor: no defaults, empty table");
        check(k.onEvent(ev(0x21, 1), out) && out.code == 0x21 && k.onEvent(ev(0x9F, 1), out) && out.code == 0x9F,
              "without defaults: 21 and 9F unchanged");
        check(!k.defaultEntry(0).used, "without defaults: defaultEntry(0) used=false");
        check(k.setMap(0x21, 0x1F) && k.mapCount() == 1, "without defaults: setMap works");
        k.resetMap();
        check(k.mapCount() == 0, "without defaults: resetMap = empty table");
        k.setDefaults(PSA_DEFAULTS, 2);
        check(k.mapCount() == 2 && k.mapped(0x21) == 0x1F, "setDefaults applies the table at once");
        k.setDefaults(nullptr, 5);
        check(k.mapCount() == 0 && k.defaultCount() == 0, "setDefaults(nullptr, 5) = no defaults");
        static const vehicle::KeyRemapDefault MANY[20] = {
            {0x40, 0x80}, {0x41, 0x81}, {0x42, 0x82}, {0x43, 0x83}, {0x44, 0x84}, {0x45, 0x85}, {0x46, 0x86},
            {0x47, 0x87}, {0x48, 0x88}, {0x49, 0x89}, {0x4A, 0x8A}, {0x4B, 0x8B}, {0x4C, 0x8C}, {0x4D, 0x8D},
            {0x4E, 0x8E}, {0x4F, 0x8F}, {0x50, 0x90}, {0x51, 0x91}, {0x52, 0x92}, {0x53, 0x93}};
        k.setDefaults(MANY, 20);
        check(k.defaultCount() == 16 && k.mapCount() == 16 && k.mapped(0x50) == 0x50,
              "more than 16 defaults: only the first 16 used");
        static const vehicle::KeyRemapDefault SAME[] = {{0x30, 0x30}};
        k.setDefaults(SAME, 1);
        check(k.mapCount() == 0 && !k.defaultEntry(0).used, "default entry XX=XX = no remapping");
    }
    // --- moon: toggle ---
    {
        xp::KeyCustomizer k;
        check(k.onEvent(ev(0xA4, 1), out) && out.code == 0xA4 && out.state == 1, "moon 1st press -> pressed");
        check(!k.onEvent(ev(0xA4, 2), out), "moon hold skipped");
        check(!k.onEvent(ev(0xA4, 0), out), "moon release skipped");
        check(k.screenOff(), "screen marked as blanked");
        check(k.onEvent(ev(0xA4, 1), out) && out.state == 0, "moon 2nd press -> released");
        check(!k.onEvent(ev(0xA4, 0), out), "moon release skipped (2)");
        check(!k.screenOff(), "screen marked as on");
        check(!k.onIgnitionOff(out), "ignition off with the screen on: nothing");
        k.onEvent(ev(0xA4, 1), out);
        check(k.onIgnitionOff(out) && out.code == 0xA4 && out.state == 0, "ignition off with the screen blanked: released");
        check(k.onEvent(ev(0xA4, 1), out) && out.state == 1, "after ignition off the first press blanks again");
        // The radio restarts (frame 81 01 = RADIO_ON, parsed as xp::decodeCommand does for Decoder::onRadioFrame):
        // screen on again.
        xp::Frame on;
        on.cmd = 0x81;
        on.len = 1;
        on.data[0] = 0x01;
        const RadioCmd c = parseRadioCmd(on);
        check(c.kind == RadioCmdKind::RADIO_ON, "radio frame 81 01 = RADIO_ON");
        if (c.kind == RadioCmdKind::RADIO_ON) k.onRadioOn();
        check(!k.screenOff(), "after 81 01 the screen is marked as on");
        check(k.onEvent(ev(0xA4, 1), out) && out.code == 0xA4 && out.state == 1, "after 81 01 the first press sends pressed again");
    }
    // --- remap table as text (serializeMap / parseMap / resetMap) ---
    {
        char buf[xp::KeyCustomizer::MAP_TEXT_MAX];
        xp::KeyCustomizer k;
        psaDefaults(k);
        check(k.serializeMap(buf, sizeof(buf)) && std::strcmp(buf, "9F=9B;21=1F") == 0, "PSA defaults: text 9F=9B;21=1F");
        check(k.mapCount() == 2 && k.defaultCount() == 2, "PSA defaults: 2 entries");
        xp::KeyCustomizer::Entry d0 = k.defaultEntry(0), d2 = k.defaultEntry(2);
        check(d0.used && d0.from == 0x9F && d0.to == 0x9B && !d2.used, "defaultEntry(0) = 9F->9B, out of range used=false");

        auto parse = [&](const char* s) { return k.parseMap(s, std::strlen(s)); };
        auto text = [&]() { k.serializeMap(buf, sizeof(buf)); return (const char*)buf; };

        check(parse("9b=1f;a0=b5") && std::strcmp(text(), "9B=1F;A0=B5") == 0 && k.mapped(0x9F) == 0x9F,
              "lower case accepted, the text replaces the whole table (9F no longer swapped)");
        check(parse("9B=1F;") && std::strcmp(text(), "9B=1F") == 0, "trailing semicolon accepted, canonical form without it");
        check(parse("9B=1F;30=30;A0=B5") && std::strcmp(text(), "9B=1F;A0=B5") == 0 && k.mapCount() == 2,
              "entry XX=XX skipped");
        check(parse("") && k.mapCount() == 0 && std::strcmp(text(), "") == 0, "empty text = table without swaps");
        check(k.parseMap(nullptr, 0) && k.mapCount() == 0, "nullptr with length 0 = table without swaps");

        // 16 pairs + semicolon = 96 characters
        char full[200];
        size_t p = 0;
        for (int i = 0; i < 16; i++) p += std::snprintf(full + p, sizeof(full) - p, "%02X=%02X;", 0x40 + i, 0x80 + i);
        check(p == 96 && k.parseMap(full, p) && k.mapCount() == 16, "16 pairs + semicolon (96 characters) accepted");
        check(k.serializeMap(buf, sizeof(buf)) && std::strlen(buf) == 95 && std::strncmp(buf, full, 95) == 0,
              "16 entries: canonical form 95 characters");
        check(!k.serializeMap(buf, 95) && buf[0] == 0, "buffer too small: false and empty text");
        check(k.serializeMap(buf, 96), "buffer 96 is enough");

        // Any error: table unchanged. Reference state = 16 entries from 'full'.
        char before[xp::KeyCustomizer::MAP_TEXT_MAX];
        k.serializeMap(before, sizeof(before));
        auto unchanged = [&]() { return std::strcmp(text(), before) == 0; };
        char seventeen[sizeof(full) + 8];   // room for one more pair than "full" holds
        std::snprintf(seventeen, sizeof(seventeen), "%s50=90", full);
        check(!parse(seventeen) && unchanged(), "17 pairs rejected, table unchanged");
        check(!parse("9F=9B;9F=30") && unchanged(), "duplicate key code rejected, table unchanged");
        check(!parse("9F=9F;9F=30") && unchanged(), "duplicate with an XX=XX entry rejected");
        check(!parse("9F=9B; 21=1F") && unchanged(), "space after a semicolon rejected");
        check(!parse(" 9F=9B") && !parse("9F=9B ") && !parse("9F = 9B") && unchanged(), "spaces rejected");
        check(!parse("9F=9B;;") && !parse(";") && !parse(";9F=9B") && unchanged(), "empty entries rejected");
        check(!parse("9F=9") && !parse("9F=") && !parse("9F") && !parse("9F=9B;2") && unchanged(), "cut entries rejected");
        check(!parse("9G=9B") && !parse("9F:9B") && !parse("9F=9B,21=1F") && !parse("19F=9B") && unchanged(),
              "bad characters rejected");
        check(!k.parseMap("9F=9B\0" "21=1F", 11) && unchanged(), "zero byte inside rejected");
        check(k.parseMap("9F=9BXXXX", 5) && std::strcmp(text(), "9F=9B") == 0, "only 'len' characters read");

        // serializeMap(parseMap(x)) = canonical form, and parsing it again changes nothing
        check(parse("a4=11;0c=0d;ff=00;") && std::strcmp(text(), "A4=11;0C=0D;FF=00") == 0, "canonical form");
        char canon[xp::KeyCustomizer::MAP_TEXT_MAX];
        std::strcpy(canon, buf);
        check(parse(canon) && std::strcmp(text(), canon) == 0, "reading the canonical form again changes nothing");
        check(k.onEvent(ev(0x0C, 1), out) && out.code == 0x0D, "a loaded swap works in onEvent");

        // parseMap does not touch the moon, resetMap = state after setDefaults
        check(k.mapped(0xA4) == 0x11 && k.onEvent(ev(0xA4, 1), out) && out.code == 0xA4 && out.state == 1,
              "swapping A4 in the table has no effect (moon handled separately)");
        check(parse("") && k.screenOff(), "parseMap does not change the moon state");
        check(k.onEvent(ev(0xA4, 1), out) && out.code == 0xA4 && out.state == 0, "moon still toggles after parseMap");
        k.resetMap();
        xp::KeyCustomizer fresh;
        psaDefaults(fresh);
        char a[xp::KeyCustomizer::MAP_TEXT_MAX], b[xp::KeyCustomizer::MAP_TEXT_MAX];
        k.serializeMap(a, sizeof(a));
        fresh.serializeMap(b, sizeof(b));
        bool sameEntries = true;
        for (uint8_t i = 0; i < xp::KeyCustomizer::MAP_SIZE; i++) {
            const xp::KeyCustomizer::Entry &x = k.entry(i), &y = fresh.entry(i);
            sameEntries = sameEntries && x.used == y.used && (!x.used || (x.from == y.from && x.to == y.to));
        }
        check(std::strcmp(a, b) == 0 && sameEntries && k.mapCount() == 2, "resetMap = state after setDefaults");
        check(k.setMap(0x9F, 0x9F) && k.mapCount() == 1 && k.setMap(0x55, 0x66), "setMap works after resetMap");
    }
    // --- doors ---
    {
        xp::DoorEncoder d;
        xp::Frame f;
        check(!d.onCanFrame(frame(0x1A1, {0xFF, 0xFF, 0, 0, 0, 0, 0, 0}), f), "1A1 without a message: nothing");
        check(!d.onCanFrame(frame(0x1A1, {0x00, 0xDE, 0xA6, 0x40, 0, 0, 0, 0}), f), "message without the lamp: skipped");
        d.onCanFrame(frame(0x128, {0, 0, 0, 0, 0x40, 0, 0, 0}), f);
        check(d.onCanFrame(frame(0x1A1, {0x00, 0xDE, 0xA6, 0x40, 0, 0, 0, 0}), f) && f.cmd == 0x38 && f.len == 6 &&
                  f.data[0] == 0x84, "driver door -> DATA[0] = 84");
        // DATA[1..5] (vehicle settings states) are not the constants 02 E1 A9 98 40: DoorEncoder
        // gets them via setSettings() (computed by vset::update38 from CAN), and until then it sends zeros,
        // like the box before the first 260 frame (capture _190551, 74.090 s: 04 00 00 00 00 00).
        bool zeros = f.data[1] == 0 && f.data[2] == 0 && f.data[3] == 0 && f.data[4] == 0 && f.data[5] == 0;
        const uint8_t set[5] = {0x02, 0x90, 0xA9, 0xB8, 0x40};
        d.setSettings(set);
        xp::Frame g;
        d.build(g);
        check(zeros && g.len == 6 && g.data[0] == 0x84 && g.data[1] == 0x02 && g.data[2] == 0x90 &&
                  g.data[3] == 0xA9 && g.data[4] == 0xB8 && g.data[5] == 0x40,
              "DATA[1..5]: zeros before setSettings, then the given bytes");
        check(!d.onCanFrame(frame(0x128, {0, 0, 0, 0, 0x40, 0, 0, 0}), f) && d.doors() == 0x40,
              "lamp lit, message gone: the indication stays");
        check(d.onCanFrame(frame(0x1A1, {0x80, 0xDE, 0xA6, 0x48, 0, 0, 0, 0}), f) && f.data[0] == 0x8C,
              "driver + boot -> 8C");
        check(d.onCanFrame(frame(0x128, {0, 0, 0, 0, 0x00, 0, 0, 0}), f) && f.data[0] == 0x04,
              "lamp off -> everything closed (04)");
    }

    // --- capture from the car: door sequence ---
    if (argc > 1) {
        std::FILE* fp = std::fopen(argv[1], "r");
        if (!fp) {
            std::printf("cannot open %s\n", argv[1]);
            return 2;
        }
        xp::DoorEncoder d;
        xp::Frame f;
        char line[512];
        int changes = 0;
        uint8_t last = 0xFF;
        while (std::fgets(line, sizeof(line), fp)) {
            CanFrame cf;
            if (!parseCandumpLine(line, cf)) continue;
            if (!d.onCanFrame(cf, f)) continue;
            if (f.data[0] != last) {
                last = f.data[0];
                changes++;
                double ts = 0;
                std::sscanf(line, "(%lf)", &ts);
                std::printf("  %9.2f s  frame 38 DATA[0] = %02X\n", ts, last);
            }
        }
        std::fclose(fp);
        std::printf("door state changes in the capture: %d\n", changes);
    }

    std::printf("\n%s\n", failures == 0 ? "RESULT: MATCH" : "RESULT: ERRORS");
    return failures == 0 ? 0 : 1;
}
