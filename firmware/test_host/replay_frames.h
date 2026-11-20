// Host tests of the car profiles (radio_none.cpp, example_car.cpp): loading a candump capture and replaying it
// through a Decoder in 1 ms steps, in the main loop order (onCanFrame -> tick -> nextRadioTx).
// Lines that are not CAN frames (UART lines, comments) are skipped.
#pragma once
#include <cmath>
#include <cstdio>
#include <vector>

#include "candump.h"
#include "decoder.h"

struct ReplayFrame {
    uint32_t ms;
    CanFrame f;
};

// false = the file cannot be opened. Times in ms from the capture clock (rounded).
inline bool loadCapture(const char* path, std::vector<ReplayFrame>& out) {
    FILE* fp = std::fopen(path, "r");
    if (!fp) return false;
    char line[1024];
    while (std::fgets(line, sizeof(line), fp)) {
        ReplayFrame r;
        double ts;
        if (parseCandumpLine(line, r.f, &ts) && ts >= 0) {
            r.ms = (uint32_t)std::llround(ts * 1000.0);
            out.push_back(r);
        }
    }
    std::fclose(fp);
    return true;
}

// What happened during one replay.
struct ReplayResult {
    unsigned long frames = 0;          // CAN frames fed
    unsigned long uartBytes = 0;       // bytes handed to the radio port by nextRadioTx
    unsigned long uartFrames = 0;      // frames handed to the radio port
    unsigned long accMs = 0, illMs = 0, revMs = 0, toneMs = 0;   // milliseconds with the output on
};

// Step callback: called after every tick with the bytes for the radio (n = 0: nothing).
typedef void (*ReplayStepFn)(uint32_t ms, const DecoderOutputs& o, const uint8_t* buf, size_t n);

// Replays 'frames' from the first frame time to the last + tailMs, in 1 ms steps.
inline ReplayResult replay(Decoder& d, const std::vector<ReplayFrame>& frames, uint32_t tailMs,
                           ReplayStepFn step = nullptr) {
    ReplayResult r;
    if (frames.empty()) return r;
    size_t next = 0;
    const uint32_t start = frames.front().ms, end = frames.back().ms + tailMs;
    uint8_t buf[64];
    for (uint32_t ms = start; ms <= end; ms++) {
        while (next < frames.size() && frames[next].ms <= ms) {
            d.onCanFrame(frames[next].f, ms, true);
            r.frames++;
            next++;
        }
        const DecoderOutputs& o = d.tick(ms, false);
        const size_t n = d.nextRadioTx(ms, true, buf, sizeof(buf));
        if (n > 0) {
            r.uartBytes += n;
            r.uartFrames++;
        }
        if (o.power.acc) r.accMs++;
        if (o.power.illumination) r.illMs++;
        if (o.power.reverse) r.revMs++;
        if (o.beep.toneOn) r.toneMs++;
        if (step) step(ms, o, buf, n);
    }
    return r;
}
