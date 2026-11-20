// Host test: feeds a sniffer capture (candump format) through the same
// logic that runs in the ESP32 (lib/core) and prints what the speaker would play.
//
// Usage: replay_log <capture.log>
// Build: see README.md (section on the test on a car capture).
#include <cstdio>
#include <cstring>
#include <string>

#include "candump.h"
#include "parking_beeper.h"
#include "psa_frames.h"

struct StepStat {
    std::string name;
    int beeps = 0;            // number of beeps (intermittent tone)
    long continuousMs = 0;    // continuous tone time
    long activeMs = 0;        // time during which the car requested sound
    int frames = 0;           // 0E1 frames in the step
};

static void printStep(const StepStat& s) {
    if (s.frames == 0 && s.activeMs == 0) return;
    if (s.activeMs == 0) return;   // we skip steps without sound
    std::printf("%-58.58s | beeps %3d | continuous tone %5ld ms | sound request %6ld ms\n",
                s.name.c_str(), s.beeps, s.continuousMs, s.activeMs);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: replay_log <capture.log>\n");
        return 2;
    }
    FILE* fp = std::fopen(argv[1], "r");
    if (!fp) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }

    ParkingBeeper beeper;
    StepStat step;
    step.name = "(before the first step)";
    char line[512];
    long nowMs = -1;
    bool prevTone = false;
    long frames0E1 = 0, framesAll = 0, totalBeeps = 0, totalContinuous = 0;
    long lastParkFrameMs = -1;
    long maxToneAfterLastFrameMs = 0;   // how long the tone played after the last 0E1 frame

    auto tick = [&](long t) {
        BeeperOutput o = beeper.update((uint32_t)t);
        if (o.active) step.activeMs++;
        if (o.toneOn && o.continuous) {
            step.continuousMs++;
            totalContinuous++;
        }
        if (o.toneOn && !prevTone && !o.continuous) {
            step.beeps++;
            totalBeeps++;
        }
        if (o.toneOn && lastParkFrameMs >= 0 && t - lastParkFrameMs > maxToneAfterLastFrameMs)
            maxToneAfterLastFrameMs = t - lastParkFrameMs;
        prevTone = o.toneOn;
    };

    while (std::fgets(line, sizeof(line), fp)) {
        if (std::strncmp(line, "# KROK", 6) == 0) {
            printStep(step);
            step = StepStat();
            step.name = line + 2;
            while (!step.name.empty() && (step.name.back() == '\n' || step.name.back() == '\r')) step.name.pop_back();
            continue;
        }
        CanFrame f;
        double ts;
        if (!parseCandumpLine(line, f, &ts) || ts < 0) continue;
        framesAll++;
        long t = (long)(ts * 1000.0);
        if (nowMs < 0) nowMs = t;
        // simulation of time passing every 1 ms, like the main loop in the ESP32
        while (nowMs < t) tick(++nowMs);
        psa::ParkingAid pa;
        if (psa::decodeParkingAid(f, pa)) {
            beeper.onFrame(pa, (uint32_t)nowMs);
            lastParkFrameMs = nowMs;
            frames0E1++;
            step.frames++;
        }
    }
    // after the end of the capture: we check that without frames the sound stops by itself
    for (int i = 0; i < 2000; i++) tick(++nowMs);
    bool silentAtEnd = !beeper.update((uint32_t)nowMs).toneOn;
    printStep(step);
    std::fclose(fp);

    std::printf("\nFrames in total: %ld, 0E1 frames: %ld\n", framesAll, frames0E1);
    std::printf("Beeps in total: %ld, continuous tone in total: %ld ms\n", totalBeeps, totalContinuous);
    std::printf("Longest sound after the last 0E1 frame: %ld ms (limit 400 ms)\n", maxToneAfterLastFrameMs);
    std::printf("Silence after the end of the capture: %s\n", silentAtEnd ? "YES" : "NO");
    return (silentAtEnd && maxToneAfterLastFrameMs <= 400) ? 0 : 1;
}
