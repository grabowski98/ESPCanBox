// Host test: car profile "generic" and radio protocol "none".
//   radio_none <capture.log>
// Replays a capture from the car three times through a Decoder:
//   1. generic + none (environment listen_generic_wifi): zero bytes for the radio UART, all 12 V outputs off,
//      no tone, no console line from the decoder;
//   2. psa_aee2010 + none: the car is decoded (ACC, tone may come on), but zero bytes for the radio;
//   3. psa_aee2010 + xp (reference, default DecoderConfig): bytes for the radio, for comparison.
// Exit code 1 when a rule of variant 1 or 2 is broken.
#include <cstdio>
#include <vector>

#include "profile_generic.h"
#include "protocol_none.h"
#include "replay_frames.h"

namespace {

unsigned long g_lines = 0;
void countLine(const char*) { g_lines++; }

ReplayResult run(const std::vector<ReplayFrame>& frames, const vehicle::CarProfile* car, const RadioProtocol* radio,
                 const char* name) {
    static Decoder d;            // static: the object is large; a new one per variant below
    d = Decoder();
    DecoderConfig cfg;
    cfg.car = car;
    cfg.radio = radio;
    g_lines = 0;
    d.begin(cfg, countLine);
    ReplayResult r = replay(d, frames, 3000);
    printf("%-18s frames=%lu uart_bytes=%lu uart_frames=%lu acc_ms=%lu ill_ms=%lu rev_ms=%lu tone_ms=%lu "
           "console_lines=%lu suppressed=%lu\n",
           name, r.frames, r.uartBytes, r.uartFrames, r.accMs, r.illMs, r.revMs, r.toneMs, g_lines,
           (unsigned long)d.radioTxSuppressed());
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: radio_none <capture.log>\n");
        return 2;
    }
    std::vector<ReplayFrame> frames;
    if (!loadCapture(argv[1], frames)) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    int bad = 0;
    const ReplayResult g = run(frames, &kCarGeneric, &kRadioNone, "generic+none");
    const unsigned long gLines = g_lines;
    if (g.uartBytes != 0 || g.accMs != 0 || g.illMs != 0 || g.revMs != 0 || g.toneMs != 0 || gLines != 0) {
        printf("FAIL generic+none: something was sent or switched on\n");
        bad++;
    }
    const ReplayResult p = run(frames, &kCarPsaAee2010, &kRadioNone, "psa_aee2010+none");
    if (p.uartBytes != 0) {
        printf("FAIL psa_aee2010+none: bytes for the radio\n");
        bad++;
    }
    run(frames, &kCarPsaAee2010, &kRadioXp, "psa_aee2010+xp");
    printf("%s\n", bad ? "RULES BROKEN" : "generic+none: silent and all outputs off; none: zero UART bytes");
    return bad ? 1 : 0;
}
