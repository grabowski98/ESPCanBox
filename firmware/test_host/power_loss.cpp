// Host test: PowerPolicy in the mode tolerant to lost frames (setLossTolerant, for the duration of
// an update over Wi-Fi) and proof that with the mode off the behaviour is the same as before.
//
// "Same as before" = a copy of PowerPolicy::update from before the mode was added (class OldPolicy below, copied
// 1:1 from power_policy.cpp of 16.11.2026). Both get the same events and their outputs are compared
// every 1 ms: on random runs (fixed seed) and, when a file is given, on a capture from the car.
//
// Usage: power_loss [capture.log]      exit code 0 = conformant
// Build (from the firmware directory):
//   uvx --from ziglang python -m ziglang c++ -std=c++17 -O1 -Wall -I lib/core test_host/power_loss.cpp lib/core/power_policy.cpp lib/car_psa_aee2010/psa_frames.cpp lib/core/candump.cpp lib/app/xp_car.cpp lib/radio_xp/xp_protocol.cpp -o .build_host/power_loss.exe
//   .build_host/power_loss.exe test_data/captures/can_20261111_191045.log
#include <cstdio>
#include <cstdint>

#include "candump.h"
#include "power_policy.h"
#include "psa_frames.h"
#include "xp_car.h"

static int failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) failures++;
}

// Behaviour from before the tolerant mode (reference for the comparison).
class OldPolicy {
public:
    void onBodyState(const psa::BodyState& b, uint32_t nowMs) {
        last_ = b;
        haveFrame_ = true;
        lastFrameMs_ = nowMs;
    }
    void onLights(bool lightsOn, uint32_t nowMs) {
        lightsOn_ = lightsOn;
        haveLights_ = true;
        lastLightsMs_ = nowMs;
    }
    PowerOutputs update(uint32_t nowMs) {
        PowerOutputs out;
        bool fresh = haveFrame_ && (uint32_t)(nowMs - lastFrameMs_) <= cfg_.frameTimeoutMs;
        out.ignition = fresh && psa::ignitionOn(last_);
        if (out.ignition) {
            acc_ = true;
            offPending_ = false;
        } else if (acc_) {
            if (!offPending_) {
                offPending_ = true;
                offAtMs_ = nowMs + cfg_.accOffDelayMs;
            } else if ((int32_t)(nowMs - offAtMs_) >= 0) {
                acc_ = false;
                offPending_ = false;
            }
        }
        out.acc = acc_;
        out.reverse = out.ignition && last_.reverseGear;
        bool lightsFresh = haveLights_ && (uint32_t)(nowMs - lastLightsMs_) <= cfg_.frameTimeoutMs;
        out.illumination = out.ignition && lightsFresh && lightsOn_;
        return out;
    }

private:
    PowerConfig cfg_;
    psa::BodyState last_;
    bool haveFrame_ = false;
    uint32_t lastFrameMs_ = 0;
    bool acc_ = false;
    bool offPending_ = false;
    uint32_t offAtMs_ = 0;
    bool lightsOn_ = false;
    bool haveLights_ = false;
    uint32_t lastLightsMs_ = 0;
};

static bool same(const PowerOutputs& a, const PowerOutputs& b) {
    return a.ignition == b.ignition && a.acc == b.acc && a.reverse == b.reverse && a.illumination == b.illumination;
}

static psa::BodyState body(uint8_t ign, bool rev = false) {
    psa::BodyState b;
    b.ignitionRaw = ign;
    b.ignition = psa::ignitionFromByte(ign);   // as psa::decodeBodyState fills it
    b.reverseGear = rev;
    return b;
}

// Simple number generator (fixed seed = repeatable test).
static uint32_t rng = 12345;
static uint32_t rnd(uint32_t n) {
    rng = rng * 1664525u + 1013904223u;
    return (rng >> 8) % n;
}

// Random run: frames 0F6 and 036 at random intervals (from a few ms to a few s), various ignition
// values, reverse, lights. Time starts just before the millis() wrap-around, to cover that case too.
static bool randomRunMatches(uint32_t startMs, uint32_t lengthMs) {
    static const uint8_t IGN[] = {0x80, 0x88, 0x8E, 0x90, 0x86, 0x84, 0x00};
    OldPolicy ref;
    PowerPolicy pol;
    pol.setLossTolerant(false);
    uint32_t nextBody = startMs + rnd(2000), nextLights = startMs + rnd(2000);
    for (uint32_t t = 0; t < lengthMs; t++) {
        uint32_t now = startMs + t;
        if (now == nextBody) {
            psa::BodyState b = body(IGN[rnd(sizeof(IGN))], rnd(4) == 0);
            ref.onBodyState(b, now);
            pol.onBodyState(b, now);
            // usually every 500 ms, sometimes a gap of up to 4 s
            nextBody = now + (rnd(10) == 0 ? 1 + rnd(4000) : 1 + rnd(600));
        }
        if (now == nextLights) {
            bool l = rnd(2) == 0;
            ref.onLights(l, now);
            pol.onLights(l, now);
            nextLights = now + (rnd(10) == 0 ? 1 + rnd(4000) : 1 + rnd(600));
        }
        if (!same(ref.update(now), pol.update(now))) {
            std::printf("     difference at %lu ms\n", (unsigned long)now);
            return false;
        }
    }
    return true;
}

// Capture from the car: both policies get frames 0F6 and 036 as in main.cpp, update every 1 ms.
static bool logMatches(const char* path, long& frames, long& accChanges) {
    std::FILE* fp = std::fopen(path, "r");
    if (!fp) {
        std::printf("cannot open %s\n", path);
        return false;
    }
    OldPolicy ref;
    PowerPolicy pol;
    pol.setLossTolerant(false);
    char line[1024];
    long nowMs = -1;
    bool ok = true, lastAcc = false;
    frames = 0;
    accChanges = 0;
    while (ok && std::fgets(line, sizeof(line), fp)) {
        double ts;
        CanFrame cf;
        if (!parseCandumpLine(line, cf, &ts) || ts < 0) continue;
        long t = (long)(ts * 1000.0);
        if (nowMs < 0) nowMs = t;
        while (nowMs < t) {
            nowMs++;
            PowerOutputs a = ref.update((uint32_t)nowMs), b = pol.update((uint32_t)nowMs);
            if (!same(a, b)) {
                std::printf("     difference at %ld ms of the capture\n", nowMs);
                ok = false;
                break;
            }
            if (b.acc != lastAcc) {
                lastAcc = b.acc;
                accChanges++;
            }
        }
        psa::BodyState bs;
        if (psa::decodeBodyState(cf, bs)) {
            ref.onBodyState(bs, (uint32_t)nowMs);
            pol.onBodyState(bs, (uint32_t)nowMs);
            frames++;
        }
        bool lights;
        if (xp::decodeLightsOn(cf, lights)) {
            ref.onLights(lights, (uint32_t)nowMs);
            pol.onLights(lights, (uint32_t)nowMs);
        }
    }
    std::fclose(fp);
    return ok;
}

// Ignition on, reverse, lights; frames every 500 ms until time 'lastMs', then a gap.
// Returns the output 'gapMs' after the last frame.
static PowerOutputs afterGap(bool tolerant, uint32_t base, uint32_t gapMs) {
    PowerPolicy p;
    p.setLossTolerant(tolerant);
    uint32_t t = base;
    for (int i = 0; i < 4; i++, t += 500) {
        p.onBodyState(body(0x8E, true), t);
        p.onLights(true, t);
        p.update(t);
    }
    uint32_t last = t - 500;
    PowerOutputs o;
    for (uint32_t now = last; now != last + gapMs + 1; now++) o = p.update(now);
    return o;
}

int main(int argc, char** argv) {
    // --- mode off = behaviour as before ---
    check(randomRunMatches(0, 600000), "mode off: random 10 min run matches the old logic");
    check(randomRunMatches(0xFFFFFFFFu - 300000u, 600000), "mode off: run through the millis() wrap matches");
    {
        PowerPolicy p;      // off by default
        OldPolicy r;
        p.onBodyState(body(0x88), 0);
        r.onBodyState(body(0x88), 0);
        bool ok = true;
        for (uint32_t t = 0; t < 5000; t++) ok = ok && same(p.update(t), r.update(t));
        check(ok, "mode off by default (without setLossTolerant)");
    }
    {
        PowerOutputs o = afterGap(false, 1000, 1000);
        check(o.ignition && o.reverse && o.illumination, "mode off: a 1000 ms gap does not switch off yet");
        o = afterGap(false, 1000, 1001);
        check(!o.ignition && !o.reverse && !o.illumination && o.acc,
              "mode off: a 1001 ms gap switches off ignition, reverse, ILL (ACC waits 2.5 s)");
    }

    // --- tolerant mode ---
    {
        PowerOutputs o = afterGap(true, 1000, 4900);
        check(o.ignition && o.acc && o.reverse && o.illumination,
              "tolerant mode: a 4.9 s gap does not switch off ignition, ACC, ILL, reverse");
        o = afterGap(true, 1000, 5000);
        check(o.ignition && o.acc && o.reverse && o.illumination, "tolerant mode: a 5000 ms gap does not switch off yet");
        o = afterGap(true, 1000, 5001);
        check(!o.ignition && !o.reverse && !o.illumination && o.acc,
              "tolerant mode: a 5001 ms gap switches off ignition, reverse, ILL");
        o = afterGap(true, 1000, 5001 + 2500);
        check(!o.acc, "tolerant mode: ACC goes off 2.5 s after the ignition is lost");
        o = afterGap(true, 0xFFFFFFFFu - 1800u, 4900);
        check(o.ignition && o.acc, "tolerant mode: a 4.9 s gap across the millis() wrap does not switch off");
    }
    {
        // Ignition switched off by a frame (byte 1 = 80) in tolerant mode: ignition goes off immediately, ACC after 2.5 s.
        PowerPolicy p;
        p.setLossTolerant(true);
        p.onBodyState(body(0x8E, true), 0);
        p.onLights(true, 0);
        check(p.update(0).acc, "tolerant mode: ignition from a frame switches ACC on");
        p.onBodyState(body(0x80), 100);
        PowerOutputs o = p.update(100);
        check(!o.ignition && !o.reverse && !o.illumination && o.acc, "tolerant mode: frame 80 switches the ignition off at once");
        bool accHeld = true;
        for (uint32_t t = 101; t < 100 + 2500; t++) {
            if (t % 500 == 100) p.onBodyState(body(0x80), t);
            accHeld = accHeld && p.update(t).acc;
        }
        check(accHeld, "tolerant mode: ACC held up to 2.5 s after frame 80");
        check(!p.update(100 + 2500).acc, "tolerant mode: ACC goes off 2.5 s after frame 80");
    }
    {
        // Lights (036) have the same timeout as 0F6: 0F6 keeps arriving, 036 goes silent for 4.9 s.
        PowerPolicy p;
        p.setLossTolerant(true);
        p.onLights(true, 0);
        PowerOutputs o;
        for (uint32_t t = 0; t <= 4900; t++) {
            if (t % 500 == 0) p.onBodyState(body(0x88), t);
            o = p.update(t);
        }
        check(o.illumination, "tolerant mode: a 4.9 s gap of 036 does not switch off ILL");
        for (uint32_t t = 4901; t <= 5001; t++) {
            if (t % 500 == 0) p.onBodyState(body(0x88), t);
            o = p.update(t);
        }
        check(!o.illumination && o.ignition, "tolerant mode: a gap of 036 > 5 s switches off ILL, the ignition stays");
    }
    {
        // Return to normal mode (3 s after the update) during a 2 s gap: 1 s counts immediately.
        PowerPolicy p;
        p.setLossTolerant(true);
        p.onBodyState(body(0x88), 0);
        PowerOutputs o;
        for (uint32_t t = 0; t <= 2000; t++) o = p.update(t);
        check(o.ignition, "tolerant mode: a 2 s gap keeps the ignition");
        p.setLossTolerant(false);
        o = p.update(2001);
        check(!o.ignition && o.acc, "after the mode is turned off: the old gap > 1 s switches off the ignition, ACC waits 2.5 s");
    }

    // --- capture from the car ---
    if (argc > 1) {
        long frames = 0, accChanges = 0;
        bool ok = logMatches(argv[1], frames, accChanges);
        std::printf("     0F6 frames: %ld, ACC changes: %ld\n", frames, accChanges);
        check(ok && frames > 0, "mode off: the car capture matches the old logic every 1 ms");
    }

    std::printf("\n%s\n", failures == 0 ? "RESULT: MATCH" : "RESULT: ERRORS");
    return failures == 0 ? 0 : 1;
}
