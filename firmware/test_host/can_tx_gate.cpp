// Host test: CAN transmit gate (lib/core/can_tx_gate.h).
//
// TRANSMITS NOTHING: the hardware (the transceiver SLNT pin and the CAN controller) is replaced by the MockPort stub.
// Time is simulated, step = 1 ms = one pass of the main loop.
//
// Invariant checked after EVERY step and after every gate call: the transmitter is enabled only
// in states OPENING and SENDING and never for longer than 20 ms.
//
// The gate takes the transmit allow list of the PSA AEE2010 profile.
// Built twice by tools/run_host_tests.sh: with -DCAN_TX_EXPERIMENTAL=1 (the output must equal the reference
// recorded before the profile allow lists, expected/can_tx_gate.txt) and with 0, the default (expected/can_tx_gate_default.txt:
// the same output plus a section showing that valid 1A9 / 364 frames are refused).
//
// Run: bash tools/run_host_tests.sh (README.md, section on tests on recordings).
#include <cstdio>
#include <cstring>
#include <vector>

#include "can_tx_gate.h"
#include "profile_generic.h"
#include "profile_psa_aee2010.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

// Hardware stub: remembers when the transmitter was enabled and plays back the send result from the plan.
struct MockPort : CanTxPort {
    const uint32_t* now = nullptr;
    bool on = false;
    uint32_t tOn = 0;
    uint32_t maxOnMs = 0;          // longest time with the transmitter enabled
    unsigned onCount = 0;
    std::vector<CanFrame> sent;    // frames handed to the "controller"
    bool sendOk = true;
    // Result of successive sends: plan[i] for the i-th send since the last reset(), then 'rest'.
    std::vector<uint8_t> plan;
    uint8_t rest = 1;              // acknowledged by default
    uint32_t delayMs = 2;          // poll() returns the result this many ms after send()
    size_t planPos = 0;
    bool pending = false;
    uint32_t tSend = 0;
    uint8_t pendingResult = 0;
    CanHealth h;
    unsigned sendWhileOff = 0;     // send() with the transmitter disabled (must never happen)

    void transmitter(bool v) override {
        if (v && !on) {
            tOn = *now;
            onCount++;
        }
        if (!v && on) {
            uint32_t d = *now - tOn;
            if (d > maxOnMs) maxOnMs = d;
        }
        on = v;
    }
    bool send(const CanFrame& f) override {
        if (!on) sendWhileOff++;
        if (!sendOk) return false;
        sent.push_back(f);
        pendingResult = planPos < plan.size() ? plan[planPos] : rest;
        planPos++;
        pending = true;
        tSend = *now;
        return true;
    }
    uint8_t poll() override {
        if (!pending) return 0;
        if (pendingResult == 0) return 0;                       // never completes
        if ((uint32_t)(*now - tSend) < delayMs) return 0;
        pending = false;
        return pendingResult;
    }
    CanHealth health() override { return h; }
    void script(std::vector<uint8_t> p, uint8_t r = 1) {
        plan = p;
        rest = r;
        planPos = 0;
    }
};

struct Sim {
    uint32_t now = 1000;
    MockPort port;
    CanTxGate gate;
    TxConditions c;
    unsigned violations = 0;       // invariant violations
    bool checkInvariant = true;

    explicit Sim(uint32_t start = 1000, bool withPort = true, bool armed = true) {
        now = start;
        port.now = &now;
        c.ignitionOk = true;
        c.busAlive = true;
        gate.begin(withPort ? &port : nullptr, &kCarPsaAee2010);
        if (armed) gate.setArmed(true);
    }
    void inv() {
        if (!checkInvariant) return;
        TxState s = gate.state();
        bool mayBeOn = s == TxState::OPENING || s == TxState::SENDING;
        if (port.on && !mayBeOn) violations++;
        if (port.on && (uint32_t)(now - port.tOn) > CanTxGate::WINDOW_MS) violations++;
        if (port.on != gate.transmitterOn()) violations++;
    }
    void step(uint32_t ms = 1) {
        for (uint32_t i = 0; i < ms; i++) {
            now++;
            gate.tick(c, now);
            inv();
        }
    }
    TxRefuse req(uint32_t id = 0x15B, uint8_t len = 8) {
        CanFrame f;
        f.id = id;
        f.len = len;
        for (int i = 0; i < 8; i++) f.data[i] = (uint8_t)(0x10 + i);
        TxRefuse r = gate.request(f, c, now);
        inv();
        return r;
    }
    // Request, then steps until a result (at most 100 ms). Returns the result or NONE.
    TxOutcome sendOne() {
        if (req() != TxRefuse::OK) return TxOutcome::NONE;
        TxOutcome o = TxOutcome::NONE;
        for (int i = 0; i < 100; i++) {
            step();
            if (gate.takeOutcome(o)) return o;
        }
        return TxOutcome::NONE;
    }
    bool clean() const { return violations == 0 && port.sendWhileOff == 0; }
};

static void testNotBuiltDisarmed() {
    std::printf("--- no hardware and the disarmed state ---\n");
    {
        Sim s(1000, false, true);
        TxRefuse r = s.req();
        s.step(50);
        s.gate.forceClose();
        s.gate.latchFault(TxFault::OUTSIDE);
        s.gate.clearFault();
        check(r == TxRefuse::NOT_BUILT && s.gate.state() == TxState::NOT_BUILT && s.port.onCount == 0 &&
                  s.gate.stats().refused == 1,
              "port = nullptr: NOT_BUILT forever, request refused, nothing called");
    }
    {
        Sim s(1000, true, false);
        TxRefuse r = s.req();
        s.step(50);
        check(r == TxRefuse::DISARMED && s.gate.state() == TxState::DISARMED && s.port.onCount == 0 && s.port.sent.empty(),
              "disarmed: request refused, transmitter never enabled");
        s.gate.setArmed(true);
        bool idle = s.gate.state() == TxState::IDLE;
        s.gate.setArmed(false);
        check(idle && s.gate.state() == TxState::DISARMED && s.req() == TxRefuse::DISARMED,
              "arming -> IDLE, disarming -> DISARMED");
    }
}

static void testFrameList() {
    std::printf("--- list of allowed frames ---\n");
    Sim s;
    bool ok = true;
    const uint32_t ids[] = {0x15A, 0x15C, 0x1A9, 0x364, 0x260, 0x1E5, 0x000, 0x7FF, 0x95B, 0x1000015B};
    for (uint32_t id : ids)
        if (s.req(id) != TxRefuse::ID_NOT_ALLOWED) ok = false;
    check(ok && s.gate.state() == TxState::IDLE && s.port.onCount == 0,
          "IDs not on the list (15A, 15C, 1A9, 364, 260, 1E5, 000, 7FF, 95B, 1000015B): refused, transmitter off");
    bool lenOk = true;
    for (uint8_t len = 0; len <= 9; len++)
        if (len != 8 && s.req(0x15B, len) != TxRefuse::BAD_LEN) lenOk = false;
    check(lenOk && s.gate.state() == TxState::IDLE && s.port.onCount == 0, "15B with length 0-7 and 9: refused");
    check(s.clean(), "transmitter invariant");
}

#if !CAN_TX_EXPERIMENTAL
// Default build: the trial frames 1A9 and 364 are not in the allow list, even with a content that the
// experimental validators accept; the generic profile refuses everything (empty list).
static void testExperimentalOff() {
    std::printf("--- default build (CAN_TX_EXPERIMENTAL = 0): trial frames 1A9 and 364 not on the list ---\n");
    Sim s;
    CanFrame b;
    b.id = 0x1A9;
    b.len = 8;
    const uint8_t radar[8] = {0x00, 0x7F, 0xFF, 0x04, 0xFF, 0x00, 0x10, 0x00};
    std::memcpy(b.data, radar, 8);
    CanFrame c4;
    c4.id = 0x364;
    c4.len = 4;
    const uint8_t idle[4] = {0x80, 0x00, 0x1E, 0x00};
    std::memcpy(c4.data, idle, 4);
    CanFrame c6;
    c6.id = 0x364;
    c6.len = 6;
    const uint8_t full[6] = {0x00, 0x00, 0x1E, 0x78, 0xE0, 0x00};
    std::memcpy(c6.data, full, 6);
    TxRefuse rb = s.gate.request(b, s.c, s.now);
    TxRefuse r4 = s.gate.request(c4, s.c, s.now);
    TxRefuse r6 = s.gate.request(c6, s.c, s.now);
    s.inv();
    check(rb == TxRefuse::ID_NOT_ALLOWED && r4 == TxRefuse::ID_NOT_ALLOWED && r6 == TxRefuse::ID_NOT_ALLOWED &&
              s.gate.state() == TxState::IDLE && s.port.onCount == 0,
          "1A9 00 7F FF 04 FF 00 10 00, 364 80 00 1E 00, 364 00 00 1E 78 E0 00: refused (not on the list), transmitter off");
    check(!CanTxGate::allowedFrame(&kCarPsaAee2010, b) && !CanTxGate::allowedFrame(&kCarPsaAee2010, c4) &&
              !CanTxGate::allowedFrame(&kCarPsaAee2010, c6),
          "the same three frames: refused by the second check of the hardware port (allowedFrame)");
    check(kCarPsaAee2010.txAllowCount == 1 && kCarPsaAee2010.txAllow[0].canId == 0x15B && s.req() == TxRefuse::OK,
          "PSA AEE2010 allow list: one row, 15B; a 15B frame is accepted");
    MockPort gp;
    uint32_t gnow = 1000;
    gp.now = &gnow;
    CanTxGate g;
    g.begin(&gp, &kCarGeneric);
    g.setArmed(true);
    TxConditions gc;
    gc.ignitionOk = true;
    gc.busAlive = true;
    CanFrame f15;
    f15.id = 0x15B;
    f15.len = 8;
    TxRefuse rg = g.request(f15, gc, gnow);
    CanTxGate n;
    n.begin(&gp, nullptr);
    n.setArmed(true);
    TxRefuse rn = n.request(f15, gc, gnow);
    check(rg == TxRefuse::ID_NOT_ALLOWED && rn == TxRefuse::ID_NOT_ALLOWED && gp.onCount == 0 &&
              !CanTxGate::allowedFrame(&kCarGeneric, f15) && !CanTxGate::allowedFrame(nullptr, f15),
          "profile generic (empty list) and no profile: 15B refused, transmitter never enabled");
    check(s.clean(), "transmitter invariant");
}
#endif

static void testNormalSend() {
    std::printf("--- normal send ---\n");
    Sim s;
    TxRefuse r = s.req();
    bool opening = r == TxRefuse::OK && s.gate.state() == TxState::OPENING && s.port.on && s.port.sent.empty();
    check(opening, "request accepted: OPENING, transmitter on, frame not sent yet");
    s.step();
    bool still = s.gate.state() == TxState::OPENING && s.port.sent.empty();   // 2 ticks to leave silent mode
    s.step();
    check(still && s.gate.state() == TxState::SENDING && s.port.sent.size() == 1 && s.port.sent[0].id == 0x15B &&
              s.port.sent[0].len == 8 && s.port.sent[0].data[0] == 0x10 && s.port.sent[0].data[7] == 0x17,
          "after 1 ms still OPENING, after 2 ms: SENDING, the same 15B frame went to the port");
    check(s.req() == TxRefuse::BUSY, "second request meanwhile: BUSY");
    s.step(2);
    TxOutcome o = TxOutcome::NONE;
    bool got = s.gate.takeOutcome(o);
    check(got && o == TxOutcome::ACKED && s.gate.state() == TxState::COOLDOWN && !s.port.on && s.port.maxOnMs == 4,
          "ack after 2 ms: result ACKED, COOLDOWN, transmitter on for 4 ms in total");
    check(!s.gate.takeOutcome(o), "result handed out once");
    s.step(499);
    bool busy = s.req() == TxRefuse::BUSY && s.gate.state() == TxState::COOLDOWN;
    s.step(1);
    check(busy && s.gate.state() == TxState::IDLE, "cooldown: after 499 ms still BUSY, after 500 ms IDLE");
    check(s.req() == TxRefuse::OK, "after the cooldown the next request is accepted");
    const TxStats& t = s.gate.stats();
    check(t.sent == 1 && t.acked == 1 && t.failed == 0 && t.refused == 2, "counters: sent 1, acked 1, refused 2");
    check(s.clean(), "transmitter invariant");
}

static void testRate(uint32_t start, const char* what) {
    Sim s(start);
    unsigned ok = 0;
    for (int i = 0; i < 10; i++) {
        if (s.sendOne() == TxOutcome::ACKED) ok++;
        s.step(600);                                       // after a 500 ms pause
    }
    uint32_t tFirst = start;
    TxRefuse r11 = s.req();
    // Up to 60 s from the first frame still refused, then one free slot.
    while ((uint32_t)(s.now - tFirst) < 59990) s.step(10);
    TxRefuse rBefore = s.req();
    s.step(20);
    TxRefuse rAfter = s.req();
    char line[200];
    std::snprintf(line, sizeof(line), "%s: 10 frames accepted, 11th RATE, before 60 s RATE, 60 s after the first OK", what);
    check(ok == 10 && r11 == TxRefuse::RATE && rBefore == TxRefuse::RATE && rAfter == TxRefuse::OK && s.clean(), line);
}

static void testConditions() {
    std::printf("--- admission conditions, each on its own ---\n");
    {
        Sim s;
        s.c.ignitionOk = false;
        check(s.req() == TxRefuse::IGNITION && s.port.onCount == 0, "no fresh 0F6 with ignition: IGNITION");
        s.c.ignitionOk = true;
        s.c.busAlive = false;
        check(s.req() == TxRefuse::BUS_QUIET && s.port.onCount == 0, "bus is quiet: BUS_QUIET");
        s.c.busAlive = true;
        s.c.flashBusy = true;
        check(s.req() == TxRefuse::FLASH_BUSY && s.port.onCount == 0, "flash write or update: FLASH_BUSY");
        s.c.flashBusy = false;
        s.c.nvsPending = true;
        check(s.req() == TxRefuse::FLASH_BUSY && s.port.onCount == 0, "settings write pending: FLASH_BUSY");
        s.c.nvsPending = false;
        check(s.gate.state() == TxState::IDLE && s.req() == TxRefuse::OK, "after the conditions clear: accepted (refusals do not latch)");
        check(s.clean(), "transmitter invariant");
    }
    {
        Sim s;
        s.port.h.rec = 5;
        TxRefuse r1 = s.req();
        bool idle1 = s.gate.state() == TxState::IDLE;
        s.port.h.rec = 0;
        TxRefuse r2 = s.req();
        check(r1 == TxRefuse::CONTROLLER && idle1 && r2 == TxRefuse::OK && s.gate.stats().fault == TxFault::NONE,
              "REC = 5: temporary refusal CONTROLLER without a latch; REC = 0: accepted");
    }
    {
        Sim s;
        s.port.h.rec = 130;
        TxRefuse r1 = s.req();
        s.port.h.rec = 0;
        s.port.h.state = 0;
        TxRefuse r2 = s.req();
        s.port.h.state = 3;
        TxRefuse r3 = s.req();
        check(r1 == TxRefuse::CONTROLLER && r2 == TxRefuse::CONTROLLER && r3 == TxRefuse::CONTROLLER &&
                  s.gate.state() == TxState::IDLE,
              "REC = 130, controller stopped, recovering from bus-off: CONTROLLER without a latch");
    }
    {
        Sim s;
        s.port.h.busErrors = 1;
        s.port.h.msLastBusError = s.now - 50;
        TxRefuse r1 = s.req();
        s.step(49);
        TxRefuse r2 = s.req();
        s.step(1);
        TxRefuse r3 = s.req();
        check(r1 == TxRefuse::CONTROLLER && r2 == TxRefuse::CONTROLLER && r3 == TxRefuse::OK,
              "bus error 50 ms ago: CONTROLLER; 100 ms after the error: accepted");
    }
    {
        Sim s;
        s.port.h.tec = 32;
        TxRefuse r = s.req();
        check(r == TxRefuse::FAULT && s.gate.state() == TxState::FAULT && s.gate.stats().fault == TxFault::TEC && !s.port.on,
              "TEC = 32: latch FAULT");
        Sim b;
        b.port.h.state = 2;
        TxRefuse rb = b.req();
        check(rb == TxRefuse::FAULT && b.gate.state() == TxState::FAULT && b.gate.stats().fault == TxFault::BUS_OFF,
              "bus-off: latch FAULT");
    }
}

static void testArbitration() {
    std::printf("--- arbitration ---\n");
    {
        Sim s;
        s.port.script({3, 3, 1});
        TxOutcome o = s.sendOne();
        check(o == TxOutcome::ACKED && s.port.sent.size() == 3 && s.gate.state() == TxState::COOLDOWN && s.port.onCount == 1 &&
                  s.port.maxOnMs <= 20,
              "arbitration lost 2 times, then ACK: 3 tries in one window, result ACKED");
        check(s.clean(), "transmitter invariant");
    }
    {
        Sim s;
        s.port.script({}, 3);                              // every attempt fails
        unsigned failed = 0;
        for (int i = 0; i < 4; i++) {
            if (s.sendOne() == TxOutcome::FAILED) failed++;
            s.step(600);
        }
        check(failed == 4 && s.port.sent.size() == 12 && s.gate.state() == TxState::IDLE &&
                  s.gate.stats().fault == TxFault::NONE,
              "arbitration lost 3 times: FAILED after 3 tries; 4 such sends in a row do not latch");
        check(s.clean(), "transmitter invariant");
    }
}

static void testFaults() {
    std::printf("--- FAULT latch ---\n");
    {
        Sim s;
        s.port.script({2, 2, 1, 2, 2, 2});
        TxOutcome a = s.sendOne();
        s.step(600);
        TxOutcome b = s.sendOne();
        s.step(600);
        bool noLatch2 = s.gate.state() == TxState::IDLE;
        TxOutcome ok = s.sendOne();                        // a successful one resets the counter
        s.step(600);
        TxOutcome f1 = s.sendOne();
        s.step(600);
        TxOutcome f2 = s.sendOne();
        s.step(600);
        bool noLatch4 = s.gate.state() == TxState::IDLE;
        TxOutcome f3 = s.sendOne();
        check(a == TxOutcome::FAILED && b == TxOutcome::FAILED && noLatch2 && ok == TxOutcome::ACKED,
              "2 failed, then a good one: no latch");
        check(f1 == TxOutcome::FAILED && f2 == TxOutcome::FAILED && noLatch4 && f3 == TxOutcome::FAILED &&
                  s.gate.state() == TxState::FAULT && s.gate.stats().fault == TxFault::THREE_FAILED && !s.port.on,
              "3 failed in a row: FAULT");
        s.step(5000);
        TxRefuse r = s.req();
        check(r == TxRefuse::FAULT && s.gate.state() == TxState::FAULT && s.port.onCount == 6,
              "FAULT holds (5 s later still refused, transmitter not enabled)");
        s.gate.clearFault();
        check(s.gate.state() == TxState::IDLE && s.gate.stats().fault == TxFault::NONE && s.sendOne() == TxOutcome::ACKED,
              "clearFault: IDLE, the next send works");
        check(s.clean(), "transmitter invariant");
    }
    {
        Sim s;
        s.port.script({4});
        TxOutcome o = s.sendOne();
        check(o == TxOutcome::FAILED && s.gate.state() == TxState::FAULT && s.gate.stats().fault == TxFault::CONTROLLER_EVENT &&
                  !s.port.on,
              "poll() = 4 (controller event): FAULT at once");
    }
    {
        Sim s;
        s.port.script({0});                                // the frame never completes
        TxOutcome o = s.sendOne();
        check(o == TxOutcome::TIMEOUT && s.gate.state() == TxState::FAULT && s.gate.stats().fault == TxFault::WINDOW &&
                  !s.port.on && s.port.maxOnMs == 20,
              "no result: after 20 ms transmitter off, result TIMEOUT, FAULT");
        check(s.clean(), "transmitter invariant");
    }
    {
        Sim s;
        s.port.sendOk = false;
        TxOutcome o = s.sendOne();
        check(o == TxOutcome::FAILED && s.gate.state() == TxState::COOLDOWN && !s.port.on && s.gate.stats().sent == 0,
              "port did not take the frame: FAILED, transmitter off");
    }
    {
        Sim s;
        s.gate.latchFault(TxFault::SIDE_EFFECT);
        bool idleLatch = s.gate.state() == TxState::FAULT && s.req() == TxRefuse::FAULT;
        Sim t;
        t.req();
        t.step();
        t.gate.latchFault(TxFault::SIDE_EFFECT);
        t.inv();
        TxOutcome o = TxOutcome::NONE;
        bool got = t.gate.takeOutcome(o);
        check(idleLatch && t.gate.state() == TxState::FAULT && !t.port.on && got && o == TxOutcome::FAILED && t.clean(),
              "latchFault from outside: in IDLE and during a send (transmitter off, result FAILED)");
        Sim d(1000, true, false);
        d.gate.latchFault(TxFault::OUTSIDE);
        bool stays = d.gate.state() == TxState::DISARMED;
        d.gate.setArmed(true);
        check(stays && d.gate.state() == TxState::FAULT, "latch in the disarmed state: FAULT after arming, not IDLE");
    }
    {
        // The main loop stalls for 30 ms with the transmitter enabled (in the device the 25 ms hardware
        // timer closes it then). On return the gate latches, even though the frame was acknowledged.
        Sim s;
        s.req();
        s.step(2);
        s.checkInvariant = false;
        s.now += 30;
        s.gate.tick(s.c, s.now);
        TxOutcome o = TxOutcome::NONE;
        bool got = s.gate.takeOutcome(o);
        check(got && o == TxOutcome::ACKED && s.gate.state() == TxState::FAULT && s.gate.stats().fault == TxFault::WINDOW &&
                  !s.port.on,
              "loop stall 30 ms in SENDING: after it the transmitter is off and FAULT (outside the invariant: the loop stood still)");
        Sim t;
        t.req();
        t.checkInvariant = false;
        t.now += 30;
        t.gate.tick(t.c, t.now);
        got = t.gate.takeOutcome(o);
        check(got && o == TxOutcome::TIMEOUT && t.gate.state() == TxState::FAULT && !t.port.on && t.port.sent.empty(),
              "loop stall 30 ms in OPENING: nothing sent, FAULT");
    }
}

static void testFlashAndForce() {
    std::printf("--- flash write during the window and forceClose ---\n");
    {
        Sim s;
        s.req();
        s.c.flashBusy = true;
        s.step();
        TxOutcome o = TxOutcome::NONE;
        bool got = s.gate.takeOutcome(o);
        check(got && o == TxOutcome::FAILED && s.gate.state() == TxState::COOLDOWN && !s.port.on && s.port.sent.empty(),
              "flashBusy in OPENING: nothing sent, FAILED, COOLDOWN");
        Sim t;
        t.port.script({0});
        t.req();
        t.step(2);
        t.c.flashBusy = true;
        t.step();
        got = t.gate.takeOutcome(o);
        check(got && o == TxOutcome::FAILED && t.gate.state() == TxState::COOLDOWN && !t.port.on && t.port.maxOnMs == 3,
              "flashBusy in SENDING: transmitter off, FAILED, COOLDOWN");
        check(s.clean() && t.clean(), "transmitter invariant");
    }
    {
        Sim a(1000, false);
        a.gate.forceClose();
        Sim b(1000, true, false);
        b.gate.forceClose();
        b.inv();
        Sim c;
        c.gate.forceClose();
        c.inv();
        check(a.gate.state() == TxState::NOT_BUILT && b.gate.state() == TxState::DISARMED && c.gate.state() == TxState::IDLE &&
                  !b.port.on && !c.port.on,
              "forceClose in NOT_BUILT, DISARMED, IDLE: state unchanged, transmitter off");
        Sim o;
        o.req();
        o.gate.forceClose();
        o.inv();
        TxOutcome out = TxOutcome::NONE;
        bool got = o.gate.takeOutcome(out);
        check(o.gate.state() == TxState::COOLDOWN && !o.port.on && got && out == TxOutcome::FAILED && o.port.sent.empty(),
              "forceClose in OPENING: transmitter off, nothing sent, result FAILED");
        o.step(600);
        check(o.gate.state() == TxState::IDLE, "after forceClose the gate returns to IDLE after the cooldown");
        Sim sn;
        sn.port.script({0});
        sn.req();
        sn.step(3);
        sn.gate.forceClose();
        sn.inv();
        got = sn.gate.takeOutcome(out);
        check(sn.gate.state() == TxState::COOLDOWN && !sn.port.on && got && out == TxOutcome::FAILED,
              "forceClose in SENDING: transmitter off, result FAILED");
        Sim cd;
        cd.sendOne();
        cd.gate.forceClose();
        cd.inv();
        Sim f;
        f.gate.latchFault(TxFault::OUTSIDE);
        f.gate.forceClose();
        f.inv();
        check(cd.gate.state() == TxState::COOLDOWN && f.gate.state() == TxState::FAULT && !cd.port.on && !f.port.on,
              "forceClose in COOLDOWN and FAULT: state unchanged, transmitter off");
        check(b.clean() && c.clean() && o.clean() && sn.clean() && cd.clean() && f.clean(), "transmitter invariant");
    }
}

// Long run with random events (generator with a fixed seed): requests, port results,
// delays, flash write, forceClose, latch and its release, disarming. Counts invariant violations.
static void testSoak(uint32_t start, const char* what) {
    Sim s(start);
    uint32_t seed = 0x508C0DEu;
    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return seed >> 8;
    };
    s.port.rest = 1;
    unsigned accepted = 0, onNotBuiltOrFault = 0;
    const uint32_t STEPS = 300000;
    for (uint32_t i = 0; i < STEPS; i++) {
        uint32_t r = rnd() % 1000;
        if (r < 30) {
            static const uint8_t RES[] = {1, 1, 1, 2, 3, 3, 4, 0};
            s.port.script({RES[rnd() % 8], RES[rnd() % 8], RES[rnd() % 8]}, 1);
            s.port.delayMs = rnd() % 30;
            if (s.req() == TxRefuse::OK) accepted++;
        } else if (r < 33) {
            s.gate.forceClose();
            s.inv();
        } else if (r < 36) {
            s.c.flashBusy = !s.c.flashBusy;
        } else if (r < 38) {
            s.gate.clearFault();
        } else if (r < 39) {
            s.gate.latchFault(TxFault::OUTSIDE);
            s.inv();
        } else if (r < 40) {
            s.gate.setArmed(false);
            s.inv();
        } else if (r < 42) {
            s.gate.setArmed(true);
        } else if (r < 44) {
            s.port.sendOk = !s.port.sendOk;
        }
        s.step();
        TxState st = s.gate.state();
        if (s.port.on && (st == TxState::NOT_BUILT || st == TxState::DISARMED || st == TxState::FAULT)) onNotBuiltOrFault++;
        TxOutcome o;
        s.gate.takeOutcome(o);
    }
    char line[240];
    std::snprintf(line, sizeof(line),
                  "%s: %lu steps of 1 ms, accepted requests %u, transmitter enables %u, longest %lu ms, invariant violations %u",
                  what, (unsigned long)STEPS, accepted, s.port.onCount, (unsigned long)s.port.maxOnMs, s.violations);
    check(s.violations == 0 && onNotBuiltOrFault == 0 && s.port.sendWhileOff == 0 && s.port.maxOnMs <= 20 && accepted > 20 &&
              s.port.onCount == accepted,
          line);
}

int main() {
    testNotBuiltDisarmed();
    testFrameList();
#if !CAN_TX_EXPERIMENTAL
    testExperimentalOff();
#endif
    testNormalSend();
    std::printf("--- limit of 10 frames per minute ---\n");
    testRate(1000, "start 1 s");
    testRate(0xFFFFFFFFu - 3000u, "millisecond counter wraps through zero meanwhile");
    testConditions();
    testArbitration();
    testFaults();
    testFlashAndForce();
    std::printf("--- invariant in a long random run ---\n");
    testSoak(1000, "start 1 s");
    testSoak(0xFFFFFFFFu - 100000u, "with the counter wrapping through zero");
    std::printf("can_tx_gate: %d checks, %d errors\n", checks, failures);
    return failures ? 1 : 0;
}
