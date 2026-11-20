// Host test: the link to the radio (lib/radio_xp/radio_link.h) and transmission to the radio from the Decoder class
// (Decoder::nextRadioTx). Records the CURRENT behaviour, so that later
// deliberate stream changes have a reference point:
//   - ACC gate: without ACC only the power frame EA goes into the queue; txGated counter;
//   - state frames (slots): sent only when the content changes, 12 kinds, the 13th skipped (slotsFull);
//   - steering angle (29) at most every 150 ms;
//   - sensors (32): on change and every 1 s while active;
//   - radio request 90 71: version, sensors, all state frames;
//   - full queue (24 frames): txDropped counter;
//   - nextRadioTx: 10 ms interval, queue emptied while transmission is disabled, a frame that does not fit
//     in the buffer is dropped and does not shift the time.
//
// Run: bash tools/run_host_tests.sh (README.md, section on tests on recordings).
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>

#include "decoder.h"
#include "radio_link.h"
#include "xp_parking.h"
#include "xp_protocol.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

static xp::Frame frame(uint8_t cmd, std::initializer_list<uint8_t> d) {
    xp::Frame f;
    f.cmd = cmd;
    f.len = (uint8_t)d.size();
    uint8_t i = 0;
    for (uint8_t b : d) f.data[i++] = b;
    return f;
}

static std::string text(const xp::Frame& f) {
    char b[8];
    std::snprintf(b, sizeof(b), "%02X", f.cmd);
    std::string s = b;
    for (uint8_t i = 0; i < f.len; i++) {
        std::snprintf(b, sizeof(b), " %02X", f.data[i]);
        s += b;
    }
    return s;
}

// Empties the queue and returns the frames as text "CMD DATA | CMD DATA".
static std::string drain(RadioLink& r) {
    std::string s;
    xp::Frame f;
    while (r.popTx(f)) {
        if (!s.empty()) s += " | ";
        s += text(f);
    }
    return s;
}

// Check of the queue content; prints it so that the golden file shows the bytes.
static void expectQueue(RadioLink& r, const char* want, const char* what) {
    std::string got = drain(r);
    std::printf("      queue: %s\n", got.empty() ? "(empty)" : got.c_str());
    check(got == want, what);
}

static xp::KeyEvent key(uint8_t code, uint8_t state) {
    xp::KeyEvent e;
    e.code = code;
    e.state = state;
    return e;
}

// Sensors: front active, rear waiting, obstacle at the front centre in zone 'zone'.
static psa::ParkingAid parking(uint8_t frontCenterZone) {
    psa::ParkingAid pa;
    pa.rearStatus = psa::PARK_WAITING;
    pa.frontStatus = psa::PARK_ACTIVE;
    pa.frontCenter = frontCenterZone;
    return pa;
}

static psa::ParkingAid parkingOff() {
    psa::ParkingAid pa;
    pa.rearStatus = psa::PARK_WAITING;
    pa.frontStatus = psa::PARK_WAITING;
    return pa;
}

// ---------- ACC gate ----------
static void testAccGate() {
    std::printf("--- ACC gate ---\n");
    RadioLink r;
    r.onKey(key(0x14, 1));
    check(r.txQueued() == 0 && r.txGated() == 1, "without ACC: a key does not get into the queue, txGated = 1");
    r.onParking(parking(3), true, 100);
    check(r.txQueued() == 0 && r.txGated() == 2, "without ACC: the sensor frame is dropped, txGated = 2");
    r.onState(frame(0x36, {0x5E}), 100);
    r.update(110);
    check(r.txQueued() == 0 && r.txGated() == 2, "without ACC: the state frame waits in its slot (does not count as dropped)");
    check(r.resendState(0x36) && r.txQueued() == 0 && r.txGated() == 2, "without ACC: resendState returns true, sends nothing");
    r.onRadioFrame(frame(0x90, {0x71}), 120);
    check(r.txQueued() == 0 && r.txGated() == 4, "without ACC: the reply to 90 71 (version + sensors) is dropped, txGated = 4");

    r.onPower(true, 200);
    r.update(200);
    expectQueue(r, "EA 01 0A | 36 5E", "ACC on: EA 01 0A, then the waiting state frame");
    r.onPower(true, 210);
    check(r.txQueued() == 0, "repeated onPower(true): no second EA frame");
    r.onKey(key(0x14, 1));
    r.onKey(key(0x14, 0));
    expectQueue(r, "20 14 01 | 20 14 00", "with ACC: key pressed and released in the queue");

    r.onPower(false, 300);
    r.onKey(key(0x14, 1));
    expectQueue(r, "EA 00 0A", "ACC off: EA 00 0A goes through, a key after it does not");
    check(r.txGated() == 5 && r.txDropped() == 0, "counters: txGated = 5, txDropped = 0");
}

// ---------- state frames ----------
static void testSlots() {
    std::printf("--- state frames (slots) ---\n");
    RadioLink r;
    r.onPower(true, 0);
    drain(r);
    r.onState(frame(0x36, {0x5E}), 10);
    r.update(10);
    expectQueue(r, "36 5E", "new state frame: sent in update()");
    r.onState(frame(0x36, {0x5E}), 20);
    r.update(20);
    check(r.txQueued() == 0, "same content: not sent");
    r.onState(frame(0x36, {0x60}), 30);
    r.onState(frame(0x36, {0x62}), 31);
    r.update(32);
    expectQueue(r, "36 62", "two changes before update(): only the last content goes");
    r.onState(frame(0x36, {0x62, 0x00}), 40);
    r.update(40);
    expectQueue(r, "36 62 00", "same content, different length: sent");

    // 12 kinds of state frames fit in the slots (36 is already there), the 13th is skipped.
    for (uint8_t i = 0; i < 11; i++) r.onState(frame((uint8_t)(0x40 + i), {i}), 50);
    check(r.slotsFull() == 0, "12 kinds of state frames: slotsFull = 0");
    r.update(50);
    expectQueue(r, "40 00 | 41 01 | 42 02 | 43 03 | 44 04 | 45 05 | 46 06 | 47 07 | 48 08 | 49 09 | 4A 0A",
                "11 new frames sent in the order of first appearance");
    r.onState(frame(0x60, {0xAA}), 60);
    r.onState(frame(0x60, {0xAB}), 61);
    r.update(62);
    check(r.txQueued() == 0 && r.slotsFull() == 2, "13th kind: skipped, slotsFull grows with every try (2)");
    r.onState(frame(0x4A, {0x55}), 70);
    r.update(70);
    expectQueue(r, "4A 55", "full slots do not block changes of known frames");
    check(!r.resendState(0x60), "resendState for a frame without a slot: false");
    check(r.resendState(0x36), "resendState(36): true");
    expectQueue(r, "36 62 00", "resendState: the current content once more");
}

// ---------- steering angle ----------
static void testSteering() {
    std::printf("--- steering angle (29) every 150 ms ---\n");
    RadioLink r;
    r.onPower(true, 1000);
    drain(r);
    r.onState(frame(0x29, {0x00, 0x10}), 1000);
    r.onState(frame(0x36, {0x5E}), 1000);
    r.update(1000);
    expectQueue(r, "29 00 10 | 36 5E", "the first angle frame sent at once");
    r.onState(frame(0x29, {0x00, 0x11}), 1010);
    r.onState(frame(0x36, {0x60}), 1010);
    r.update(1010);
    expectQueue(r, "36 60", "10 ms later: the angle waits, another state frame goes at once");
    r.onState(frame(0x29, {0x00, 0x12}), 1100);
    r.update(1149);
    check(r.txQueued() == 0, "149 ms after the previous angle: still waits");
    r.update(1150);
    expectQueue(r, "29 00 12", "150 ms: the last angle content goes");
    r.update(1400);
    check(r.txQueued() == 0, "content unchanged: not sent");
    r.onState(frame(0x29, {0x00, 0x13}), 1401);
    r.update(1401);
    expectQueue(r, "29 00 13", "change after a longer gap: at once");
    // Millisecond counter wrapping through zero.
    RadioLink w;
    w.onPower(true, 0xFFFFFF00u);
    drain(w);
    w.onState(frame(0x29, {0x01}), 0xFFFFFFF0u);
    w.update(0xFFFFFFF0u);
    drain(w);
    w.onState(frame(0x29, {0x02}), 0x00000010u);
    w.update(0x00000010u);
    bool early = w.txQueued() == 0;
    w.update(0xFFFFFFF0u + 150u);
    std::string late = drain(w);
    check(early && late == "29 02", "counter wrapping through zero: 32 ms too early, 150 ms sent");
}

// ---------- parking sensors ----------
static void testParking() {
    std::printf("--- sensors (32): on change and every 1 s ---\n");
    RadioLink r;
    r.onPower(true, 0);
    drain(r);
    r.onParking(parking(3), true, 1000);
    expectQueue(r, "32 01 05 05 05 05 02 05", "active sensors: frame 32 at once");
    r.onParking(parking(3), true, 1100);
    r.update(1999);
    check(r.txQueued() == 0, "same content and 999 ms: not sent");
    r.update(2000);
    expectQueue(r, "32 01 05 05 05 05 02 05", "1000 ms: repeat");
    r.update(2500);
    r.onParking(parking(1), true, 2500);
    expectQueue(r, "32 01 05 05 05 05 01 05", "zone change: at once");
    r.update(3499);
    check(r.txQueued() == 0, "a change moves the repeat deadline (999 ms after the change: nothing)");
    r.update(3500);
    expectQueue(r, "32 01 05 05 05 05 01 05", "1000 ms after the change: repeat");
    r.onParking(parkingOff(), true, 3600);
    expectQueue(r, "32 00 FF FF FF FF FF FF", "sensors inactive: one frame with FF");
    r.update(6000);
    check(r.txQueued() == 0, "inactive: no repeating");
    r.onParking(parking(1), false, 6100);
    check(r.txQueued() == 0, "no fresh 0E1 frames (valid = false) with the same content 00 FF: not sent");

    // After ACC goes off and on again, the sensor state is sent afresh with the next frame.
    r.onParking(parking(2), true, 7000);
    drain(r);
    r.onPower(false, 7100);
    r.onPower(true, 7200);
    drain(r);
    r.update(9000);
    check(r.txQueued() == 0, "after ACC comes back: no repeating until a sensor frame arrives");
    r.onParking(parking(2), true, 9100);
    expectQueue(r, "32 01 05 05 05 05 01 05", "after ACC comes back: the same content sent once more");
}

// ---------- request 90 71 ----------
static void testRequest() {
    std::printf("--- radio request 90 71 ---\n");
    RadioLink r;
    r.onPower(true, 0);
    drain(r);
    r.onRadioFrame(frame(0x90, {0x71}), 100);
    expectQueue(r, "71 0A 05 16 10 20 56 02 19 06", "without state: the version only");
    r.onState(frame(0x29, {0x00, 0x10}), 200);
    r.onState(frame(0x36, {0x5E}), 200);
    r.update(200);
    r.onParking(parking(3), true, 200);
    drain(r);
    r.onState(frame(0x29, {0x00, 0x11}), 210);     // angle changed 10 ms after a send: normally it would wait
    r.onRadioFrame(frame(0x90, {0x71}), 220);
    expectQueue(r, "71 0A 05 16 10 20 56 02 19 06 | 32 01 05 05 05 05 02 05 | 29 00 11 | 36 5E",
                "version, sensors, all state frames (angle without the 150 ms wait)");
    r.update(230);
    check(r.txQueued() == 0, "after the reply nothing is pending");
    r.onRadioFrame(frame(0x90, {0x21}), 300);
    r.onRadioFrame(frame(0x90, {}), 310);
    check(r.txQueued() == 0, "90 21 and 90 without data: no reply (current state)");
}

// ---------- full queue and data from the radio ----------
static void testQueueAndInfo() {
    std::printf("--- full queue, data from the radio ---\n");
    RadioLink r;
    r.onPower(true, 0);                     // 1 EA frame in the queue
    for (uint8_t i = 0; i < 23; i++) r.onKey(key(i, 1));
    check(r.txQueued() == RadioLink::txCapacity() && r.txDropped() == 0, "24 frames: queue full, txDropped = 0");
    r.onKey(key(0x77, 1));
    r.onKey(key(0x78, 1));
    check(r.txQueued() == 24 && r.txDropped() == 2, "the next two are lost: txDropped = 2");
    xp::Frame f;
    r.popTx(f);
    bool first = f.cmd == 0xEA;
    xp::Frame last;
    while (r.popTx(f)) last = f;
    check(first && last.cmd == 0x20 && last.data[0] == 22, "order kept: first EA, last key no. 22");
    check(r.txGated() == 0 && r.slotsFull() == 0, "counters txGated and slotsFull unchanged (0)");

    const RadioInfo& info = r.info();
    check(!info.everHeard, "before the first frame: everHeard = false");
    r.onRadioFrame(frame(0xA6, {26, 11, 18, 2, 5}), 500);
    r.onRadioFrame(frame(0xC4, {12}), 510);
    r.onRadioFrame(frame(0x81, {0x01}), 520);
    check(info.everHeard && info.lastFrameMs == 520 && info.haveTime && info.year == 26 && info.minute == 5 &&
              info.volume == 12 && !info.muted && info.radioOn,
          "A6, C4, 81: time, volume, radio on");
    r.onRadioFrame(frame(0xC4, {0x80}), 530);
    check(info.muted && info.volume == 12, "C4 80: muted, volume remembered");
    r.onRadioFrame(frame(0xA6, {26, 11}), 540);
    check(info.year == 26 && info.day == 18, "A6 too short: unchanged");
    r.onRadioFrame(frame(0x80, {0x01, 0x01}), 550);
    r.onRadioFrame(frame(0x80, {0x01}), 551);
    r.onRadioFrame(frame(0x8A, {0x04, 0x02}), 560);
    r.onRadioFrame(frame(0x55, {0x00}), 570);
    check(info.settingCount == 1 && info.lastSettingId == 1 && info.lastSettingValue == 1 && info.airCount == 1 &&
              info.lastAirId == 4 && info.lastAirValue == 2 && info.unknownCount == 1,
          "80, 8A, unknown: counters 1 / 1 / 1 (a too short 80 is not counted)");
    check(r.txQueued() == 0, "radio frames other than 90 71 send nothing");
}

// ---------- Decoder::nextRadioTx ----------
static CanFrame can0F6(uint8_t ignition) {
    CanFrame f;
    f.id = 0x0F6;
    f.len = 8;
    f.data[0] = ignition;
    f.data[6] = 0x5E;        // outside temperature: 0x5E / 2 - 40 = 7 deg C
    return f;
}

static std::string hex(const uint8_t* b, size_t n) {
    std::string s;
    char t[8];
    for (size_t i = 0; i < n; i++) {
        std::snprintf(t, sizeof(t), i ? " %02X" : "%02X", b[i]);
        s += t;
    }
    return s;
}

static void testNextRadioTx() {
    std::printf("--- Decoder::nextRadioTx ---\n");
    Decoder d;
    DecoderConfig cfg;
    cfg.decoderMode = true;
    d.begin(cfg, nullptr);
    uint8_t buf[xp::MAX_DATA + 4];

    check(d.nextRadioTx(500, true, buf, sizeof(buf)) == 0, "empty queue: 0");
    // Ignition on: in the same pass ACC and three frames (EA, sensors "no data", temperature).
    d.onCanFrame(can0F6(0x88), 1000, true);
    d.tick(1000, false);
    check(d.link().txQueued() == 3, "after the ignition is on, 3 frames in the queue");
    size_t n = d.nextRadioTx(1000, true, buf, sizeof(buf));
    std::printf("      1000 ms: %s\n", hex(buf, n).c_str());
    check(hex(buf, n) == "2E EA 02 01 0A 08", "first frame: EA 01 0A with header and checksum");
    bool gap = d.nextRadioTx(1000, true, buf, sizeof(buf)) == 0 && d.nextRadioTx(1005, true, buf, sizeof(buf)) == 0 &&
               d.nextRadioTx(1009, true, buf, sizeof(buf)) == 0;
    check(gap && d.link().txQueued() == 2, "0, 5 and 9 ms after the frame: nothing, queue untouched");
    n = d.nextRadioTx(1010, true, buf, sizeof(buf));
    std::printf("      1010 ms: %s\n", hex(buf, n).c_str());
    check(n == 11 && buf[1] == 0x32, "10 ms after the frame: the next one (32)");
    check(d.radioTxSent() == 2 && d.radioTxSuppressed() == 0, "sent counter = 2");

    // Frame that does not fit in the buffer: dropped, time of the last send unchanged.
    d.injectKey(0x14, 1);
    check(d.link().txQueued() == 2, "in the queue: temperature and a key");
    n = d.nextRadioTx(1020, true, buf, 4);
    check(n == 0 && d.link().txQueued() == 1 && d.radioTxSent() == 2, "buffer 4 B: frame 36 is lost, counter unchanged");
    n = d.nextRadioTx(1020, true, buf, sizeof(buf));
    std::printf("      1020 ms: %s\n", hex(buf, n).c_str());
    check(hex(buf, n) == "2E 20 02 14 01 C8", "in the same millisecond the next frame goes (key)");
    check(d.nextRadioTx(1029, true, buf, sizeof(buf)) == 0, "the gap is counted from a successful send");

    // Transmission disabled (mode alongside the box): queue emptied, nothing goes out.
    d.injectKey(0x15, 1);
    d.injectKey(0x15, 0);
    d.injectKey(0x16, 1);
    n = d.nextRadioTx(2000, false, buf, sizeof(buf));
    check(n == 0 && d.link().txQueued() == 0 && d.radioTxSuppressed() == 3 && d.radioTxSent() == 3,
          "txEnabled = false: 0 bytes, queue empty, held back = 3");
    d.injectKey(0x17, 1);
    n = d.nextRadioTx(2001, true, buf, sizeof(buf));
    check(n == 6 && buf[3] == 0x17, "after transmit is enabled: at once (discarded frames do not move the time)");

    // Millisecond counter wrapping through zero: the 10 ms interval still works.
    Decoder w;
    w.begin(cfg, nullptr);
    w.onCanFrame(can0F6(0x88), 0xFFFFFFF0u, true);
    w.tick(0xFFFFFFF0u, false);
    size_t a = w.nextRadioTx(0xFFFFFFF8u, true, buf, sizeof(buf));
    size_t b = w.nextRadioTx(0x00000001u, true, buf, sizeof(buf));
    size_t c = w.nextRadioTx(0x00000002u, true, buf, sizeof(buf));
    check(a == 6 && b == 0 && c == 11, "counter wrapping through zero: 9 ms nothing, 10 ms the next frame");

    // txGated counter through Decoder: a button with the ignition off.
    Decoder off;
    off.begin(cfg, nullptr);
    off.tick(100, false);
    uint32_t before = off.link().txGated();
    off.injectKey(0x14, 1);
    check(off.link().txGated() == before + 1 && off.link().txQueued() == 0, "Decoder without ACC: a key is counted in txGated");
}

// ---------- fixes after the code review of 19.11.2026 ----------
static std::string s_log;
static void logToString(const char* line) { s_log += line; }

static size_t encodeNothing(const RadioFrame&, uint8_t*, size_t) { return 0; }

// The 21F frame with the "volume +" bit of the PSA AEE2010 key map (row VOLUME_UP), other keys released.
static CanFrame volumeUp(bool down) {
    CanFrame f;
    f.id = 0x21F;
    f.len = 3;
    for (uint8_t i = 0; i < kCarPsaAee2010.keyCount; i++) {
        const vehicle::KeyMapEntry& k = kCarPsaAee2010.keys[i];
        if (k.key == vehicle::KeyId::VOLUME_UP && down) f.data[k.byteIndex] = (uint8_t)((f.data[k.byteIndex] & ~k.mask) | k.value);
    }
    return f;
}

static void testReviewFixes() {
    std::printf("--- fixes after the code review ---\n");
    {
        // The power frame with a full queue: before, it was dropped silently and never sent again.
        RadioLink r;
        r.onPower(true, 0);
        for (uint8_t i = 0; i < 23; i++) r.onKey(key(i, 1));
        check(r.txQueued() == 24, "queue full (EA + 23 keys)");
        r.onPower(false, 10);
        xp::Frame f, last;
        uint8_t n = 0;
        while (r.popTx(f)) {
            last = f;
            n++;
        }
        check(n == 24 && last.cmd == 0xEA && last.len >= 1 && last.data[0] == 0x00 && r.txDropped() == 1,
              "ACC off with a full queue: EA 00 0A still queued (the oldest frame dropped, txDropped = 1)");
    }
    {
        // State frames keyed by sub-type bytes (RadioProtocol::stateKeyBytes): a protocol with one "car data"
        // frame number and the page in DATA[0] keeps both pages. XP (0) keeps one slot per number, as before.
        RadioProtocol paged = kRadioXp;
        paged.stateKeyBytes = 1;
        RadioSession s(&paged);
        s.onPower(true, 0);
        RadioFrame a;
        s.popTx(a);                                  // EA
        s.onState(frame(0x40, {0x01, 0x11}), 10);
        s.onState(frame(0x40, {0x02, 0x22}), 10);
        s.update(10);
        std::string got;
        while (s.popTx(a)) got += (got.empty() ? "" : " | ") + text(a);
        std::printf("      queue: %s\n", got.c_str());
        check(got == "40 01 11 | 40 02 22", "stateKeyBytes = 1: pages 1 and 2 of frame 40 both sent");
        check(s.resendState(0x40), "resendState(40): true");
        got.clear();
        while (s.popTx(a)) got += (got.empty() ? "" : " | ") + text(a);
        check(got == "40 01 11 | 40 02 22", "resendState(40): both pages again");
        RadioLink x;
        x.onPower(true, 0);
        drain(x);
        x.onState(frame(0x40, {0x01, 0x11}), 10);
        x.onState(frame(0x40, {0x02, 0x22}), 10);
        x.update(10);
        expectQueue(x, "40 02 22", "XP (stateKeyBytes = 0): one slot per frame number, unchanged");
    }
    {
        // A held key without fresh CAN frames is released after KeyEncoder::STALE_MS.
        xp::KeyEncoder k;
        k.onCanFrame(volumeUp(true), 0);
        int held = 0, released = -1;
        xp::KeyEvent e;
        for (uint32_t t = 1; t <= 6000; t++) {
            k.update(t);
            while (k.pop(e)) {
                if (e.state == 2) held++;
                if (e.state == 0 && released < 0) released = (int)t;
            }
        }
        std::printf("      held events %d, released at %d ms\n", held, released);
        check(released > 0 && released <= (int)xp::KeyEncoder::STALE_MS + 200,
              "reception stops with volume + held: released within STALE_MS + one 200 ms tick");
        xp::KeyEncoder h;
        h.onCanFrame(volumeUp(true), 0);
        bool early = false;
        int heldOk = 0;
        for (uint32_t t = 1; t <= 6000; t++) {
            if (t % 100 == 0) h.onCanFrame(volumeUp(true), t);    // the car keeps sending 21F every 100 ms
            h.update(t);
            while (h.pop(e)) {
                if (e.state == 0) early = true;
                if (e.state == 2) heldOk++;
            }
        }
        check(!early && heldOk == 30, "frames keep coming every 100 ms: held for 6 s, no release (as before)");
    }
    {
        // A frame the protocol cannot frame (encodeFrame returns 0) is counted, not lost without a trace.
        RadioProtocol tiny = kRadioXp;
        tiny.encodeFrame = encodeNothing;
        Decoder d;
        DecoderConfig cfg;
        cfg.decoderMode = true;
        cfg.radio = &tiny;
        d.begin(cfg, nullptr);
        d.onCanFrame(can0F6(0x88), 0, true);
        d.tick(0, false);
        uint8_t buf[RADIO_TX_MAX_BYTES];
        size_t n = d.nextRadioTx(100, true, buf, sizeof(buf));
        check(n == 0 && d.radioTxTooLong() == 1 && d.radioTxSent() == 0, "encodeFrame = 0: radioTxTooLong = 1");
    }
    {
        // Default key remaps of the PSA profile are XP codes: only with the XP protocol.
        RadioProtocol mine = kRadioXp;
        mine.id = "mine";
        char map[xp::KeyCustomizer::MAP_TEXT_MAX];
        Decoder a;
        DecoderConfig cfg;
        a.begin(cfg, nullptr);
        a.keyMap().serializeMap(map, sizeof(map));
        std::printf("      remaps with xp: '%s'\n", map);
        const bool xpHas = map[0] != 0;
        Decoder b;
        cfg.radio = &mine;
        b.begin(cfg, nullptr);
        b.keyMap().serializeMap(map, sizeof(map));
        std::printf("      remaps with another protocol: '%s'\n", map);
        check(xpHas && map[0] == 0, "PSA profile remaps (XP codes): applied with xp, not with another protocol");

        // Setting / climate / sound numbers are XP (vset) numbers only when the protocol says so.
        mine.commandsUseVsetNumbers = false;
        Decoder c;
        c.begin(cfg, logToString);
        s_log.clear();
        c.onRadioFrame(frame(0x80, {0x0C, 0x01}), 100);
        std::printf("      log: %s", s_log.c_str());
        check(s_log.find("not supported") != std::string::npos && s_log.find("[vset]") == std::string::npos &&
                  s_log.find("80 0C") == std::string::npos,
              "foreign protocol: setting command not taken for an XP item, no XP bytes in the line");
        Decoder x;
        DecoderConfig xcfg;
        x.begin(xcfg, logToString);
        s_log.clear();
        x.onRadioFrame(frame(0x80, {0x0C, 0x01}), 100);
        check(s_log.find("[vset] radio: 80 0C 01") != std::string::npos, "XP: the setting command as before");
    }
}

int main() {
    testAccGate();
    testSlots();
    testSteering();
    testParking();
    testRequest();
    testQueueAndInfo();
    testNextRadioTx();
    testReviewFixes();
    std::printf("radio_link: %d checks, %d errors\n", checks, failures);
    return failures ? 1 : 0;
}
