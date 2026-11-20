// Host test: CarModel / CarState (lib/car_psa_aee2010/car_state.h).
// Checks: copy of the content and time of every frame held in CarState, frames too short and too long,
// fromBus = false, fresh() when the millisecond counter wraps through zero, haveDoors conditions.
// With a capture file as an argument: replays the capture and compares CarModel with what the
// existing encoders compute (psa::decodeBodyState, xp::DoorEncoder), frame by frame.
//
// Run: bash tools/run_host_tests.sh (README.md, section on tests on recordings).
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "can_ids.h"
#include "candump.h"
#include "car_state.h"
#include "xp_car.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

static CanFrame frame(uint32_t id, std::initializer_list<uint8_t> d) {
    CanFrame f;
    f.id = id;
    f.len = (uint8_t)d.size();
    uint8_t i = 0;
    for (uint8_t b : d) f.data[i++] = b;
    return f;
}

static bool same(const RawFrame& r, std::initializer_list<uint8_t> d, uint32_t ms) {
    if (!r.seen || r.len != d.size() || r.ms != ms) return false;
    uint8_t i = 0;
    for (uint8_t b : d) {
        if (r.d[i++] != b) return false;
    }
    for (; i < 8; i++) {
        if (r.d[i] != 0) return false;      // bytes above len must be zeros
    }
    return true;
}

static bool sameRaw(const RawFrame& a, const RawFrame& b) {
    return a.seen == b.seen && a.len == b.len && a.ms == b.ms && std::memcmp(a.d, b.d, 8) == 0;
}

static void printRaw(const char* name, const RawFrame& r, unsigned count) {
    std::printf("  %s: frames %u, last", name, count);
    if (!r.seen) {
        std::printf(" none\n");
        return;
    }
    for (uint8_t i = 0; i < r.len; i++) std::printf(" %02X", r.d[i]);
    std::printf(" (length %u)\n", r.len);
}

static int replay(const char* path) {
    std::FILE* fp = std::fopen(path, "r");
    if (!fp) {
        std::printf("FAIL  cannot open %s\n", path);
        return 1;
    }
    CarModel m;
    xp::DoorEncoder doors;
    psa::BodyState body;
    bool haveBody = false, haveDoors = false;
    uint32_t ms0F6 = 0, msLast = 0;
    unsigned frames = 0, n0F6 = 0, n036 = 0, n260 = 0, n361 = 0, n227 = 0, n3E1 = 0, n217 = 0;
    unsigned diffBody = 0, diffDoors = 0, diffTime = 0, diffAny = 0;
    long firstDoorsFrame = -1;
    char line[256];
    while (std::fgets(line, sizeof(line), fp)) {
        CanFrame f;
        double ts = -1;
        if (!parseCandumpLine(line, f, &ts)) continue;
        uint32_t now = (uint32_t)(ts * 1000.0);
        frames++;
        msLast = now;
        m.onCanFrame(f, now, true);
        const CarState& s = m.state();

        // reference: what Decoder computed next to the encoders in earlier versions
        psa::BodyState b;
        if (psa::decodeBodyState(f, b)) {
            body = b;
            haveBody = true;
            ms0F6 = now;
            n0F6++;
        }
        xp::Frame sf;
        if (doors.onCanFrame(f, sf) && !haveDoors) {
            haveDoors = true;
            firstDoorsFrame = (long)frames;
        }
        if (f.id == psa::ID_LIGHTING) n036++;
        if (f.id == psa::ID_PERSONALIZATION) n260++;
        if (f.id == psa::ID_OPTIONS) n361++;
        if (f.id == psa::ID_BUTTON_LEDS) n227++;
        if (f.id == psa::ID_STOP_START) n3E1++;
        if (f.id == psa::ID_FRAME_217) n217++;

        if (s.f0F6.seen != haveBody || s.body.ignitionRaw != body.ignitionRaw || s.body.reverseGear != body.reverseGear)
            diffBody++;
        if (haveBody && s.f0F6.ms != ms0F6) diffTime++;
        if (s.haveDoors != haveDoors) diffDoors++;
        if (!s.haveAnyFrame || s.msAnyFrame != now) diffAny++;
    }
    std::fclose(fp);

    const CarState& s = m.state();
    std::printf("record %s: frames %u, last at %lu ms\n", path, frames, (unsigned long)msLast);
    printRaw("0F6", s.f0F6, n0F6);
    printRaw("036", s.f036, n036);
    printRaw("260", s.f260, n260);
    printRaw("361", s.f361, n361);
    printRaw("227", s.f227, n227);
    printRaw("3E1", s.f3E1, n3E1);
    printRaw("217", s.f217, n217);
    std::printf("  ignition=0x%02X reverse=%u doors known=%u (first time at frame no. %ld)\n",
                s.body.ignitionRaw, s.body.reverseGear, s.haveDoors, firstDoorsFrame);
    std::printf("  differences from the encoders: ignition/reverse %u, time 0F6 %u, doors %u, bus traffic %u\n",
                diffBody, diffTime, diffDoors, diffAny);
    check(frames > 0, "the capture contains frames");
    check(diffBody == 0 && diffTime == 0, "0F6: CarModel = decodeBodyState after every frame of the capture");
    check(diffDoors == 0, "haveDoors: CarModel = DoorEncoder after every frame of the capture");
    check(diffAny == 0, "msAnyFrame = time of every frame of the capture");
    return 0;
}

int main(int argc, char** argv) {
    // --- initial state ---
    {
        CarModel m;
        const CarState& s = m.state();
        check(!s.haveAnyFrame && !s.haveDoors && !s.f0F6.seen && !s.f036.seen && !s.f260.seen && !s.f361.seen &&
                  !s.f227.seen && !s.f3E1.seen && !s.f217.seen,
              "start: nothing has arrived");
        check(!fresh(s.f0F6, 0, 1000) && !fresh(s.f260, 500, 1000), "start: fresh() = false");
        check(s.body.ignitionRaw == 0 && !s.body.reverseGear, "start: ignition 0, reverse 0");
    }

    // --- every frame from CarState: copy of content and time, the rest untouched ---
    {
        CarModel m;
        const CarState& s = m.state();
        m.onCanFrame(frame(0x0F6, {0x8E, 0x01, 0x02, 0x03, 0x04, 0x05, 0x5C, 0x80}), 1000, true);
        check(same(s.f0F6, {0x8E, 0x01, 0x02, 0x03, 0x04, 0x05, 0x5C, 0x80}, 1000), "0F6: content and time");
        check(s.body.ignitionRaw == 0x8E && s.body.reverseGear, "0F6: ignition 8E, reverse 1");
        check(s.f0F6.len >= 7 && s.f0F6.d[6] == 0x5C, "0F6: temperature byte 5C");
        check(!s.f036.seen && !s.f260.seen && !s.f361.seen && !s.f227.seen && !s.f3E1.seen && !s.f217.seen,
              "0F6 does not touch other frames");

        m.onCanFrame(frame(0x036, {0x0E, 0x00, 0x80, 0x2F, 0x01, 0x80, 0x00, 0xA0}), 1100, true);
        check(same(s.f036, {0x0E, 0x00, 0x80, 0x2F, 0x01, 0x80, 0x00, 0xA0}, 1100), "036: content and time");
        m.onCanFrame(frame(0x260, {0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}), 1200, true);
        check(same(s.f260, {0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}, 1200), "260: content and time (7 bytes)");
        m.onCanFrame(frame(0x361, {0xA0, 0x66, 0xC8, 0x0A, 0x00, 0x00}), 1300, true);
        check(same(s.f361, {0xA0, 0x66, 0xC8, 0x0A, 0x00, 0x00}, 1300), "361: content and time (6 bytes)");
        m.onCanFrame(frame(0x227, {0x00, 0x00, 0x11, 0x00, 0x00}), 1400, true);
        check(same(s.f227, {0x00, 0x00, 0x11, 0x00, 0x00}, 1400), "227: content and time (5 bytes)");
        m.onCanFrame(frame(0x3E1, {0x00, 0x00, 0x00, 0x40, 0x00}), 1500, true);
        check(same(s.f3E1, {0x00, 0x00, 0x00, 0x40, 0x00}, 1500), "3E1: content and time (5 bytes)");
        m.onCanFrame(frame(0x217, {0xF0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0B, 0xFF}), 1600, true);
        check(same(s.f217, {0xF0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0B, 0xFF}, 1600), "217: content and time");

        check(same(s.f0F6, {0x8E, 0x01, 0x02, 0x03, 0x04, 0x05, 0x5C, 0x80}, 1000) &&
                  same(s.f036, {0x0E, 0x00, 0x80, 0x2F, 0x01, 0x80, 0x00, 0xA0}, 1100) &&
                  same(s.f260, {0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}, 1200),
              "later frames did not touch earlier ones");
        check(s.haveAnyFrame && s.msAnyFrame == 1600, "msAnyFrame = time of the last frame");

        // a second frame with the same identifier replaces content and time
        m.onCanFrame(frame(0x260, {0x24, 0x1C, 0x97, 0xBA, 0x55, 0x40, 0x00}), 1700, true);
        check(same(s.f260, {0x24, 0x1C, 0x97, 0xBA, 0x55, 0x40, 0x00}, 1700), "260: new content replaces the old one");

        // a frame CarState does not hold (10B): bus activity only
        RawFrame before = s.f0F6;
        m.onCanFrame(frame(0x10B, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}), 1800, true);
        check(s.msAnyFrame == 1800 && sameRaw(s.f0F6, before) && same(s.f217, {0xF0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0B, 0xFF}, 1600),
              "10B: msAnyFrame only");
    }

    // --- a frame shorter than needed does not corrupt the state ---
    {
        CarModel m;
        const CarState& s = m.state();
        m.onCanFrame(frame(0x0F6, {0x8E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5C}), 100, true);   // 7 bytes
        check(!s.f0F6.seen && s.body.ignitionRaw == 0, "0F6 with 7 bytes before the first complete one: empty state");
        check(s.haveAnyFrame && s.msAnyFrame == 100, "a too short 0F6 counts as bus traffic");
        m.onCanFrame(frame(0x0F6, {0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D, 0x00}), 200, true);
        m.onCanFrame(frame(0x0F6, {0x8E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11}), 300, true);   // 7 bytes
        m.onCanFrame(frame(0x0F6, {}), 400, true);                                           // empty
        check(same(s.f0F6, {0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D, 0x00}, 200) && s.body.ignitionRaw == 0x88 &&
                  !s.body.reverseGear,
              "a too short 0F6 after a complete one: content, time, ignition unchanged");
        check(fresh(s.f0F6, 1199, 1000) && !fresh(s.f0F6, 1200, 1000), "a too short 0F6 does not refresh the frame age");

        // other frames: stored length, bytes above it zeroed (the reader checks len)
        m.onCanFrame(frame(0x260, {0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}), 500, true);
        m.onCanFrame(frame(0x260, {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}), 600, true);         // 6 bytes
        check(same(s.f260, {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}, 600), "260 with 6 bytes: len = 6, byte 7 zeroed");
        m.onCanFrame(frame(0x361, {0xA0, 0x66, 0xC8}), 700, true);
        check(same(s.f361, {0xA0, 0x66, 0xC8}, 700), "361 with 3 bytes: len = 3");
        m.onCanFrame(frame(0x227, {}), 800, true);
        check(s.f227.seen && s.f227.len == 0 && s.f227.ms == 800, "227 empty: len = 0");

        // length greater than 8 (broken frame): clipped, no access outside the buffer
        CanFrame bad = frame(0x217, {1, 2, 3, 4, 5, 6, 7, 8});
        bad.len = 200;
        m.onCanFrame(bad, 900, true);
        check(same(s.f217, {1, 2, 3, 4, 5, 6, 7, 8}, 900), "217 with length 200: cut to 8");
        check(same(s.f260, {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}, 600) && same(s.f361, {0xA0, 0x66, 0xC8}, 700),
              "a frame with length 200 did not touch the neighbouring fields");
    }

    // --- fromBus = false (frame pasted from the panel) does not touch msAnyFrame ---
    {
        CarModel m;
        const CarState& s = m.state();
        m.onCanFrame(frame(0x260, {0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}), 100, false);
        check(!s.haveAnyFrame && s.msAnyFrame == 0, "fromBus=0 as the first: no bus traffic");
        check(same(s.f260, {0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}, 100), "fromBus=0: frame content stored");
        m.onCanFrame(frame(0x036, {0, 0, 0, 0, 0, 0, 0, 0}), 200, true);
        m.onCanFrame(frame(0x0F6, {0x88, 0, 0, 0, 0, 0, 0x5C, 0x00}), 900, false);
        check(s.haveAnyFrame && s.msAnyFrame == 200, "fromBus=0 after a frame from the bus: msAnyFrame unchanged");
        check(same(s.f0F6, {0x88, 0, 0, 0, 0, 0, 0x5C, 0x00}, 900) && s.body.ignitionRaw == 0x88,
              "fromBus=0: 0F6 stored, ignition 88");
    }

    // --- fresh(): boundary and millisecond counter wrapping through zero ---
    {
        CarModel m;
        const CarState& s = m.state();
        m.onCanFrame(frame(0x260, {0xA4, 0x1C, 0x8F, 0xBB, 0xC5, 0x50, 0x00}), 0xFFFFFF00u, true);
        check(fresh(s.f260, 0xFFFFFF00u, 1500), "fresh: age 0");
        check(fresh(s.f260, 0xFFFFFFFFu, 1500), "fresh: age 255 ms before the wrap");
        check(fresh(s.f260, 0x00000010u, 1500), "fresh: age 272 ms after wrapping through zero");
        check(fresh(s.f260, 0x000004DBu, 1500), "fresh: age 1499 ms after the wrap");
        check(!fresh(s.f260, 0x000004DCu, 1500), "fresh: age 1500 ms = already stale");
        check(!fresh(s.f260, 0x00010000u, 1500), "fresh: age 65 s = stale");
        check(!fresh(s.f361, 0x00000010u, 1500), "fresh: no frame = false");
        check(!fresh(s.f260, 0xFFFFFF00u, 0), "fresh: limit 0 = always false");
    }

    // --- haveDoors: the same conditions as DoorEncoder::onCanFrame ---
    {
        CarModel m;
        const CarState& s = m.state();
        m.onCanFrame(frame(0x1A1, {0x80, 0xDE, 0x00, 0x40, 0, 0, 0, 0}), 10, true);
        check(!s.haveDoors, "1A1 with a door message before the warning lamp: not yet");
        m.onCanFrame(frame(0x128, {0, 0, 0, 0}), 20, true);                      // 4 bytes: too short
        check(!s.haveDoors, "128 too short: no");
        m.onCanFrame(frame(0x128, {0, 0, 0, 0, 0x40, 0, 0, 0}), 30, true);       // warning lamp on
        check(!s.haveDoors, "128 lamp lit, no message: no");
        m.onCanFrame(frame(0x1A1, {0x80, 0x00, 0x00, 0x40, 0, 0, 0, 0}), 40, true);
        check(!s.haveDoors, "1A1 another message: no");
        m.onCanFrame(frame(0x1A1, {0x80, 0xDE, 0x00}), 50, true);                // 3 bytes: too short
        check(!s.haveDoors, "1A1 too short: no");
        m.onCanFrame(frame(0x1A1, {0x80, 0xDE, 0x00, 0x40, 0, 0, 0, 0}), 60, true);
        check(s.haveDoors, "1A1 with a message while the lamp is lit: yes");
    }
    {
        CarModel m;
        m.onCanFrame(frame(0x128, {0, 0, 0, 0, 0x00, 0, 0, 0}), 10, true);
        check(m.state().haveDoors, "128 lamp off: doors known (closed)");
    }

    std::printf("checks: %d, errors: %d\n", checks, failures);

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (replay(argv[i]) != 0) failures++;
        }
    }

    std::printf("\n%s\n", failures == 0 ? "RESULT: MATCH" : "RESULT: ERRORS");
    return failures == 0 ? 0 : 1;
}
