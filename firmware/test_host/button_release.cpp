// Host test: release frame after a 1A9 button press (lib/car_psa_aee2010/settings_changer.cpp), together with
// the transmit gate (lib/core/can_tx_gate.h) and the Decoder console lines.
//
// TRANSMITS NOTHING: the transmit hardware is a stub that records the frames handed to the "controller".
// Simulated time, step = 1 ms = one pass of the main loop.
//
// Built twice by tools/run_host_tests.sh, both with -DCAN_TX_EXPERIMENTAL=1 (the trial frame 1A9):
//   -DCAN_TX_1A9_RELEASE=0: a press is one frame, no release is ever queued, the gate refuses the release content;
//   -DCAN_TX_1A9_RELEASE=1: about 200 ms after an acknowledged press (in practice when the gate pause of 500 ms
//                           ends) one frame 1A9 20 7F FF 00 FF 00 50 00 (the box's periodic
//                           1A9 with ignition on); none after a press without an ack.
// Both outputs go into expected/button_release.txt.
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "can_tx_gate.h"
#include "car_state.h"
#include "decoder.h"
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
    bool sendOk = true;            // false: the controller does not take the frame (send() returns false)
    bool pending = false;
    uint32_t tSend = 0;
    CanHealth h;

    void transmitter(bool v) override {
        if (v && !on) tOn = *now;
        if (!v && on && *now - tOn > maxOnMs) maxOnMs = *now - tOn;
        on = v;
    }
    bool send(const CanFrame& f) override {
        if (!sendOk) return false;
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

// Press frame with button bit b4; b4 = 0 means the release frame 20 7F FF 00 FF 00 50 00.
static bool is1A9(const CanFrame& f, uint8_t b4) {
    const uint8_t press[8] = {0x00, 0x7F, 0xFF, b4, 0xFF, 0x00, 0x10, 0x00};
    const uint8_t rel[8] = {0x20, 0x7F, 0xFF, 0x00, 0xFF, 0x00, 0x50, 0x00};
    return f.id == 0x1A9 && f.len == 8 && std::memcmp(f.data, b4 ? press : rel, 8) == 0;
}

// SettingsChanger + gate + CarModel, as Decoder::tick calls them (changer first, then the gate).
struct Sim {
    uint32_t now = 10000;
    MockPort port;
    CanTxGate gate;
    vset::SettingsChanger ch;
    CarModel car;
    TxConditions c;
    std::vector<vset::ChangeReport> reps;
    std::vector<vset::ReleaseEvent> rels;
    std::vector<uint32_t> repMs, relMs;

    Sim() {
        port.now = &now;
        c.ignitionOk = true;
        c.busAlive = true;
        gate.begin(&port, &kCarPsaAee2010);
        gate.setArmed(true);
        ch.begin(&gate, 0xFFFFFFFFUL);
    }
    Sim(const Sim&) = delete;
    void can(const CanFrame& f) { car.onCanFrame(f, now, true); }
    void take() {
        vset::ChangeReport r;
        if (ch.takeReport(r)) {
            reps.push_back(r);
            repMs.push_back(now);
        }
        vset::ReleaseEvent e;
        if (ch.takeReleaseEvent(e)) {
            rels.push_back(e);
            relMs.push_back(now);
        }
    }
    void cmd(uint8_t id, uint8_t v) {
        ch.onRadioSetting(id, v, car.state(), c, now);
        take();
    }
    // "BSI": a press in 1A9 byte 4 bit 2 toggles 126 byte 1 (18 <-> 00) 20 ms later.
    size_t seen = 0;
    uint8_t s126 = 0x00;
    int32_t toggleAt = -1;
    void step(uint32_t ms = 1) {
        for (uint32_t i = 0; i < ms; i++) {
            now++;
            while (seen < port.sent.size()) {
                const CanFrame& f = port.sent[seen++].f;
                if (port.result == 1 && f.id == 0x1A9 && (f.data[3] & 0x04)) toggleAt = (int32_t)(now + 20);
            }
            if (toggleAt >= 0 && now == (uint32_t)toggleAt) {
                s126 = s126 ? 0x00 : 0x18;
                toggleAt = -1;
                can(frame(0x126, {s126, 0xB0, 0x00}));
            }
            if (now % 500 == 0) can(frame(0x126, {s126, 0xB0, 0x00}));
            ch.tick(car.state(), c, now);
            gate.tick(c, now);
            take();
        }
    }
};

static void testPressAndRelease() {
    std::printf("--- press acknowledged ---\n");
    Sim s;
    s.can(frame(0x126, {0x00, 0xB0, 0x00}));       // sensors on
    s.cmd(0x05, 1);
    const uint32_t t0 = s.now;
    s.step(3000);
    bool press = !s.port.sent.empty() && is1A9(s.port.sent[0].f, 0x04);
    bool accepted = s.reps.size() == 1 && s.reps[0].result == vset::ChangeResult::ACCEPTED;
    char line[240];
    std::snprintf(line, sizeof(line), "80 05 01: press 00 7F FF 04 FF 00 10 00, report ACCEPTED after %u ms (not delayed by the release)",
                  s.reps.empty() ? 0 : s.reps[0].tookMs);
    check(press && accepted && s.reps[0].tookMs < 100, line);
#if CAN_TX_1A9_RELEASE
    const bool rel = s.port.sent.size() == 2 && is1A9(s.port.sent[1].f, 0x00);
    const uint32_t gap = rel ? s.port.sent[1].ms - s.port.sent[0].ms : 0;
    std::snprintf(line, sizeof(line), "one release 20 7F FF 00 FF 00 50 00, %lu ms after the press (gate pause 500 ms)",
                  (unsigned long)gap);
    check(rel && gap >= vset::SettingsChanger::RELEASE_DELAY_MS && gap >= CanTxGate::COOLDOWN_MS && gap < 600, line);
    check(s.rels.size() == 1 && s.rels[0].sent && s.rels[0].acked && s.relMs[0] > s.repMs[0] && s.s126 == 0x18 &&
              s.gate.stats().fault == TxFault::NONE && s.port.maxOnMs <= CanTxGate::WINDOW_MS,
          "release event: sent, acked, after the ACCEPTED report; state unchanged by the release; no latch");
#else
    check(s.port.sent.size() == 1 && s.rels.empty() && !s.ch.busy(),
          "CAN_TX_1A9_RELEASE=0: no release queued, one frame only");
#endif
    (void)t0;
}

static void testNoAckNoRelease() {
    std::printf("--- press not acknowledged ---\n");
    Sim s;
    s.can(frame(0x126, {0x00, 0xB0, 0x00}));
    s.port.result = 2;
    s.cmd(0x05, 1);
    s.step(3000);
    check(s.port.sent.size() == 1 && s.rels.empty() && s.reps.size() == 1 && s.reps[0].result == vset::ChangeResult::TX_FAILED,
          "press without an ack: TX_FAILED, no release");
}

static void testReleaseGivesUp() {
    std::printf("--- release refused by the gate ---\n");
    {
        Sim s;
        s.can(frame(0x126, {0x00, 0xB0, 0x00}));
        s.cmd(0x05, 1);
        s.step(10);
        s.gate.setArmed(false);                    // "cantx off" right after the press
        s.step(2000);
#if CAN_TX_1A9_RELEASE
        check(s.port.sent.size() == 1 && s.rels.size() == 1 && !s.rels[0].sent && s.rels[0].refuse == TxRefuse::DISARMED &&
                  !s.ch.busy(),
              "disarmed after the press: release not sent (transmit disarmed), at once at its due time");
#else
        check(s.port.sent.size() == 1 && s.rels.empty(), "CAN_TX_1A9_RELEASE=0: nothing more");
#endif
    }
    {
        Sim s;
        s.can(frame(0x126, {0x00, 0xB0, 0x00}));
        s.cmd(0x05, 1);
        s.step(10);
        s.port.h.rec = 3;                          // controller refuses (temporary) for longer than the retry limit
        s.step(3000);
#if CAN_TX_1A9_RELEASE
        const uint32_t after = s.rels.empty() ? 0 : s.rels[0].afterPressMs;
        char line[200];
        std::snprintf(line, sizeof(line), "controller not ready: release retried, given up %lu ms after the press (limit 200 + 1000)",
                      (unsigned long)after);
        check(s.port.sent.size() == 1 && s.rels.size() == 1 && !s.rels[0].sent && s.rels[0].refuse == TxRefuse::CONTROLLER &&
                  after >= 1200 && after <= 1201,
              line);
#else
        check(s.port.sent.size() == 1 && s.rels.empty(), "CAN_TX_1A9_RELEASE=0: nothing more");
#endif
    }
}

static void testCommandWaitsForRelease() {
    std::printf("--- next command while the release is due ---\n");
    Sim s;
    s.can(frame(0x126, {0x00, 0xB0, 0x00}));
    s.can(frame(0x227, {0x00, 0x00, 0x11, 0x00, 0x00}));   // blind spot on
    s.cmd(0x05, 1);
    s.step(100);
    s.cmd(0x0C, 0);                                         // blind spot off, 100 ms after the press
    for (int i = 0; i < 6; i++) {
        s.step(500);
        s.can(frame(0x227, {0x00, 0x00, 0x11, 0x00, 0x00}));
    }
#if CAN_TX_1A9_RELEASE
    check(s.port.sent.size() == 4 && is1A9(s.port.sent[0].f, 0x04) && is1A9(s.port.sent[1].f, 0x00) &&
              is1A9(s.port.sent[2].f, 0x20) && is1A9(s.port.sent[3].f, 0x00) &&
              s.port.sent[2].ms - s.port.sent[1].ms >= CanTxGate::COOLDOWN_MS,
          "order: press 05, release, press 0C, release (the second command waits until the first release is finished)");
    check(s.rels.size() == 2 && s.rels[0].acked && s.rels[1].acked, "two releases, both acked");
#else
    check(s.port.sent.size() == 2 && is1A9(s.port.sent[0].f, 0x04) && is1A9(s.port.sent[1].f, 0x20) && s.rels.empty(),
          "order: press 05, press 0C; no release");
#endif
}

static void testRateRoom() {
    std::printf("--- per-minute limit ---\n");
    Sim s;
    s.can(frame(0x126, {0x00, 0xB0, 0x00}));
    // 9 frames 15B straight through the gate (not through the changer), 600 ms apart.
    for (int i = 0; i < 9; i++) {
        TxRefuse r = s.gate.request(frame(0x15B, {0x00, 0x34, 0x97, 0xBA, 0xD5, 0x40, 0x00, 0x00}), s.c, s.now);
        if (r != TxRefuse::OK) std::printf("      unexpected refusal %s\n", txRefuseName(r));
        TxOutcome o;
        s.step(600);
        s.gate.takeOutcome(o);
    }
    s.cmd(0x05, 1);
    s.step(1500);
#if CAN_TX_1A9_RELEASE
    check(s.port.sent.size() == 9 && s.reps.size() == 1 && s.reps[0].result == vset::ChangeResult::REFUSED &&
              s.reps[0].txRefuse == TxRefuse::RATE && s.rels.empty(),
          "9 frames in the last minute: the press is refused (no room for press + release), nothing sent");
#else
    check(s.port.sent.size() == 10 && is1A9(s.port.sent[9].f, 0x04) && s.reps.size() == 1 &&
              s.reps[0].result == vset::ChangeResult::ACCEPTED,
          "9 frames in the last minute: the press is the 10th frame (no release needed)");
#endif
}

static void testGateContent() {
    std::printf("--- gate allow list ---\n");
    const CanFrame rel = frame(0x1A9, {0x20, 0x7F, 0xFF, 0x00, 0xFF, 0x00, 0x50, 0x00});
    const TxRefuse r = CanTxGate::checkFrame(&kCarPsaAee2010, rel);
    check(r == (CAN_TX_1A9_RELEASE ? TxRefuse::OK : TxRefuse::ID_NOT_ALLOWED),
          CAN_TX_1A9_RELEASE ? "release content on the list" : "release content NOT on the list (byte 4 = 00 refused)");
    // 00 7F FF 00 FF 00 10 00 (seen only with ignition 80 and once at box start-up): refused with either flag.
    const CanFrame old = frame(0x1A9, {0x00, 0x7F, 0xFF, 0x00, 0xFF, 0x00, 0x10, 0x00});
    check(CanTxGate::checkFrame(&kCarPsaAee2010, old) == TxRefuse::ID_NOT_ALLOWED,
          "00 7F FF 00 FF 00 10 00 NOT on the list");
    uint8_t b[8];
    vset::releaseFrame(b);
    check(std::memcmp(b, rel.data, 8) == 0, "vset::releaseFrame = 20 7F FF 00 FF 00 50 00");
}

// Decoder: console lines and the [diag] counters.
static std::vector<std::string> g_lines;
static void logLine(const char* line) {
    if (std::strncmp(line, "[vset]", 6) == 0) g_lines.push_back(line);
}

static void testDecoderLines() {
    std::printf("--- Decoder: console lines ---\n");
    uint32_t now = 10000;
    MockPort port;
    port.now = &now;
    Decoder d;
    DecoderConfig cfg;
    cfg.decoderMode = true;
    d.begin(cfg, logLine);
    d.attachCanTx(&port, true);
    d.setSettingsMask(0xFFFFFFFFUL);
    uint8_t s126 = 0x00;
    size_t seen = 0;
    auto feed = [&]() {
        d.onCanFrame(frame(0x0F6, {0x88, 0x59, 0x1E, 0x23, 0x23, 0x5C, 0x5C, 0x24}), now, true);
        d.onCanFrame(frame(0x126, {s126, 0xB0, 0x00}), now, true);
    };
    auto step = [&](uint32_t ms) {
        for (uint32_t i = 0; i < ms; i++) {
            now++;
            while (seen < port.sent.size()) {
                const CanFrame& f = port.sent[seen++].f;
                if (f.data[3] & 0x04) {
                    s126 = s126 ? 0x00 : 0x18;
                    d.onCanFrame(frame(0x126, {s126, 0xB0, 0x00}), now, true);
                }
            }
            if (now % 500 == 0) feed();
            d.tick(now, false);
        }
    };
    feed();
    step(4000);                                    // past the 3.5 s after ignition on
    g_lines.clear();
    xp::Frame rf;
    rf.cmd = 0x80;
    rf.len = 2;
    rf.data[0] = 0x05;
    rf.data[1] = 0x01;
    d.onRadioFrame(rf, now);
    step(2000);
    for (const std::string& l : g_lines) std::printf("      %s", l.c_str());
    char diag[160];
    d.formatParkDiag(diag, sizeof(diag));
    std::printf("      %s", diag);
#if CAN_TX_1A9_RELEASE
    const bool ok = g_lines.size() == 3 && g_lines[0].find("[vset] 1A9 00 7F FF 04 FF 00 10 00 sent (acked after") == 0 &&
                    g_lines[1] == "[vset] radar switch = 1: ACCEPTED by BSI after 6 ms\n" &&
                    g_lines[2].find("[vset] 1A9 20 7F FF 00 FF 00 50 00 release sent (acked after ") == 0 &&
                    std::strcmp(diag, "[diag] wakes=0 park-flips-asleep=0 1A9 press=1 release=1\n") == 0;
    check(ok, "press line, ACCEPTED, release line; [diag] press=1 release=1");
#else
    const bool ok = g_lines.size() == 2 && g_lines[1] == "[vset] radar switch = 1: ACCEPTED by BSI after 6 ms\n" &&
                    std::strcmp(diag, "[diag] wakes=0 park-flips-asleep=0 1A9 press=1 release=0\n") == 0;
    check(ok, "press line, ACCEPTED, no release line; [diag] press=1 release=0");
#endif
}

// A press the gate ended FAILED before the controller took it (send() returned false) never reached the bus:
// the [diag] press counter must not count it (lib/app/decoder.cpp, TxEvent::handedOver).
static void testDecoderNotHandedOver() {
    std::printf("--- Decoder: press not handed to the controller ---\n");
    uint32_t now = 10000;
    MockPort port;
    port.now = &now;
    Decoder d;
    DecoderConfig cfg;
    cfg.decoderMode = true;
    d.begin(cfg, logLine);
    d.attachCanTx(&port, true);
    d.setSettingsMask(0xFFFFFFFFUL);
    auto step = [&](uint32_t ms) {
        for (uint32_t i = 0; i < ms; i++) {
            now++;
            if (now % 500 == 0) {
                d.onCanFrame(frame(0x0F6, {0x88, 0x59, 0x1E, 0x23, 0x23, 0x5C, 0x5C, 0x24}), now, true);
                d.onCanFrame(frame(0x126, {0x00, 0xB0, 0x00}), now, true);
            }
            d.tick(now, false);
        }
    };
    d.onCanFrame(frame(0x0F6, {0x88, 0x59, 0x1E, 0x23, 0x23, 0x5C, 0x5C, 0x24}), now, true);
    d.onCanFrame(frame(0x126, {0x00, 0xB0, 0x00}), now, true);
    step(4000);                                    // past the 3.5 s after ignition on
    port.sendOk = false;
    g_lines.clear();
    xp::Frame rf;
    rf.cmd = 0x80;
    rf.len = 2;
    rf.data[0] = 0x05;
    rf.data[1] = 0x01;
    d.onRadioFrame(rf, now);
    step(2000);
    for (const std::string& l : g_lines) std::printf("      %s", l.c_str());
    char diag[160];
    d.formatParkDiag(diag, sizeof(diag));
    std::printf("      %s", diag);
    const bool ok = port.sent.empty() && !g_lines.empty() &&
                    g_lines[0].find("[vset] 1A9 00 7F FF 04 FF 00 10 00 NOT ACKED") == 0 &&
                    std::strcmp(diag, "[diag] wakes=0 park-flips-asleep=0 1A9 press=0 release=0\n") == 0;
    check(ok, "send() refused by the controller: NOT ACKED line, [diag] press=0 release=0");
}

int main() {
    std::printf("CAN_TX_EXPERIMENTAL = %d, CAN_TX_1A9_RELEASE = %d\n", CAN_TX_EXPERIMENTAL, CAN_TX_1A9_RELEASE);
    testPressAndRelease();
    testNoAckNoRelease();
    testReleaseGivesUp();
    testCommandWaitsForRelease();
    testRateRoom();
    testGateContent();
    testDecoderLines();
    testDecoderNotHandedOver();
    std::printf("button_release: %d checks, %d errors\n", checks, failures);
    return failures ? 1 : 0;
}
