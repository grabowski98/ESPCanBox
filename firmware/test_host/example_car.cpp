// Host test: the template car profile lib/car_example on a synthetic
// capture (test_host/synthetic/example_car.log, INVENTED frames, not a real car).
//   example_car <capture.log>
// Keeps the template compiling and shows the profile hooks working without any PSA code: buttons and a knob
// from the key map (codes from the XP radio protocol), ignition -> ACC, lights -> illumination, reverse,
// rear parking sensor -> tone and the XP parking frame, and the car data for the radio screen through the
// car-neutral interface: doors (XP frame 38) and one trip computer page (XP frame 34), without any PSA or
// XP code in the profile. Prints the decoder console lines, every frame for the
// radio and every change of the outputs, with the time in ms from the capture clock.
#include <cstdio>
#include <vector>

#include "profile_example.h"
#include "replay_frames.h"

namespace {

void printLine(const char* line) { printf("console %s", line); }

DecoderOutputs g_prev;
bool g_havePrev = false;
bool g_sawDoors = false;   // XP frame 38 (doors) went to the radio
bool g_sawTrip = false;    // XP frame 34 (trip 1) went to the radio
bool g_sawEngine = false;  // engineRunning from the profile's decodeBody (not only PSA frame 0F6)

void onStep(uint32_t ms, const DecoderOutputs& o, const uint8_t* buf, size_t n) {
    if (o.engineRunning) g_sawEngine = true;
    if (n > 0) {
        printf("%6lu radio", (unsigned long)ms);
        for (size_t i = 0; i < n; i++) printf(" %02X", buf[i]);
        printf("\n");
        if (n > 1 && buf[1] == 0x38) g_sawDoors = true;
        if (n > 1 && buf[1] == 0x34) g_sawTrip = true;
    }
    if (!g_havePrev || o.power.acc != g_prev.power.acc || o.power.illumination != g_prev.power.illumination ||
        o.power.reverse != g_prev.power.reverse || o.beep.toneOn != g_prev.beep.toneOn ||
        o.beep.continuous != g_prev.beep.continuous) {
        printf("%6lu outputs acc=%u ill=%u rev=%u tone=%u continuous=%u\n", (unsigned long)ms, o.power.acc,
               o.power.illumination, o.power.reverse, o.beep.toneOn, o.beep.continuous);
        g_prev = o;
        g_havePrev = true;
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: example_car <capture.log>\n");
        return 2;
    }
    std::vector<ReplayFrame> frames;
    if (!loadCapture(argv[1], frames)) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    printf("profile %s: %lu bit/s, %u key rows, %u knob(s), txAllow %u\n", kCarExample.id,
           (unsigned long)kCarExample.canBitrate, kCarExample.keyCount, kCarExample.knobCount,
           kCarExample.txAllowCount);
    static Decoder d;
    DecoderConfig cfg;
    cfg.car = &kCarExample;
    cfg.radio = &kRadioXp;
    d.begin(cfg, printLine);
    const ReplayResult r = replay(d, frames, 1000, onStep);
    printf("frames=%lu uart_bytes=%lu uart_frames=%lu acc_ms=%lu ill_ms=%lu rev_ms=%lu tone_ms=%lu keys_dropped=%lu\n",
           r.frames, r.uartBytes, r.uartFrames, r.accMs, r.illMs, r.revMs, r.toneMs,
           (unsigned long)d.keys().dropped());
    const vehicle::DoorState& ds = d.doorState();
    printf("doors: valid=%u open=0x%02X (neutral DOOR_* bits)\n", ds.valid, ds.open);
    // The template must produce something on every axis, otherwise it is not an example.
    const bool ok = r.accMs > 0 && r.illMs > 0 && r.revMs > 0 && r.toneMs > 0 && r.uartFrames > 0 && d.haveLastKey() &&
                    d.doorsKnown() && g_sawDoors && g_sawTrip && g_sawEngine;
    printf("%s\n", ok ? "example profile works on every axis" : "FAIL: an axis of the example profile did nothing");
    return ok ? 0 : 1;
}
