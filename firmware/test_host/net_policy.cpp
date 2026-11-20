// Host test: decisions about Wi-Fi, standby and attempts to connect to the home network (net_policy).
//
// Build (from the firmware directory):
//   uvx --from ziglang python -m ziglang c++ -std=c++17 -O1 -Wall -I lib/core test_host/net_policy.cpp
//       lib/core/net_policy.cpp -o .build_host/net_policy.exe
#include <cmath>
#include <cstdio>

#include "net_policy.h"

static int failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) failures++;
}

static NetInputs awake() {
    NetInputs in;
    in.busAlive = true;
    in.accOn = true;
    return in;
}

static NetInputs quiet() { return NetInputs(); }

// The client polls at time 'ms'.
static NetInputs quietClient(uint32_t ms) {
    NetInputs in;
    in.clientSeen = true;
    in.lastClientMs = ms;
    return in;
}

static void testBasic() {
    NetPolicy p;
    NetDecision d = p.update(awake(), 1000);
    check(d.wifiOn && !d.holdAwake, "car awake: Wi-Fi on, no hold");
    NetInputs in = awake();
    in.enabled = false;
    d = p.update(in, 1100);
    check(!d.wifiOn, "the user switch turns Wi-Fi off");
    in = quiet();
    in.accOn = true;
    d = p.update(in, 1200);
    check(d.wifiOn, "ACC alone = car awake");
    in = quiet();
    in.usbPlugged = true;
    d = p.update(in, 1300);
    check(d.wifiOn && !d.holdAwake, "computer on USB = Wi-Fi on");

    // bus goes silent with no client: off immediately
    d = p.update(quiet(), 1400);
    check(!d.wifiOn && !d.holdAwake, "USB unplugged on a quiet bus without a client: Wi-Fi off");
}

static void testLinger(uint32_t t0, const char* label) {
    char name[160];
    NetPolicy p;
    NetDecision d = p.update(awake(), t0);
    // the client polled just before the bus went silent
    NetInputs in = quietClient(t0 + 500);
    d = p.update(in, t0 + 1000);    // bus goes silent
    std::snprintf(name, sizeof(name), "%s: going quiet with an active client holds", label);
    check(d.wifiOn && d.holdAwake, name);
    // the client stopped polling: 60 s from the last request
    d = p.update(in, t0 + 500 + 59999);
    std::snprintf(name, sizeof(name), "%s: 59.999 s after a request still holds", label);
    check(d.wifiOn && d.holdAwake, name);
    d = p.update(in, t0 + 500 + 60000);
    std::snprintf(name, sizeof(name), "%s: 60 s after a request the standby ends", label);
    check(!d.wifiOn && !d.holdAwake, name);
    // a late request after the decision does not resume standby
    in.lastClientMs = t0 + 500 + 60000;
    d = p.update(in, t0 + 500 + 60001);
    std::snprintf(name, sizeof(name), "%s: an ended standby does not come back without the car waking up", label);
    check(!d.wifiOn && !d.holdAwake, name);

    // 10 min limit with a client polling continuously
    NetPolicy q;
    q.update(awake(), t0);
    uint32_t quietAt = t0 + 100;
    bool held = true;
    uint32_t t = quietAt;
    for (; (uint32_t)(t - quietAt) < 600000; t += 1000) {
        d = q.update(quietClient(t), t);
        if (!d.holdAwake || !d.wifiOn) held = false;
    }
    d = q.update(quietClient(t), t);
    std::snprintf(name, sizeof(name), "%s: an active client holds up to 10 min, then the end", label);
    check(held && !d.holdAwake && !d.wifiOn, name);
    // after the car wakes up, standby is possible again
    q.update(awake(), t + 1000);
    d = q.update(quietClient(t + 2000), t + 2000);
    std::snprintf(name, sizeof(name), "%s: after the car wakes up a new standby", label);
    check(d.holdAwake && d.wifiOn, name);
}

static void testStartQuiet() {
    // start with a silent bus: the first update() = bus silent
    NetPolicy p;
    NetDecision d = p.update(quietClient(5000), 5000);
    check(d.wifiOn && d.holdAwake, "start on a quiet bus with a client: standby");
    d = p.update(quietClient(5000), 5000 + 59999);
    check(d.holdAwake, "start: the client holds 60 s");
    d = p.update(quietClient(5000), 5000 + 60000);
    check(!d.holdAwake && !d.wifiOn, "start: after 60 s without requests the end");

    NetPolicy q;
    bool held = true;
    uint32_t t = 5000;
    for (; t < 5000 + 600000; t += 10000)
        if (!q.update(quietClient(t), t).holdAwake) held = false;
    d = q.update(quietClient(t), t);
    check(held && !d.holdAwake, "start: in total at most 10 min from the first update()");

    NetPolicy r;
    d = r.update(quiet(), 5000);
    check(d.wifiOn && d.holdAwake, "start on a quiet bus without a client: Wi-Fi for 60 s (a client has time to ask)");
    d = r.update(quiet(), 5000 + 59999);
    check(d.wifiOn, "start without a client: still in the 60 s window");
    d = r.update(quiet(), 5000 + 60000);
    check(!d.wifiOn && !d.holdAwake, "start without a client: after 60 s Wi-Fi off");

    // start with a silent bus, the client polls from t = 5 s every 5 s: standby longer than 60 s, up to 10 min
    NetPolicy s;
    NetInputs in = quiet();
    d = s.update(in, 0);
    bool on = d.wifiOn;
    t = 0;
    t = 5000;
    for (; t < 600000; t += 5000) {
        in = quietClient(t);
        if (!s.update(in, t).wifiOn) on = false;
    }
    check(on, "start, client since 5 s: Wi-Fi on for more than 60 s up to 10 min");
    d = s.update(quietClient(t), t);
    check(!d.wifiOn && !d.holdAwake, "start, client since 5 s: after 10 min Wi-Fi off");

    // start after an update: confirmation waits 90 s (longer than the start window), the client polls from 95 s
    NetPolicy u;
    in = quiet();
    in.otaActive = true;
    for (uint32_t k = 0; k <= 90000; k += 1000) u.update(in, k);
    in = quiet();
    on = true;
    for (t = 95000; t < 300000; t += 5000) {
        in = quietClient(t);
        if (!u.update(in, t).wifiOn) on = false;
    }
    check(on, "after a new program is confirmed the client holds Wi-Fi (quiet bus)");
}

static void testClientTime() {
    NetPolicy p;
    p.update(awake(), 1000);
    // client timestamp written by the other core later than nowMs: age 0, not "wrapped"
    NetDecision d = p.update(quietClient(2005), 2000);
    check(d.holdAwake && d.wifiOn, "lastClientMs = nowMs + 5 counts as age 0");
    d = p.update(quietClient(2005), 2005 + 59999);
    check(d.holdAwake, "then the usual 60 s from that timestamp");

    NetPolicy q;
    q.update(awake(), 1000);
    NetInputs in = quietClient(2000);
    in.clientSeen = false;
    d = q.update(in, 2000);
    check(!d.holdAwake, "clientSeen == false: the timestamp does not count");
}

static void testBattery() {
    NetPolicy p;
    p.update(awake(), 1000);
    NetInputs in = quietClient(2000);
    in.batteryMeasured = true;
    in.batteryV = 12.1f;
    NetDecision d = p.update(in, 2000);
    check(d.holdAwake, "voltage above the threshold: standby");
    in.batteryV = 11.79f;
    d = p.update(in, 3000);
    check(!d.holdAwake && !d.wifiOn, "voltage below the threshold: end of standby");

    NetPolicy q;
    q.update(awake(), 1000);
    in = quietClient(2000);
    in.batteryMeasured = false;
    in.batteryV = 3.0f;
    d = q.update(in, 2000);
    check(d.holdAwake, "voltage not measured: no threshold");

    NetPolicy r;
    r.update(awake(), 1000);
    in = quietClient(2000);
    in.batteryMeasured = true;
    in.batteryV = NAN;
    d = r.update(in, 2000);
    check(!d.holdAwake, "voltage NaN while measuring: end of standby");

    NetPolicy s;
    s.update(awake(), 1000);
    in = quietClient(2000);
    in.batteryMeasured = true;
    in.batteryV = 11.8f;
    d = s.update(in, 2000);
    check(d.holdAwake, "voltage equal to the threshold: standby");

    // the voltage does not matter while the car is awake
    NetPolicy u;
    in = awake();
    in.batteryMeasured = true;
    in.batteryV = 10.0f;
    d = u.update(in, 1000);
    check(d.wifiOn, "car awake: low voltage does not turn Wi-Fi off");
}

static void testOta() {
    NetPolicy p;
    p.update(awake(), 1000);
    NetInputs in = quiet();
    in.otaActive = true;
    NetDecision d = p.update(in, 2000);
    check(d.holdAwake && d.wifiOn, "otaActive: holds and Wi-Fi on");
    in.enabled = false;
    d = p.update(in, 3000);
    check(d.holdAwake && !d.wifiOn, "otaActive with enabled == false: holds, Wi-Fi off");
    in.uploadBusy = true;
    d = p.update(in, 4000);
    check(d.holdAwake && d.wifiOn, "uploadBusy: Wi-Fi on also with enabled == false");
    in.uploadBusy = false;
    d = p.update(in, 5000);
    check(d.holdAwake && !d.wifiOn, "upload finished: the switch works");
    in.otaActive = false;
    d = p.update(in, 6000);
    check(!d.holdAwake && !d.wifiOn, "otaActive released: holdAwake == false in the same call");

    // otaActive also holds after 10 minutes of silence
    NetPolicy q;
    q.update(awake(), 0);
    q.update(quiet(), 100);
    in = quiet();
    in.otaActive = true;
    d = q.update(in, 900000);
    check(d.holdAwake && d.wifiOn, "otaActive holds regardless of the 10 min limit");
    in.otaActive = false;
    d = q.update(in, 900001);
    check(!d.holdAwake && !d.wifiOn, "after otaActive without a client: the end");
}

static void testSta(uint32_t t0, const char* label) {
    char name[160];
    NetPolicy p;
    NetInputs in = awake();
    in.staConfigured = true;
    NetDecision d = p.update(in, t0);
    std::snprintf(name, sizeof(name), "%s: no pulse at once (the start makes the first connection)", label);
    check(!d.staAttempt, name);
    int pulses = 0;
    bool earlyPulse = false;
    for (uint32_t t = 1000; t <= 120000; t += 1000) {
        d = p.update(in, t0 + t);
        if (d.staAttempt) {
            pulses++;
            if (t % 30000 != 0) earlyPulse = true;
        }
    }
    std::snprintf(name, sizeof(name), "%s: tries every 30 s", label);
    check(pulses == 4 && !earlyPulse, name);

    // someone on our network: every 5 min
    NetPolicy q;
    in.apStations = 1;
    q.update(in, t0);
    pulses = 0;
    for (uint32_t t = 1000; t <= 600000; t += 1000)
        if (q.update(in, t0 + t).staAttempt) pulses++;
    std::snprintf(name, sizeof(name), "%s: with a client on our network every 5 min", label);
    check(pulses == 2, name);

    // engine running: never
    NetPolicy r;
    in.apStations = 0;
    in.engineRunning = true;
    pulses = 0;
    for (uint32_t t = 0; t <= 600000; t += 1000)
        if (r.update(in, t0 + t).staAttempt) pulses++;
    std::snprintf(name, sizeof(name), "%s: never with the engine running", label);
    check(pulses == 0, name);
    // after the engine is switched off, the first pulse after a full 30 s
    in.engineRunning = false;
    bool at30 = false, before30 = false;
    for (uint32_t t = 601000; t <= 631000; t += 1000) {
        bool a = r.update(in, t0 + t).staAttempt;
        if (a && t < 631000) before30 = true;
        if (a && t == 631000) at30 = true;
    }
    std::snprintf(name, sizeof(name), "%s: after the engine stops the first try after 30 s", label);
    check(at30 && !before30, name);

    // connected, not configured, Wi-Fi off, flashing: never
    NetPolicy s;
    NetInputs a = awake();
    a.staConfigured = true;
    a.staConnected = true;
    NetInputs b = awake();
    NetInputs c = awake();
    c.staConfigured = true;
    c.enabled = false;
    NetInputs e = awake();
    e.staConfigured = true;
    e.uploadBusy = true;
    e.otaActive = true;
    pulses = 0;
    NetPolicy s2, s3, s4;
    for (uint32_t t = 0; t <= 300000; t += 1000) {
        if (s.update(a, t0 + t).staAttempt) pulses++;
        if (s2.update(b, t0 + t).staAttempt) pulses++;
        if (s3.update(c, t0 + t).staAttempt) pulses++;
        if (s4.update(e, t0 + t).staAttempt) pulses++;
    }
    std::snprintf(name, sizeof(name), "%s: no tries when connected / no network / Wi-Fi off / uploading", label);
    check(pulses == 0, name);
}

int main() {
    testBasic();
    testLinger(1000, "standby");
    testLinger(0xFFFFFFFFu - 30000, "standby with millis wrapping");
    testStartQuiet();
    testClientTime();
    testBattery();
    testOta();
    testSta(0, "home network");
    testSta(0xFFFFFFFFu - 100000, "home network with millis wrapping");
    if (failures == 0) std::printf("\nALL OK\n");
    else std::printf("\nERRORS: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
