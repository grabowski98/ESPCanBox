// Host test: the whole path "radio command 80 (or the set command) -> Decoder -> SettingsChanger
// -> CanTxGate gate -> port" in the firmware WITH TRANSMISSION.
//
// TRANSMITS NOTHING: the hardware (src/can_tx/can_tx_port.cpp) is replaced by the MockPort stub, which records
// the frames handed to the "controller" and the time the transmitter is enabled. The car is replaced by a BSI stub: every 500 ms
// it supplies frames 0F6 and 260, and after receiving a 15B frame (if so configured) shows the new state in 260.
// Step = 1 ms = one pass of the main loop: CAN frames -> Decoder::tick, as in src/main.cpp.
//
// Checked after EVERY step: only a 15B frame of length 8 reaches the port, the transmitter is never
// enabled in the disarmed state or after a latch, and never for longer than 20 ms.
//
// Built twice by tools/run_host_tests.sh. With -DCAN_TX_EXPERIMENTAL=1 (expected/tx_path.txt) the last section
// shows that radio climate commands (8A) send nothing; with 0, the default (expected/tx_path_default.txt), it
// shows that the trial frame 1A9 is never handed to the port and 8A sends nothing either.
//
// Run: bash tools/run_host_tests.sh (README.md, section on tests on recordings).
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "can_tx_gate.h"
#include "decoder.h"
#include "vehicle_settings.h"
#include "xp_protocol.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

static std::vector<std::string> g_lines;
static void logLine(const char* line) {
    if (std::strncmp(line, "[vset]", 6) == 0 || std::strncmp(line, "[cantx]", 7) == 0) g_lines.push_back(line);
}

struct MockPort : CanTxPort {
    const uint32_t* now = nullptr;
    bool on = false;
    uint32_t tOn = 0, maxOnMs = 0;
    unsigned onCount = 0;
    std::vector<CanFrame> sent;
    unsigned badFrames = 0;        // frames other than 15B / 8 bytes (must never happen)
    unsigned sendWhileOff = 0;     // send with the transmitter disabled (must never happen)
    uint8_t result = 1;            // result of each send: 1 acknowledged, 2 failed
    bool pending = false;
    uint32_t tSend = 0;
    CanHealth h;

    void transmitter(bool v) override {
        if (v && !on) {
            tOn = *now;
            onCount++;
        }
        if (!v && on && *now - tOn > maxOnMs) maxOnMs = *now - tOn;
        on = v;
    }
    bool send(const CanFrame& f) override {
        if (!on) sendWhileOff++;
        if (f.id != 0x15B || f.len != 8) badFrames++;
        sent.push_back(f);
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

static std::string hex8(const CanFrame& f) {
    char b[40];
    std::snprintf(b, sizeof(b), "%02X %02X %02X %02X %02X %02X %02X %02X", f.data[0], f.data[1], f.data[2], f.data[3],
                  f.data[4], f.data[5], f.data[6], f.data[7]);
    return b;
}

struct Sim {
    uint32_t now = 10000;
    MockPort port;
    Decoder d;
    uint8_t s260[7] = {0xA4, 0x34, 0x97, 0xBA, 0x55, 0x40, 0x00};   // "BSI" state (capture of 11.11.2026)
    uint8_t ign = 0x8E;            // byte 1 of frame 0F6: engine running
    bool bsiAccepts = true;        // the BSI accepts the 15B frame
    uint8_t bsiExtra[5] = {0};     // extra bits the BSI changes on acceptance (side effect)
    size_t seen = 0;               // how many frames from the port the "BSI" has already seen
    unsigned violations = 0;

    Sim(bool attach, bool armed) {
        port.now = &now;
        DecoderConfig cfg;
        cfg.decoderMode = true;
        d.begin(cfg, logLine);
        if (attach) d.attachCanTx(&port, armed);
        feed();
        d.onCanFrame(frame(0x361, {0xA0, 0x66, 0xC8, 0x0A, 0x00, 0x00}), now, true);
        d.tick(now, false);
        g_lines.clear();
    }
    void feed() {
        d.onCanFrame(frame(0x0F6, {ign, 0x59, 0x1E, 0x23, 0x23, 0x5C, 0x5C, 0x24}), now, true);
        CanFrame f;
        f.id = 0x260;
        f.len = 7;
        std::memcpy(f.data, s260, 7);
        d.onCanFrame(f, now, true);
    }
    void inv() {
        TxState s = d.canTxState();
        bool mayBeOn = s == TxState::OPENING || s == TxState::SENDING;
        if (port.on && !mayBeOn) violations++;
        if (port.on && (uint32_t)(now - port.tOn) > CanTxGate::WINDOW_MS) violations++;
    }
    void step(uint32_t ms = 1) {
        for (uint32_t i = 0; i < ms; i++) {
            now++;
            // "BSI": a new 15B frame changes the state shown in the next 260 frame.
            while (seen < port.sent.size()) {
                const CanFrame& f = port.sent[seen++];
                if (!bsiAccepts || port.result != 1) continue;
                for (int k = 0; k < 4; k++) s260[1 + k] = (uint8_t)(f.data[1 + k] ^ bsiExtra[k]);
                s260[5] = (uint8_t)(((f.data[5] & 0xF4) | (s260[5] & 0x0B)) ^ bsiExtra[4]);
            }
            if (now % 500 == 0) feed();
            d.tick(now, false);
            inv();
        }
    }
    void radio80(uint8_t id, uint8_t value) {
        xp::Frame rf;
        rf.cmd = 0x80;
        rf.len = 2;
        rf.data[0] = id;
        rf.data[1] = value;
        d.onRadioFrame(rf, now);
        inv();
    }
    bool clean() const { return violations == 0 && port.badFrames == 0 && port.sendWhileOff == 0; }
};

static void printLines(size_t from = 0) {
    for (size_t i = from; i < g_lines.size(); i++) std::printf("      %s", g_lines[i].c_str());
}

static bool lineIs(size_t i, const char* text) { return i < g_lines.size() && g_lines[i] == text; }

static void testNotAttachedAndDisarmed() {
    std::printf("--- program without hardware and a disarmed program ---\n");
    {
        Sim s(false, false);
        s.radio80(0x01, 0x01);
        s.step(2000);
        printLines();
        check(g_lines.size() == 1 && lineIs(0, "[vset] radio: 80 01 01 parking assist = 1: REFUSED (no CAN transmit in this build)\n") &&
                  s.d.canTxState() == TxState::NOT_BUILT && s.port.onCount == 0 && s.port.sent.empty(),
              "without attachCanTx: like the four old environments (the same line, nobody touches the port)");
    }
    {
        Sim s(true, false);
        s.radio80(0x01, 0x01);
        s.d.requestSetting(0x01, 0x01, s.now);
        s.step(2000);
        printLines();
        char st[160];
        s.d.formatCanTx(st, sizeof(st));
        check(g_lines.size() == 2 && lineIs(0, "[vset] radio: 80 01 01 parking assist = 1: REFUSED (transmit disarmed)\n") &&
                  lineIs(1, g_lines[0].c_str()) && s.d.canTxState() == TxState::DISARMED && s.port.onCount == 0 &&
                  s.port.sent.empty() && std::strncmp(st, "[stat] can-tx=disarmed ", 23) == 0 && s.clean(),
              "hardware present, but not armed: REFUSED (transmit disarmed), transmitter never enabled");
        const SettingCmdStats& sc = s.d.settingCmds();
        check(sc.refused == 2 && sc.lastResult == SettingCmdResult::REFUSED && !sc.lastHaveFrame && s.d.canTxStats().refused == 2,
              "counters: 2 command refusals, 2 gate refusals, no frame");
    }
}

static void testAccepted() {
    std::printf("--- armed: 80 01 01, ack, accepted by BSI ---\n");
    Sim s(true, true);
    check(s.d.canTxState() == TxState::IDLE, "after attachCanTx(port, true): gate ready");
    s.step(120);                                   // command at 10120 ms, the next 260 at 10500 ms
    s.radio80(0x01, 0x01);
    s.step(600);
    printLines();
    bool frameOk = s.port.sent.size() == 1 && hex8(s.port.sent[0]) == "00 34 97 BA D5 40 00 00";
    check(frameOk && s.port.sent[0].id == 0x15B && s.port.sent[0].len == 8,
          "one 15B frame = 00 34 97 BA D5 40 00 00 went to the port (like the box, measured 11.11.2026)");
    check(g_lines.size() == 2 && lineIs(0, "[vset] 15B 00 34 97 BA D5 40 00 00 sent (acked after 5 ms)\n") &&
              lineIs(1, "[vset] parking assist = 1: ACCEPTED by BSI after 380 ms\n"),
          "lines: 15B sent (acked 5 ms after the request, counted in loop passes), ACCEPTED by BSI after 380 ms");
    const SettingCmdStats& sc = s.d.settingCmds();
    check(sc.accepted == 1 && sc.lastResult == SettingCmdResult::ACCEPTED && sc.lastHaveFrame && sc.lastTookMs == 380 &&
              sc.lastFrame[4] == 0xD5 && s.d.canTxStats().sent == 1 && s.d.canTxStats().acked == 1,
          "data for the panel: accepted, tookMs = 380, frame remembered; gate: sent 1, acked 1");
    check(s.port.maxOnMs == 4 && s.port.onCount == 1, "transmitter on once, 4 ms in total");
    size_t n = g_lines.size();
    s.radio80(0x01, 0x01);
    s.step(600);
    printLines(n);
    check(g_lines.size() == n + 1 && lineIs(n, "[vset] parking assist = 1: unchanged (the car already has this state), nothing sent\n") &&
              s.port.sent.size() == 1,
          "the same command a second time: unchanged, nothing sent");
    n = g_lines.size();
    s.d.requestSetting(0x01, 0x00, s.now);         // command "set 01 00": the same path
    s.step(1000);
    printLines(n);
    check(s.port.sent.size() == 2 && hex8(s.port.sent[1]) == "00 34 97 BA 55 40 00 00" && g_lines.size() == n + 2 &&
              std::strncmp(g_lines[n + 1].c_str(), "[vset] parking assist = 0: ACCEPTED by BSI after ", 49) == 0,
          "command set 01 00: frame 00 34 97 BA 55 40 00 00, ACCEPTED");
    char diff[160];
    s.d.formatSettingsDiff(diff, sizeof(diff));
    std::printf("      %s", diff);
    check(std::strcmp(diff, "[vset] 260 at startup=A4 34 97 BA 55 40 00 changed bits (bytes 2-6)=00 00 00 00 00\n") == 0,
          "vset: difference of 260 against the startup = zero after enabling and disabling");
    check(s.clean(), "invariants: 15B / 8 bytes only, transmitter at most 20 ms and only in the window");
}

static void testNotAcceptedAndFailed() {
    std::printf("--- BSI does not accept; no ack; latch ---\n");
    {
        Sim s(true, true);
        s.bsiAccepts = false;
        s.step(120);
        s.radio80(0x01, 0x01);
        s.step(2000);
        printLines();
        check(g_lines.size() == 2 && lineIs(1, "[vset] parking assist = 1: NOT ACCEPTED by BSI (260 unchanged after 1305 ms)\n") &&
                  s.port.sent.size() == 1 && s.d.canTxState() != TxState::FAULT && s.d.settingCmds().notAccepted == 1,
              "260 unchanged: NOT ACCEPTED after 1305 ms, one frame (no repeats), no latch");
        check(s.clean(), "invariants");
    }
    {
        Sim s(true, true);
        s.port.result = 2;                         // nobody acknowledges (e.g. bench without a second node)
        for (int i = 0; i < 3; i++) {
            s.radio80(0x01, 0x01);
            s.step(1000);
        }
        printLines();
        bool faultLine = false;
        for (const std::string& l : g_lines)
            if (l == "[cantx] FAULT: 3 failed sends, transmit locked until \"cantx reset\"\n") faultLine = true;
        check(s.port.sent.size() == 3 && faultLine && s.d.canTxState() == TxState::FAULT &&
                  s.d.canTxStats().fault == TxFault::THREE_FAILED && !s.port.on,
              "3 frames without an ack: line [cantx] FAULT, latch, transmitter off");
        size_t n = g_lines.size();
        s.radio80(0x01, 0x01);
        s.step(1000);
        printLines(n);
        check(g_lines.size() == n + 1 && s.port.sent.size() == 3 &&
                  lineIs(n, "[vset] radio: 80 01 01 parking assist = 1: REFUSED (transmit locked after a fault)\n"),
              "after the latch: REFUSED, nothing sent");
        s.d.canTxClearFault();
        s.port.result = 1;
        n = g_lines.size();
        s.radio80(0x01, 0x01);
        s.step(1000);
        check(s.port.sent.size() == 4 && s.d.canTxState() != TxState::FAULT && g_lines.size() == n + 2,
              "after cantx reset: the next command sends a frame");
        check(s.clean(), "invariants");
    }
}

static void testSideEffect() {
    std::printf("--- foreign bits of 260 changed together with the requested one ---\n");
    Sim s(true, true);
    s.bsiExtra[2] = 0x10;                          // the BSI additionally changes bit 4.4
    s.step(120);
    s.radio80(0x01, 0x01);
    s.step(1000);
    printLines();
    bool side = false, fault = false;
    for (const std::string& l : g_lines) {
        if (l == "[vset] parking assist = 1: ACCEPTED, but other bits of 260 changed too (00 00 10 00 00): transmit locked\n")
            side = true;
        if (l == "[cantx] FAULT: foreign bits of 260 changed, transmit locked until \"cantx reset\"\n") fault = true;
    }
    check(side && fault && s.d.canTxState() == TxState::FAULT && s.d.settingCmds().lastResult == SettingCmdResult::SIDE_EFFECT,
          "ACCEPTED, but other bits of 260 changed too (00 00 10 00 00): latch");
    check(s.clean(), "invariants");
}

static void testRefusals() {
    std::printf("--- mask, groups D and E, gate conditions ---\n");
    Sim s(true, true);
    s.radio80(0x02, 0x01);                         // rear wiper on reversing: disabled in the default mask
    s.radio80(0x0A, 0x01);                         // fuel units: group D
    s.radio80(0x0D, 0x01);                         // Start/Stop: group E (no known frame)
    s.radio80(0x05, 0x01);                         // radar: frame 1A9
    s.radio80(0x0F, 0x01);                         // door opening: disabled in the mask
    s.step(1500);
    printLines();
    check(g_lines.size() == 5 && s.port.sent.empty() && s.port.onCount == 0 &&
              lineIs(2, "[vset] radio: 80 0D 01 Start/Stop = 1: no known frame\n") &&
              lineIs(3, "[vset] radio: 80 05 01 radar switch = 1: REFUSED (disabled in mask)\n"),
          "02 and 0F (mask), 0A (group D), 0D (no known frame), 05 (1A9 button disabled in mask): five refusals, transmitter never enabled");
    // Enabling item 02 in the mask (command "vset on 02").
    uint32_t m = 0;
    vset::maskChange(s.d.settingsMask(), 0x02, true, m);
    s.d.setSettingsMask(m);
    size_t n = g_lines.size();
    s.radio80(0x02, 0x01);
    s.step(1000);
    printLines(n);
    check(s.port.sent.size() == 1 && hex8(s.port.sent[0]) == "00 34 97 BA 75 40 00 00",
          "after vset on 02: 80 02 01 gives frame 00 34 97 BA 75 40 00 00 (one field: bit 5.5)");
    // Gate conditions.
    s.d.setTxBlock(true, false);                   // firmware update session
    n = g_lines.size();
    s.radio80(0x01, 0x01);
    s.step(100);
    bool ota = lineIs(n, "[vset] radio: 80 01 01 parking assist = 1: REFUSED (flash write in progress)\n");
    s.d.setTxBlock(false, true);                   // a settings save is pending
    s.radio80(0x01, 0x01);
    s.step(100);
    bool nvs = lineIs(n + 1, "[vset] radio: 80 01 01 parking assist = 1: REFUSED (flash write in progress)\n");
    s.d.setTxBlock(false, false);
    s.ign = 0x80;                                  // ignition off
    s.step(600);
    s.radio80(0x01, 0x01);
    s.step(100);
    bool ignOff = lineIs(n + 2, "[vset] radio: 80 01 01 parking assist = 1: REFUSED (no ignition)\n");
    printLines(n);
    check(ota && nvs && ignOff && s.port.sent.size() == 1,
          "firmware update, pending settings write, ignition off: three refusals, nothing sent");
    s.ign = 0x88;                                  // ignition without engine: allowed (not verified in the car)
    s.step(600);
    s.radio80(0x01, 0x01);
    s.step(1000);
    check(s.port.sent.size() == 2, "ignition 88 (engine off): frame sent");
    check(s.clean(), "invariants");
}

static void testDisarmAndClose() {
    std::printf("--- disarming and forceClose meanwhile ---\n");
    {
        Sim s(true, true);
        s.radio80(0x01, 0x01);                     // gate in OPENING, transmitter enabled
        bool wasOn = s.port.on;
        s.d.setCanTxArmed(false);                  // "cantx off"
        bool offNow = !s.port.on && s.d.canTxState() == TxState::DISARMED;
        s.step(1000);
        size_t sent = s.port.sent.size();
        s.radio80(0x01, 0x01);
        s.step(1000);
        check(wasOn && offNow && sent == 0 && s.port.sent.empty(), "cantx off in the window: transmitter off at once, nothing sent, then refusals");
        check(s.clean(), "invariants");
    }
    {
        Sim s(true, true);
        s.radio80(0x01, 0x01);
        s.step(2);                                 // frame handed over, waiting for the result
        bool wasOn = s.port.on;
        s.d.canTxForceClose();                     // before sleep / restart
        check(wasOn && !s.port.on, "forceClose during a send: transmitter off");
        s.step(3000);
        check(s.clean() && s.port.sent.size() == 1, "after forceClose no further frames, invariants");
    }
}

static void testRate() {
    std::printf("--- frame limit and a long run ---\n");
    Sim s(true, true);
    s.bsiAccepts = true;
    unsigned sentBefore = 0;
    for (int i = 0; i < 40; i++) {                 // 40 commands alternating 1 / 0 every 1.5 s = 60 s
        s.radio80(0x01, (uint8_t)((i + 1) & 1));
        s.step(1500);
        if (i == 9) sentBefore = (unsigned)s.port.sent.size();
    }
    char line[200];
    std::snprintf(line, sizeof(line), "40 commands in 60 s: after 10 commands %u frames, in total %u frames (limit 10 per minute), longest on %lu ms",
                  sentBefore, (unsigned)s.port.sent.size(), (unsigned long)s.port.maxOnMs);
    check(sentBefore == 10 && s.port.sent.size() <= 11 && s.port.maxOnMs <= CanTxGate::WINDOW_MS, line);
    bool only15B = true;
    for (const CanFrame& f : s.port.sent)
        if (f.id != 0x15B || f.len != 8 || f.data[0] != 0 || f.data[6] != 0 || f.data[7] != 0) only15B = false;
    check(only15B && s.clean(), "every frame: ID 15B, 8 bytes, bytes 1, 7, 8 = 00; invariants");
}

#if CAN_TX_EXPERIMENTAL
// Radio climate commands (8A) never reach the CAN bus, also in the build with the trial frame 1A9: the program has
// no climate transmit path (the 364 trial was removed on 19.11.2026, single 364 frames did not change the climate).
static void testClimateNoTx() {
    std::printf("--- climate: radio 8A sends nothing on CAN ---\n");
    Sim s(true, true);
    s.bsiAccepts = false;
    for (uint8_t n = 1; n <= 0x0C; n++) {
        xp::Frame rf;
        rf.cmd = 0x8A;
        rf.len = 2;
        rf.data[0] = n;
        rf.data[1] = 1;
        s.d.onRadioFrame(rf, s.now);
        s.step(300);
    }
    s.step(3000);
    check(s.port.sent.empty() && s.port.onCount == 0, "8A 01-0C: nothing sent, transmitter never enabled");
    s.radio80(0x01, 0x01);
    s.step(1000);
    check(s.port.sent.size() == 1 && s.port.sent[0].id == 0x15B && s.clean(), "80 01 01 still sends 15B; invariants");
}

#else
// Default build: radio climate commands send nothing and a 1A9 button command ends with a gate refusal.
static void testExperimentalOff() {
    std::printf("--- default build (CAN_TX_EXPERIMENTAL = 0): no 1A9, no climate frame ---\n");
    Sim s(true, true);
    s.bsiAccepts = false;
    xp::Frame rf;
    rf.cmd = 0x8A;
    rf.len = 2;
    rf.data[0] = 0x04;
    rf.data[1] = 1;
    s.d.onRadioFrame(rf, s.now);
    s.step(3000);
    check(s.port.sent.empty() && s.port.onCount == 0, "radio 8A 04 01 sends nothing, transmitter never enabled");
    uint32_t m = 0;
    vset::maskChange(s.d.settingsMask(), 0x05, true, m);
    s.d.setSettingsMask(m);
    s.step(600);                                   // more than 3.5 s after ignition on: the button state is known
    s.d.onCanFrame(frame(0x227, {0x00, 0x00, 0x00, 0x00, 0x00}), s.now, true);   // radar button LED off
    s.d.onCanFrame(frame(0x126, {0x00, 0xB0, 0x00}), s.now, true);               // parking sensors on
    s.radio80(0x05, 0x01);
    s.step(1000);
    printLines();
    check(g_lines.size() == 1 && lineIs(0, "[vset] radio: 80 05 01 radar switch = 1: REFUSED (frame not on the list)\n") &&
              s.port.sent.empty() && s.port.onCount == 0,
          "vset on 05, 80 05 01: frame 1A9 refused by the gate (not on the list), nothing sent");
    size_t n = g_lines.size();
    s.radio80(0x01, 0x01);
    s.step(1000);
    printLines(n);
    check(s.port.sent.size() == 1 && s.port.sent[0].id == 0x15B && s.clean(), "80 01 01 still sends 15B; invariants");
}
#endif

int main() {
    testNotAttachedAndDisarmed();
    testAccepted();
    testNotAcceptedAndFailed();
    testSideEffect();
    testRefusals();
    testDisarmAndClose();
    testRate();
#if CAN_TX_EXPERIMENTAL
    testClimateNoTx();
#else
    testExperimentalOff();
#endif
    std::printf("tx_path: %d checks, %d errors\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
