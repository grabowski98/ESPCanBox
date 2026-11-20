// Host test: beeper scale. For each zone it feeds synthetic 0E1 frames
// for 10 s and counts the beeps. Checks the requirements from 15.11.2026:
//   - beeping from the first zone (6), also without a sound request from the car,
//   - each closer zone beeps clearly faster (at least 1.3 times),
//   - continuous tone only with interval 0x00 from the car,
//   - zone 7, inactive side and no frames = silence.
//
// Build: see README.md (section on tests on recordings).
#include <cstdio>

#include "compat/legacy_060.h"   // psa::ParkingAid / psa::PARK_* (aliases of vehicle_signals.h)
#include "parking_beeper.h"

static int failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) failures++;
}

struct Result {
    int beeps = 0;
    long continuousMs = 0;
    long toneMs = 0;
};

// Simulates 'ms' milliseconds; 0E1 frame every 100 ms, as in the car (or none at all).
static Result run(ParkingBeeper& b, const psa::ParkingAid& pa, long ms, bool sendFrames, uint32_t& now) {
    Result r;
    bool prev = false;
    for (long i = 0; i < ms; i++, now++) {
        if (sendFrames && i % 100 == 0) b.onFrame(pa, now);
        BeeperOutput o = b.update(now);
        if (o.toneOn) r.toneMs++;
        if (o.toneOn && o.continuous) r.continuousMs++;
        if (o.toneOn && !prev && !o.continuous) r.beeps++;
        prev = o.toneOn;
    }
    return r;
}

static psa::ParkingAid frontAt(uint8_t zone) {
    psa::ParkingAid pa;
    pa.frontStatus = psa::PARK_ACTIVE;
    pa.rearStatus = psa::PARK_WAITING;
    pa.frontCenter = zone;
    return pa;
}

int main() {
    uint32_t now = 1000;
    char msg[96];

    // 1. Each zone from 6 to 1: without a sound request from the car.
    int prevBeeps = 0;
    for (int zone = 6; zone >= 1; zone--) {
        ParkingBeeper b;
        Result r = run(b, frontAt((uint8_t)zone), 10000, true, now);
        std::printf("zone %d: %d beeps in 10 s (%.1f per second)\n", zone, r.beeps, r.beeps / 10.0);
        std::snprintf(msg, sizeof(msg), "zone %d beeps", zone);
        check(r.beeps > 0 && r.continuousMs == 0, msg);
        if (prevBeeps > 0) {
            std::snprintf(msg, sizeof(msg), "zone %d at least 1.3 times faster than %d", zone, zone + 1);
            check(r.beeps * 10 >= prevBeeps * 13, msg);
        }
        prevBeeps = r.beeps;
    }

    // 2. Zone 7 = silence.
    {
        ParkingBeeper b;
        check(run(b, frontAt(7), 3000, true, now).toneMs == 0, "zone 7: silence");
    }
    // 3. Inactive side (state 5) = silence, even when the zone is close.
    {
        ParkingBeeper b;
        psa::ParkingAid pa = frontAt(3);
        pa.frontStatus = psa::PARK_WAITING;
        check(run(b, pa, 3000, true, now).toneMs == 0, "front inactive: silence");
    }
    // 4. Continuous tone only on request from the car (interval 0x00).
    {
        ParkingBeeper b;
        psa::ParkingAid pa = frontAt(0);
        check(run(b, pa, 3000, true, now).continuousMs == 0, "zone 0 without a request from the car: beeping, not a continuous tone");
        pa.soundRequest = true;
        pa.soundFront = true;
        pa.intervalRaw = psa::INTERVAL_CONTINUOUS;
        Result r = run(b, pa, 3000, true, now);
        check(r.continuousMs == 3000, "interval 0x00 from the car: continuous tone");
    }
    // 5. Frames stop arriving during the continuous tone: silence after the timeout.
    {
        ParkingBeeper b;
        psa::ParkingAid pa = frontAt(0);
        pa.soundRequest = true;
        pa.intervalRaw = psa::INTERVAL_CONTINUOUS;
        run(b, pa, 1000, true, now);
        Result r = run(b, pa, 5000, false, now);
        std::printf("tone after the last frame: %ld ms\n", r.toneMs);
        check(r.toneMs <= 400, "no frames: the tone stops after 400 ms at most");
    }
    // 6. Factory mode (beepWithoutCarRequest = false): without a request from the car, silence.
    {
        BeeperConfig cfg;
        cfg.beepWithoutCarRequest = false;
        ParkingBeeper b(cfg);
        check(run(b, frontAt(4), 3000, true, now).toneMs == 0, "factory mode: silence without a request from the car");
    }

    std::printf("\n%s\n", failures == 0 ? "RESULT: MATCH" : "RESULT: ERRORS");
    return failures == 0 ? 0 : 1;
}
