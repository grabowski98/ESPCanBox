// Host test: changing a vehicle setting on request from the radio (lib/car_psa_aee2010/settings_changer.h)
// together with the transmit gate (lib/core/can_tx_gate.h).
//
// TRANSMITS NOTHING: the transmit hardware is replaced by the MockPort stub, which records the frames handed
// to the "controller". Simulated time, step = 1 ms = one pass of the main loop, order as
// in Decoder::tick: SettingsChanger::tick, then CanTxGate::tick.
//
// Without arguments: cases with manually given 260 / 361 frames and the Decoder class (in the firmware
// the gate has no hardware: every command 80 = refusal, console line as in earlier versions).
// With a CAN + UART capture as an argument (can_20261111_191045.log): the radio commands 80 from the recording go
// to SettingsChanger, and the 15B frame it would hand over for sending is compared with the 15B frame sent
// at that time by the Simple Soft box; the result (ACCEPTED) comes from the 260 frame of the recording.
//
// Run: bash tools/run_host_tests.sh (README.md, section on tests on recordings).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "can_ids.h"
#include "can_tx_gate.h"
#include "candump.h"
#include "car_state.h"
#include "decoder.h"
#include "radio_commands.h"
#include "settings_changer.h"
#include "vehicle_settings.h"
#include "xp_protocol.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

struct MockPort : CanTxPort {
    const uint32_t* now = nullptr;
    bool on = false;
    uint32_t tOn = 0, maxOnMs = 0;
    struct Sent {
        uint32_t ms;
        CanFrame f;
    };
    std::vector<Sent> sent;
    uint8_t result = 1;            // result of each send: 1 acknowledged, 2 failed
    bool pending = false;
    uint32_t tSend = 0;
    CanHealth h;

    void transmitter(bool v) override {
        if (v && !on) tOn = *now;
        if (!v && on && *now - tOn > maxOnMs) maxOnMs = *now - tOn;
        on = v;
    }
    bool send(const CanFrame& f) override {
        sent.push_back({*now, f});
        pending = true;
        tSend = *now;
        return true;
    }
    uint8_t poll() override {
        if (!pending || (uint32_t)(*now - tSend) < 2) return 0;     // result after 2 ms
        pending = false;
        return result;
    }
    CanHealth health() override { return h; }
};

static CanFrame frame(uint32_t id, std::initializer_list<uint8_t> d) {
    CanFrame f;
    f.id = id;
    f.len = (uint8_t)d.size();
    uint8_t i = 0;
    for (uint8_t b : d) f.data[i++] = b;
    return f;
}

static bool is15B(const CanFrame& f, std::initializer_list<uint8_t> want) {
    if (f.id != 0x15B || f.len != 8 || want.size() != 8) return false;
    uint8_t i = 0;
    for (uint8_t b : want)
        if (f.data[i++] != b) return false;
    return true;
}

static const char* resultName(vset::ChangeResult r) {
    switch (r) {
        case vset::ChangeResult::NONE: return "NONE";
        case vset::ChangeResult::ALREADY: return "ALREADY";
        case vset::ChangeResult::ACCEPTED: return "ACCEPTED";
        case vset::ChangeResult::NOT_ACCEPTED: return "NOT_ACCEPTED";
        case vset::ChangeResult::TX_FAILED: return "TX_FAILED";
        case vset::ChangeResult::REFUSED: return "REFUSED";
        case vset::ChangeResult::SIDE_EFFECT: return "SIDE_EFFECT";
    }
    return "?";
}

struct Sim {
    uint32_t now = 10000;
    MockPort port;
    CanTxGate gate;
    vset::SettingsChanger ch;
    CarModel car;
    TxConditions c;
    std::vector<vset::ChangeReport> reps;

    explicit Sim(bool built = true, bool armed = true, uint32_t mask = vset::ENABLED_DEFAULT) {
        port.now = &now;
        c.ignitionOk = true;
        c.busAlive = true;
        gate.begin(built ? &port : nullptr, &kCarPsaAee2010);
        if (armed) gate.setArmed(true);
        ch.begin(&gate, mask);
        can(frame(0x361, {0xA0, 0x66, 0xC8, 0x0A, 0x00, 0x00}));     // 361 from the car
    }
    Sim(const Sim&) = delete;
    void can(const CanFrame& f) { car.onCanFrame(f, now, true); }
    void f260(uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6 = 0x40) {
        can(frame(0x260, {0xA4, b2, b3, b4, b5, b6, 0x00}));
    }
    void take() {
        vset::ChangeReport r;
        if (ch.takeReport(r)) reps.push_back(r);
    }
    void cmd(uint8_t id, uint8_t v) {
        ch.onRadioSetting(id, v, car.state(), c, now);
        take();
    }
    void step(uint32_t ms = 1) {
        for (uint32_t i = 0; i < ms; i++) {
            now++;
            ch.tick(car.state(), c, now);
            gate.tick(c, now);
            take();
        }
    }
    bool last(vset::ChangeResult r) const { return !reps.empty() && reps.back().result == r; }
};

static void testBasic() {
    std::printf("--- one command ---\n");
    {
        Sim s;
        s.f260(0x34, 0x97, 0xBA, 0x55);                        // 260 from 11.11.2026, parking assistant = 0
        s.cmd(0x01, 1);
        bool noReport = s.reps.empty() && s.ch.busy();
        s.step(5);
        check(noReport && s.port.sent.size() == 1 && is15B(s.port.sent[0].f, {0x00, 0x34, 0x97, 0xBA, 0xD5, 0x40, 0x00, 0x00}),
              "80 01 01 on 260 = A4 34 97 BA 55 40 00: 15B = 00 34 97 BA D5 40 00 00 goes to the gate (T1, measured)");
        s.step(240);
        bool waiting = s.reps.empty();
        s.f260(0x34, 0x97, 0xBA, 0xD5);                        // the car accepted
        s.step(1);
        char line[200];
        std::snprintf(line, sizeof(line), "260 with bit 5.7 after 245 ms: report %s, time %u ms, gate without a latch",
                      s.reps.empty() ? "-" : resultName(s.reps.back().result), s.reps.empty() ? 0 : s.reps.back().tookMs);
        check(waiting && s.reps.size() == 1 && s.last(vset::ChangeResult::ACCEPTED) && s.reps[0].tookMs == 246 &&
                  s.reps[0].haveFrame && s.reps[0].frame[4] == 0xD5 && s.gate.stats().fault == TxFault::NONE && !s.ch.busy(),
              line);
        // The same in the other direction.
        s.step(600);
        s.f260(0x34, 0x97, 0xBA, 0xD5);
        s.cmd(0x01, 0);
        s.step(100);
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.step(1);
        check(s.port.sent.size() == 2 && is15B(s.port.sent[1].f, {0x00, 0x34, 0x97, 0xBA, 0x55, 0x40, 0x00, 0x00}) &&
                  s.reps.size() == 2 && s.last(vset::ChangeResult::ACCEPTED),
              "80 01 00: 15B = 00 34 97 BA 55 40 00 00 (T2, measured), report ACCEPTED");
        check(s.port.maxOnMs <= 20, "transmitter on for 20 ms at most");
    }
    {
        Sim s;
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.cmd(0x01, 1);
        for (int i = 0; i < 4; i++) {
            s.step(500);
            if (s.reps.empty()) s.f260(0x34, 0x97, 0xBA, 0x55);        // 260 every 500 ms, unchanged
        }
        char line[200];
        std::snprintf(line, sizeof(line), "260 unchanged: report %s after %u ms, one 15B frame (no repeats)",
                      s.reps.empty() ? "-" : resultName(s.reps.back().result), s.reps.empty() ? 0 : s.reps.back().tookMs);
        check(s.reps.size() == 1 && s.last(vset::ChangeResult::NOT_ACCEPTED) && s.reps[0].tookMs == 1305 &&
                  s.port.sent.size() == 1 && s.gate.state() != TxState::FAULT,
              line);
    }
    {
        Sim s;
        s.f260(0x34, 0x97, 0xBA, 0xD5);
        s.cmd(0x01, 1);
        s.step(50);
        check(s.reps.size() == 1 && s.last(vset::ChangeResult::ALREADY) && s.port.sent.empty() && s.gate.state() == TxState::IDLE,
              "260 already shows the requested state: ALREADY, nothing sent");
    }
    {
        Sim s;
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.port.result = 2;
        s.cmd(0x01, 1);
        s.step(10);
        check(s.reps.size() == 1 && s.last(vset::ChangeResult::TX_FAILED) && s.reps[0].haveFrame && s.port.sent.size() == 1,
              "frame without an ack: TX_FAILED, no repeat");
    }
}

static void testRefusals() {
    std::printf("--- refusals ---\n");
    {
        Sim s;
        s.cmd(0x01, 1);                                        // no 260 frame yet
        check(s.reps.size() == 1 && s.last(vset::ChangeResult::REFUSED) && s.reps[0].refuse == vset::Refuse::NO_260 &&
                  s.reps[0].txRefuse == TxRefuse::OK && s.port.sent.empty() && !s.port.on,
              "command before the first 260: REFUSED (no frame 260), nothing sent");
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.step(1600);
        s.cmd(0x01, 1);
        check(s.reps.size() == 2 && s.reps[1].refuse == vset::Refuse::STALE_260 && s.port.sent.empty(),
              "260 older than 1.5 s: REFUSED (stale frame 260)");
    }
    {
        Sim s;
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.cmd(0x02, 1);
        bool masked = s.reps.size() == 1 && s.reps[0].refuse == vset::Refuse::DISABLED_BY_MASK && s.port.sent.empty();
        s.ch.setEnabledMask(vset::ENABLED_DEFAULT | (1UL << 0x02));
        s.cmd(0x02, 1);
        s.step(5);
        check(masked && s.port.sent.size() == 1 && is15B(s.port.sent[0].f, {0x00, 0x34, 0x97, 0xBA, 0x75, 0x40, 0x00, 0x00}),
              "default mask: 80 02 01 refused (disabled in mask); after setEnabledMask with bit 02: 15B = 00 34 97 BA 75 40 00 00");
    }
    {
        Sim s(true, true, 0xFFFFFFFFUL);
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.cmd(0x08, 1);                                        // removed from the table on 19.11.2026 (never sent by the radio)
        s.cmd(0x0D, 1);
        s.cmd(0x05, 1);
        s.cmd(0x0A, 1);
        s.cmd(0x77, 1);
        s.cmd(0x01, 2);
        check(s.reps.size() == 6 && s.reps[0].refuse == vset::Refuse::UNKNOWN_ID && s.reps[1].refuse == vset::Refuse::NO_FRAME_FOR_ID &&
                  s.reps[2].refuse == vset::Refuse::NO_STATE && s.reps[3].refuse == vset::Refuse::FIXED_REFUSE &&
                  s.reps[4].refuse == vset::Refuse::UNKNOWN_ID && s.reps[5].refuse == vset::Refuse::BAD_VALUE && s.port.sent.empty(),
              "full mask: 08 unknown, 0D no known frame, 05 (1A9 button) no state frame, 0A fixed refusal, 77 unknown, 01 = 2 bad value");
    }
    {
        Sim s(true, false);
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.cmd(0x01, 1);
        check(s.reps.size() == 1 && s.last(vset::ChangeResult::REFUSED) && s.reps[0].txRefuse == TxRefuse::DISARMED &&
                  s.port.sent.empty() && !s.port.on,
              "gate disarmed: REFUSED (DISARMED)");
        Sim t;
        t.f260(0x34, 0x97, 0xBA, 0x55);
        t.c.ignitionOk = false;
        t.cmd(0x01, 1);
        t.c.ignitionOk = true;
        t.c.flashBusy = true;
        t.cmd(0x01, 1);
        check(t.reps.size() == 2 && t.reps[0].txRefuse == TxRefuse::IGNITION && t.reps[1].txRefuse == TxRefuse::FLASH_BUSY &&
                  t.port.sent.empty(),
              "no ignition / flash write: REFUSED with the gate reason, no retry");
    }
    {
        Sim s(false);                                          // as in the device firmware: gate without hardware
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.cmd(0x01, 1);
        s.f260(0x34, 0x97, 0xBA, 0xD5);
        s.cmd(0x01, 1);                                        // even when the car already has this state: refusal, not ALREADY
        s.cmd(0x02, 1);
        s.cmd(0x0D, 1);
        s.step(100);
        bool all = s.reps.size() == 4;
        for (const vset::ChangeReport& r : s.reps)
            if (r.result != vset::ChangeResult::REFUSED || r.txRefuse != TxRefuse::NOT_BUILT || r.haveFrame || r.tookMs != 0) all = false;
        check(all && s.reps[0].refuse == vset::Refuse::OK && s.reps[1].refuse == vset::Refuse::OK &&
                  s.reps[2].refuse == vset::Refuse::DISABLED_BY_MASK && s.reps[3].refuse == vset::Refuse::NO_FRAME_FOR_ID &&
                  s.gate.state() == TxState::NOT_BUILT && s.gate.stats().refused == 0 && !s.ch.busy(),
              "gate without hardware (NOT_BUILT): every command at once REFUSED with txRefuse = NOT_BUILT, nobody asks the gate");
    }
}

static void testQueueAndTiming() {
    std::printf("--- two commands, retrying, 260 changing meanwhile ---\n");
    {
        Sim s(true, true, vset::ENABLED_DEFAULT | (1UL << 0x02));
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.cmd(0x01, 1);
        s.step(50);
        s.cmd(0x02, 1);                                        // during the first one: waits
        bool queued = s.reps.empty() && s.port.sent.size() == 1;
        s.step(195);
        s.f260(0x34, 0x97, 0xBA, 0xD5);                        // 245 ms: the car accepted the first one
        s.step(1);
        bool first = s.reps.size() == 1 && s.last(vset::ChangeResult::ACCEPTED) && s.reps[0].id == 0x01;
        s.step(300);
        bool second = s.port.sent.size() == 2 && is15B(s.port.sent[1].f, {0x00, 0x34, 0x97, 0xBA, 0xF5, 0x40, 0x00, 0x00});
        uint32_t gap = s.port.sent.size() == 2 ? s.port.sent[1].ms - s.port.sent[0].ms : 0;
        s.step(200);
        s.f260(0x34, 0x97, 0xBA, 0xF5);
        s.step(1);
        char line[220];
        std::snprintf(line, sizeof(line),
                      "80 01 01 and 50 ms later 80 02 01: two 15B frames %lu ms apart, the second = 00 34 97 BA F5 40 00 00 (has the change of the first)",
                      (unsigned long)gap);
        check(queued && first && second && gap >= 500 && s.reps.size() == 2 && s.last(vset::ChangeResult::ACCEPTED) &&
                  s.reps[1].id == 0x02 && s.gate.stats().fault == TxFault::NONE,
              line);
    }
    {
        Sim s(true, true, vset::ENABLED_DEFAULT | (1UL << 0x02) | (1UL << 0x06));
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.cmd(0x01, 1);
        s.step(10);
        s.cmd(0x02, 1);
        s.step(10);
        s.cmd(0x06, 0);                                        // replaces the waiting 80 02 01
        s.step(225);
        s.f260(0x34, 0x97, 0xBA, 0xD5);
        s.step(700);
        s.f260(0x34, 0x97, 0x3A, 0xD5);
        s.step(5);
        check(s.port.sent.size() == 2 && is15B(s.port.sent[1].f, {0x00, 0x34, 0x97, 0x3A, 0xD5, 0x40, 0x00, 0x00}) &&
                  s.reps.size() == 2 && s.reps[1].id == 0x06 && s.last(vset::ChangeResult::ACCEPTED),
              "three commands: one waiting place, the last one wins (80 02 01 skipped without a report)");
    }
    {
        Sim s;
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.port.h.rec = 3;                                      // controller timeout refusal
        s.cmd(0x01, 1);
        bool waits = s.reps.empty() && s.port.sent.empty() && s.ch.busy();
        s.step(100);
        s.f260(0x1C, 0x97, 0xBA, 0x55);                        // 260 changed between the command and the send
        s.step(50);
        s.port.h.rec = 0;
        s.step(5);
        check(waits && s.port.sent.size() == 1 && is15B(s.port.sent[0].f, {0x00, 0x1C, 0x97, 0xBA, 0xD5, 0x40, 0x00, 0x00}),
              "REC != 0 for 150 ms, meanwhile a new 260: 15B frame built from the NEW 260 (00 1C 97 BA D5 40 00 00)");
        Sim t;
        t.f260(0x34, 0x97, 0xBA, 0x55);
        t.port.h.rec = 3;
        t.cmd(0x01, 1);
        t.step(299);
        bool still = t.reps.empty();
        t.step(2);
        check(still && t.reps.size() == 1 && t.last(vset::ChangeResult::REFUSED) && t.reps[0].txRefuse == TxRefuse::CONTROLLER &&
                  t.port.sent.empty() && t.gate.state() == TxState::IDLE,
              "REC != 0 longer than 300 ms: REFUSED (CONTROLLER), no latch");
    }
}

static void testSideEffect() {
    std::printf("--- foreign bits of frame 260 ---\n");
    {
        Sim s;
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.cmd(0x01, 1);
        s.step(200);
        s.f260(0x34, 0x87, 0xBA, 0xD5);                        // requested bit 5.7 and a foreign bit 3.4
        s.step(1);
        const vset::ChangeReport r = s.reps.empty() ? vset::ChangeReport() : s.reps.back();
        char line[200];
        std::snprintf(line, sizeof(line), "a foreign bit changed together with the requested one: %s, other bits %02X %02X %02X %02X %02X, gate FAULT",
                      resultName(r.result), r.otherBits[0], r.otherBits[1], r.otherBits[2], r.otherBits[3], r.otherBits[4]);
        check(r.result == vset::ChangeResult::SIDE_EFFECT && r.otherBits[0] == 0 && r.otherBits[1] == 0x10 && r.otherBits[2] == 0 &&
                  r.otherBits[3] == 0 && r.otherBits[4] == 0 && s.gate.state() == TxState::FAULT &&
                  s.gate.stats().fault == TxFault::SIDE_EFFECT,
              line);
        s.step(1000);
        s.f260(0x34, 0x87, 0xBA, 0xD5);
        s.cmd(0x01, 0);
        check(s.reps.size() == 2 && s.last(vset::ChangeResult::REFUSED) && s.reps[1].txRefuse == TxRefuse::FAULT &&
                  s.port.sent.size() == 1,
              "after the latch the next command: REFUSED (FAULT), nothing sent");
    }
    {
        Sim s;
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.cmd(0x01, 1);
        s.step(200);
        s.f260(0x34, 0x97, 0xBA, 0xD5, 0x4B);                  // state bits 6.3-6.0 may change by themselves
        s.step(1);
        check(s.last(vset::ChangeResult::ACCEPTED) && s.gate.state() != TxState::FAULT,
              "bits 6.3-6.0 changed together with the requested one: ACCEPTED, no latch");
    }
    {
        Sim s;
        s.f260(0x34, 0x97, 0xBA, 0x55);
        s.cmd(0x01, 1);
        s.step(500);
        s.f260(0x34, 0x97, 0xBB, 0x55);                        // requested bit unchanged, foreign bit 4.0 changed
        s.step(1000);
        check(s.reps.size() == 1 && s.last(vset::ChangeResult::SIDE_EFFECT) && s.reps[0].otherBits[2] == 0x01 &&
                  s.gate.state() == TxState::FAULT,
              "requested bit unchanged, a foreign one changed: after the deadline SIDE_EFFECT and the latch");
    }
}

// The Decoder class, i.e. what is in the device firmware: gate without hardware.
static std::vector<std::string> g_lines;
static void logLine(const char* line) {
    if (std::strncmp(line, "[vset]", 6) == 0) g_lines.push_back(line);   // we skip other lines ([car] ...)
}

static void testDecoder() {
    std::printf("--- Decoder (device program: gate without hardware) ---\n");
    Decoder d;
    DecoderConfig cfg;
    cfg.decoderMode = true;
    d.begin(cfg, logLine);
    uint32_t now = 5000;
    d.onCanFrame(frame(0x0F6, {0x8E, 0x59, 0x1E, 0x23, 0x23, 0x5C, 0x5C, 0x24}), now, true);
    d.onCanFrame(frame(0x361, {0xA0, 0x66, 0xC8, 0x0A, 0x00, 0x00}), now, true);
    d.onCanFrame(frame(0x260, {0xA4, 0x34, 0x97, 0xBA, 0x55, 0x40, 0x00}), now, true);
    d.tick(now, false);
    g_lines.clear();
    xp::Frame rf;
    rf.cmd = 0x80;
    rf.len = 2;
    rf.data[0] = 0x01;
    rf.data[1] = 0x01;
    d.onRadioFrame(rf, now);
    bool line1 = g_lines.size() == 1 &&
                 g_lines[0] == "[vset] radio: 80 01 01 parking assist = 1: REFUSED (no CAN transmit in this build)\n";
    rf.data[0] = 0x0D;
    d.onRadioFrame(rf, now);
    rf.data[0] = 0x05;
    d.onRadioFrame(rf, now);
    bool lines = g_lines.size() == 3 && g_lines[1] == "[vset] radio: 80 0D 01 Start/Stop = 1: no known frame\n" &&
                 g_lines[2] == "[vset] radio: 80 05 01 radar switch = 1: known frame (1A9), transmit blocked until measured\n";
    check(line1 && lines, "commands 80 01 01, 80 0D 01, 80 05 01: three [vset] lines (0D: no known frame since 19.11.2026)");
    for (int i = 0; i < 2000; i++) d.tick(++now, false);
    const SettingCmdStats& sc = d.settingCmds();
    check(g_lines.size() == 3 && sc.refused == 3 && sc.accepted == 0 && sc.notAccepted == 0 && sc.lastId == 0x05 &&
              sc.lastResult == SettingCmdResult::BLOCKED && !sc.lastHaveFrame && sc.lastTookMs == 0,
          "counters for the panel: 3 refused, the last 05 = blocked, no frame; after 2 s no further lines");
    char buf[160];
    d.formatCanTx(buf, sizeof(buf));
    std::printf("      %s", buf);
    check(d.canTxState() == TxState::NOT_BUILT && std::strcmp(buf, "[stat] can-tx=none sent=0 acked=0 failed=0 refused=0\n") == 0 &&
              d.canTxStats().refused == 0 && d.canTxStats().fault == TxFault::NONE,
          "gate in Decoder: NOT_BUILT, stat line can-tx=none, zeros only");
    check(d.settingsMask() == vset::ENABLED_DEFAULT, "default mask = item 01 only");
}

// ---------- replay of a CAN + UART capture ----------
static uint32_t toMs(double sec) { return (uint32_t)(std::llround(sec * 1e6) / 1000); }

static bool parseUartLine(const char* line, double& ts, std::vector<uint8_t>& out) {
    if (line[0] != '(') return false;
    char name[16];
    char hex[400];
    if (std::sscanf(line, "(%lf) %15s %399s", &ts, name, hex) != 3) return false;
    if (std::strcmp(name, "uartA") != 0 && std::strcmp(name, "uartB") != 0) return false;
    out.clear();
    size_t n = std::strlen(hex);
    for (size_t i = 0; i + 1 < n; i += 2) {
        char b[3] = {hex[i], hex[i + 1], 0};
        out.push_back((uint8_t)std::strtoul(b, nullptr, 16));
    }
    return true;
}

static int replay(const char* path) {
    std::FILE* fp = std::fopen(path, "r");
    if (!fp) {
        std::printf("FAIL  cannot open %s\n", path);
        return 1;
    }
    uint32_t now = 0;
    bool started = false;
    MockPort port;
    port.now = &now;
    CanTxGate gate;
    gate.begin(&port, &kCarPsaAee2010);
    gate.setArmed(true);
    vset::SettingsChanger ch;
    ch.begin(&gate, vset::ENABLED_DEFAULT);
    CarModel car;
    std::vector<vset::ChangeReport> reps;
    struct Box {              // 15B frame of the Simple Soft box from the recording
        uint32_t ms;
        CanFrame f;
    };
    std::vector<Box> box;
    unsigned cmds = 0;
    xp::Parser parser[2];
    auto cond = [&]() {
        TxConditions c;
        const CarState& s = car.state();
        c.ignitionOk = fresh(s.f0F6, now, 1000) && (s.body.ignitionRaw == 0x8E || s.body.ignitionRaw == 0x88);
        c.busAlive = s.haveAnyFrame && (uint32_t)(now - s.msAnyFrame) < 1000;
        return c;
    };
    auto take = [&]() {
        vset::ChangeReport r;
        if (!ch.takeReport(r)) return;
        reps.push_back(r);
        std::printf("  %7.3f s  80 %02X %02X -> %s after %u ms", r.ms / 1000.0, r.id, r.value, resultName(r.result), r.tookMs);
        if (r.haveFrame)
            std::printf(", 15B = %02X %02X %02X %02X %02X %02X %02X %02X", r.frame[0], r.frame[1], r.frame[2], r.frame[3],
                        r.frame[4], r.frame[5], r.frame[6], r.frame[7]);
        std::printf("\n");
    };
    auto advance = [&](uint32_t t) {
        if (!started) {
            now = t;
            started = true;
            return;
        }
        while (now < t) {
            now++;
            TxConditions c = cond();
            ch.tick(car.state(), c, now);
            gate.tick(c, now);
            take();
        }
    };
    // The capture lines are not strictly ordered by time (a UART line is sometimes written after a CAN frame
    // with a later timestamp), so we first sort them by timestamp.
    struct Line {
        double ts;
        std::string text;
    };
    std::vector<Line> lines;
    char buf[1024];
    while (std::fgets(buf, sizeof(buf), fp)) {
        double t;
        if (buf[0] == '(' && std::sscanf(buf, "(%lf)", &t) == 1) lines.push_back({t, buf});
    }
    std::fclose(fp);
    std::stable_sort(lines.begin(), lines.end(), [](const Line& a, const Line& b) { return a.ts < b.ts; });
    for (const Line& ln : lines) {
        const char* line = ln.text.c_str();
        CanFrame cf;
        double ts;
        std::vector<uint8_t> bytes;
        if (parseCandumpLine(line, cf, &ts) && ts >= 0) {
            advance(toMs(ts));
            if (cf.id == 0x15B) {
                box.push_back({now, cf});
                std::printf("  %7.3f s  box: 15B = %02X %02X %02X %02X %02X %02X %02X %02X\n", now / 1000.0, cf.data[0],
                            cf.data[1], cf.data[2], cf.data[3], cf.data[4], cf.data[5], cf.data[6], cf.data[7]);
            }
            car.onCanFrame(cf, now, true);
        } else if (parseUartLine(line, ts, bytes)) {
            advance(toMs(ts));
            int which = std::strstr(line, "uartA") ? 0 : 1;
            for (uint8_t b : bytes) {
                xp::Frame f;
                if (parser[which].feed(b, now, f) != xp::Parser::FRAME) continue;
                RadioCmd c = parseRadioCmd(f);
                if (c.kind != RadioCmdKind::SETTING) continue;
                cmds++;
                std::printf("  %7.3f s  radio: 80 %02X %02X\n", now / 1000.0, c.id, c.value);
                ch.onRadioSetting(c.id, c.value, car.state(), cond(), now);
                take();
            }
        }
    }
    advance(now + 3000);

    bool same = port.sent.size() == box.size();
    for (size_t i = 0; same && i < box.size(); i++)
        if (std::memcmp(port.sent[i].f.data, box[i].f.data, 8) != 0 || port.sent[i].f.len != box[i].f.len) same = false;
    bool acc = reps.size() == cmds;
    for (const vset::ChangeReport& r : reps)
        if (r.result != vset::ChangeResult::ACCEPTED) acc = false;
    char what[200];
    std::snprintf(what, sizeof(what), "radio commands 80: %u, 15B frames to the gate: %u, 15B frames of the box: %u, identical content", cmds,
                  (unsigned)port.sent.size(), (unsigned)box.size());
    check(cmds == 2 && same && box.size() == 2, what);
    check(port.sent.size() == 2 && is15B(port.sent[0].f, {0x00, 0x34, 0x97, 0xBA, 0xD5, 0x40, 0x00, 0x00}) &&
              is15B(port.sent[1].f, {0x00, 0x34, 0x97, 0xBA, 0x55, 0x40, 0x00, 0x00}),
          "80 01 01 -> 15B = 00 34 97 BA D5 40 00 00; 80 01 00 -> 15B = 00 34 97 BA 55 40 00 00");
    std::snprintf(what, sizeof(what), "reports: %u, all ACCEPTED (after frame 260 from the recording)", (unsigned)reps.size());
    check(acc && reps.size() == 2, what);
    check(reps.size() == 2 && reps[0].ms >= 562902 && reps[0].ms <= 562903 && reps[1].ms >= 567902 && reps[1].ms <= 567903,
          "report ACCEPTED at the moment of frame 260 at 562,902 s and at 567,902 s");
    check(gate.stats().fault == TxFault::NONE && gate.stats().sent == 2 && gate.stats().acked == 2 && port.maxOnMs <= 20,
          "gate: 2 sent, 2 acked, no latch, transmitter at most 20 ms");
    return 0;
}

static bool is1A9(const CanFrame& f, std::initializer_list<uint8_t> want) {
    if (f.id != 0x1A9 || f.len != 8 || want.size() != 8) return false;
    uint8_t i = 0;
    for (uint8_t b : want)
        if (f.data[i++] != b) return false;
    return true;
}

// Button-type items: one 1A9 frame toggles the state; sent only for a known, fresh and different state.
static void testButtons() {
    {
        Sim s(true, true, 0xFFFFFFFFUL);
        s.can(frame(0x227, {0x00, 0x00, 0x11, 0x00, 0x00}));
        s.cmd(0x05, 1);
        s.step(10);
        bool sent = s.port.sent.size() == 1 && is1A9(s.port.sent[0].f, {0x00, 0x7F, 0xFF, 0x04, 0xFF, 0x00, 0x10, 0x00});
        s.can(frame(0x227, {0x40, 0x00, 0x11, 0x00, 0x00}));
        s.step(5);
        check(sent && s.reps.size() == 1 && s.reps[0].result == vset::ChangeResult::ACCEPTED && s.reps[0].canId == 0x1A9 &&
                  s.port.sent.size() == 1,
              "80 05 01 with 227 = 00..: one 1A9 = 00 7F FF 04 FF 00 10 00; after 227 = 40..: ACCEPTED");
        s.can(frame(0x227, {0x40, 0x00, 0x11, 0x00, 0x00}));
        s.step(700);
        s.can(frame(0x227, {0x40, 0x00, 0x11, 0x00, 0x00}));
        s.cmd(0x05, 1);
        s.step(5);
        check(s.reps.size() == 2 && s.reps[1].result == vset::ChangeResult::ALREADY && s.port.sent.size() == 1,
              "80 05 01 with 227 = 40..: the car already has this state, nothing sent");
    }
    {
        Sim s(true, true, 0xFFFFFFFFUL);
        s.can(frame(0x227, {0x00, 0x00, 0x11, 0x00, 0x00}));
        s.cmd(0x0C, 0);
        s.step(10);
        bool sent = s.port.sent.size() == 1 && is1A9(s.port.sent[0].f, {0x00, 0x7F, 0xFF, 0x20, 0xFF, 0x00, 0x10, 0x00});
        s.can(frame(0x227, {0x00, 0x00, 0x10, 0x00, 0x00}));
        s.step(5);
        check(sent && s.reps.size() == 1 && s.reps[0].result == vset::ChangeResult::ACCEPTED,
              "80 0C 00 with 227 byte 3 = 11: one 1A9 = 00 7F FF 20 FF 00 10 00; after 227 byte 3 = 10: ACCEPTED");
    }
    {
        Sim s(true, true, 0xFFFFFFFFUL);
        s.can(frame(0x3E1, {0x00, 0x00, 0x00, 0x40, 0x00}));
        s.cmd(0x0D, 1);
        s.step(10);
        check(s.reps.size() == 1 && s.reps[0].refuse == vset::Refuse::NO_FRAME_FOR_ID && s.port.sent.empty(),
              "80 0D 01 (Start/Stop): no known frame, nothing sent (no 1A9 content was ever recorded for it)");
    }
    {
        Sim s(true, true);                         // default mask: buttons disabled
        s.can(frame(0x227, {0x00, 0x00, 0x11, 0x00, 0x00}));
        s.cmd(0x05, 1);
        check(s.reps.size() == 1 && s.reps[0].refuse == vset::Refuse::DISABLED_BY_MASK && s.port.sent.empty(),
              "default mask: 80 05 01 refused (disabled in mask)");
    }
    {
        Sim s(true, true, 0xFFFFFFFFUL);
        s.can(frame(0x227, {0x00, 0x00, 0x11, 0x00, 0x00}));
        s.step(2000);                              // state frame older than 1.5 s
        s.cmd(0x05, 1);
        s.car.onCanFrame(frame(0x227, {0x00, 0x00, 0x11, 0x00, 0x00}), s.now, false);   // pasted from the panel
        s.cmd(0x0C, 1);
        check(s.reps.size() == 2 && s.reps[0].refuse == vset::Refuse::NO_STATE && s.reps[1].refuse == vset::Refuse::NO_STATE &&
                  s.port.sent.empty(),
              "a stale 227 or a 227 pasted from the panel: refused (no fresh state frame), nothing sent");
    }
    {
        Sim s(true, true, 0xFFFFFFFFUL);
        TxRefuse a = s.gate.request(frame(0x1A9, {0x02, 0x7F, 0xFF, 0x00, 0xFF, 0x00, 0x10, 0x00}), s.c, s.now);   // trip reset
        TxRefuse b = s.gate.request(frame(0x1A9, {0x00, 0x7F, 0xFF, 0x24, 0xFF, 0x00, 0x10, 0x00}), s.c, s.now);   // two bits at once
        TxRefuse d = s.gate.request(frame(0x1A9, {0x20, 0x7F, 0xFF, 0x00, 0xFF, 0x00, 0x50, 0x00}), s.c, s.now);   // idle frame
        TxRefuse ss = s.gate.request(frame(0x1A9, {0x00, 0x7F, 0xFF, 0x00, 0xFF, 0x00, 0x90, 0x00}), s.c, s.now);   // Stop and Start (never recorded)
        TxRefuse e = s.gate.request(frame(0x364, {0x80, 0x00, 0x1E, 0x00}), s.c, s.now);                          // climate (removed)
        TxRefuse e2 = s.gate.request(frame(0x364, {0x00, 0x00, 0x1E, 0x78, 0xE0, 0x00}), s.c, s.now);
        TxRefuse e5 = s.gate.request(frame(0x2E9, {0x01, 0x05, 0x58, 0x00}), s.c, s.now);
        check(a == TxRefuse::ID_NOT_ALLOWED && b == TxRefuse::ID_NOT_ALLOWED && d == TxRefuse::ID_NOT_ALLOWED &&
                  ss == TxRefuse::ID_NOT_ALLOWED && e == TxRefuse::ID_NOT_ALLOWED && e2 == TxRefuse::ID_NOT_ALLOWED &&
                  e5 == TxRefuse::ID_NOT_ALLOWED && s.port.sent.empty(),
              "gate: 1A9 with content outside the two allowed ones (also 00 90, Stop and Start), 364 in any form, frame 2E9: refused");
    }
}

int main(int argc, char** argv) {
    if (argc > 1) {
        if (replay(argv[1])) return 2;
    } else {
        testBasic();
        testRefusals();
        testQueueAndTiming();
        testSideEffect();
        testDecoder();
        testButtons();
    }
    std::printf("settings_change: %d checks, %d errors\n", checks, failures);
    return failures ? 1 : 0;
}
