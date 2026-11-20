// Host test: overnight parking sensors record (lib/car_psa_aee2010/park_diag.h).
// The record normally lives in RTC memory (src/main.cpp); here it is a plain variable that "survives" between
// two ParkDiag objects, the way RTC memory survives deep sleep. Synthetic frames, no capture.
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "car_state.h"
#include "park_diag.h"

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

// One awake period: ignition on at 'now', then 126 every 500 ms with byte 1 = b1 for 'ms'.
// Returns how many times on126 reported a change while asleep.
struct Run {
    CarModel car;
    psa::ParkDiag diag;
    uint32_t now = 1000;
    int flips = 0;
    uint8_t was = 0xFF, is = 0xFF;
    void ignitionOn() { car.onCanFrame(frame(0x0F6, {0x88, 0x59, 0x1E, 0x23, 0x23, 0x5C, 0x5C, 0x24}), now, true); }
    void f126(uint8_t b1, bool fromBus = true) {
        car.onCanFrame(frame(0x126, {b1, 0xB0, 0x00}), now, fromBus);
        uint8_t w = 0, n = 0;
        if (fromBus && diag.on126(car.state(), now, w, n)) {
            flips++;
            was = w;
            is = n;
        }
    }
    void run(uint8_t b1, uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 500) {
            now += 500;
            if (now % 1000 == 0) ignitionOnRefresh();
            f126(b1);
        }
    }
    void ignitionOnRefresh() { car.onCanFrame(frame(0x0F6, {0x88, 0x59, 0x1E, 0x23, 0x23, 0x5C, 0x5C, 0x24}), now, true); }
};

int main() {
    psa::ParkDiagRecord rtc;
    std::memset(&rtc, 0xA5, sizeof(rtc));            // RTC memory after power-on: garbage

    std::printf("--- power-on ---\n");
    {
        Run r;
        r.diag.begin(&rtc, false);
        check(rtc.magic == psa::ParkDiag::MAGIC && rtc.haveState == 0 && rtc.wakes == 0 && rtc.flipsAsleep == 0 &&
                  rtc.presses == 0 && rtc.releases == 0,
              "garbage record: started fresh, all counters 0");
        r.ignitionOn();
        r.f126(0x00);
        r.now += 500;
        r.f126(0x00);
        check(rtc.haveState == 0, "126 in the first 3.5 s after ignition on: not stored");
        r.run(0x00, 4000);
        check(rtc.haveState == 1 && rtc.state == 0 && r.flips == 0, "after 3.5 s, two equal 126 = 00: state ON stored, no report");
        r.diag.notePress();
        r.diag.noteRelease();
        r.run(0x18, 1000);
        check(rtc.state == 1 && r.flips == 0 && rtc.presses == 1 && rtc.releases == 1,
              "awake change to 18 (our own press): stored as OFF, not counted as a change while asleep");
    }
    std::printf("--- wake-up, same state ---\n");
    {
        Run r;
        r.diag.begin(&rtc, true);
        r.ignitionOn();
        r.run(0x18, 6000);
        check(rtc.wakes == 1 && r.flips == 0 && rtc.flipsAsleep == 0 && rtc.presses == 1,
              "wake-up with 126 = 18 as before sleep: wakes=1, no change, counters kept");
    }
    std::printf("--- wake-up, state changed while asleep ---\n");
    {
        Run r;
        r.diag.begin(&rtc, true);
        r.ignitionOn();
        r.f126(0x00);
        r.now += 1;
        r.f126(0x18, false);                          // pasted from the panel: ignored, breaks nothing
        r.run(0x00, 6000);
        check(rtc.wakes == 2 && r.flips == 1 && r.was == 1 && r.is == 0 && rtc.flipsAsleep == 1 && rtc.state == 0,
              "was OFF before sleep, ON after the wake-up: reported once, park-flips-asleep=1");
        r.run(0x18, 2000);
        check(r.flips == 1 && rtc.state == 1, "later changes in the same awake period: stored, not reported");
    }
    std::printf("--- unknown value and one frame only ---\n");
    {
        Run r;
        r.diag.begin(&rtc, true);
        r.ignitionOn();
        for (int i = 0; i < 4; i++) {                 // 0F6 every second: no new ignition-on moment
            r.now += 1000;
            r.ignitionOnRefresh();
        }
        r.f126(0x00);
        r.now += 500;
        r.f126(0x08);                                 // one inhibition bit only: not a state
        r.now += 500;
        r.f126(0x00);
        check(r.flips == 0 && rtc.state == 1, "00, 08, 00: never two equal known frames in a row, nothing stored");
        r.now += 500;
        r.f126(0x00);
        check(r.flips == 1 && r.was == 1 && r.is == 0 && rtc.flipsAsleep == 2 && rtc.wakes == 3,
              "then 00 again: stable, differs from before sleep, reported");
    }
    std::printf("--- RAM record (no RTC) ---\n");
    {
        psa::ParkDiag d;
        d.begin(nullptr, false);
        d.notePress();
        check(d.record().presses == 1 && d.record().wakes == 0 && rtc.presses == 1, "own record, the RTC record untouched");
    }
    std::printf("park_diag: %d checks, %d errors\n", checks, failures);
    return failures ? 1 : 0;
}
