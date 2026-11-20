// Host test: main loop decisions without hardware (lib/core/loop_policy.h)
// ("deferred save test").
//   - SaveDeferral: saving settings to flash is deferred while the car needs the device
//     (reverse, active sensors), executed once that ends, at most one per loop pass;
//   - Deadline: a deadline with an "active" flag (test beep, amplifier hold), which does not
//     come back after 24.9 days of uptime (millis() wrap-around);
//   - DriverInit: after a driver error at startup the 12 V outputs stay off, the amplifier asleep and the
//     LED red on every loop pass (review: the first pass used to turn the red LED off).
//
// Run: bash tools/run_host_tests.sh (README.md, section on tests on recordings).
#include <cstdio>

#include "loop_policy.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

static void testSaveDeferral() {
    std::printf("--- deferral of settings writes ---\n");
    const uint8_t VOL = SaveDeferral::VOLUME, SAVE = SaveDeferral::KEYMAP_SAVE, CLEAR = SaveDeferral::KEYMAP_CLEAR;
    {
        SaveDeferral s;
        check(s.pending() == 0 && s.takeDue(false) == 0 && s.takeDue(true) == 0, "after startup nothing is pending");
        check(s.request(VOL, false) && s.pending() == 0 && s.deferred() == 0, "car free: write at once, nothing pending");
        check(s.request(SAVE, false) && s.request(CLEAR, false) && s.pending() == 0, "car free: the table at once too");
    }
    {
        SaveDeferral s;
        check(!s.request(VOL, true) && s.pending() == VOL && s.deferred() == 1, "car busy: volume deferred");
        check(s.takeDue(true) == 0 && s.pending() == VOL, "still busy: nothing to do, the write waits");
        bool waits = true;
        for (int i = 0; i < 1000; i++)
            if (s.takeDue(true) != 0) waits = false;
        check(waits && s.pending() == VOL, "1000 passes with the car busy: no write at all");
        check(s.takeDue(false) == VOL && s.pending() == 0, "car free: volume write to do, once");
        check(s.takeDue(false) == 0, "the second time nothing");
    }
    {
        SaveDeferral s;
        s.request(VOL, true);
        s.request(VOL, true);
        check(s.pending() == VOL && s.deferred() == 2, "volume twice: one write pending (the console keeps the value)");
        s.request(SAVE, true);
        check(s.pending() == (VOL | SAVE), "volume and table: both pending");
        uint8_t a = s.takeDue(false);
        uint8_t b = s.takeDue(false);
        uint8_t c = s.takeDue(false);
        check(a == VOL && b == SAVE && c == 0, "afterwards: one write per pass (volume, table, nothing)");
    }
    {
        SaveDeferral s;
        s.request(SAVE, true);
        s.request(CLEAR, true);
        check(s.pending() == CLEAR, "map FROM TO, then map reset: only the table removal remains");
        s.request(SAVE, true);
        check(s.pending() == SAVE, "map reset, then map FROM TO: only the table write remains");
        check(s.takeDue(false) == SAVE && s.takeDue(false) == 0, "one table write done");
    }
    {
        SaveDeferral s;
        s.request(VOL, true);
        s.request(CLEAR, true);
        check(s.request(VOL, false) && s.pending() == CLEAR, "an immediate volume write cancels the pending volume write");
        check(s.request(SAVE, false) && s.pending() == 0, "an immediate table write cancels the pending removal");
        check(s.takeDue(false) == 0, "nothing left to do");
    }
    {
        // Car busy with gaps: the save waits for the first free pass, then does not come back.
        SaveDeferral s;
        s.request(VOL, true);
        int done = 0, firstFree = -1;
        for (int i = 0; i < 100; i++) {
            bool busy = i < 40 || (i >= 50 && i < 60);
            if (s.takeDue(busy) != 0) {
                done++;
                if (firstFree < 0) firstFree = i;
            }
        }
        check(done == 1 && firstFree == 40, "busy for 40 passes: write in the 41st, exactly once");
    }
}

static void testDeadline() {
    std::printf("--- deadline with an active flag ---\n");
    Deadline d;
    check(!d.running(0) && !d.running(0x80000001u) && d.leftMs(0x80000001u) == 0,
          "not set: not running, also when the counter passes half of its range (before: running)");
    d.start(1000, 1500);
    check(d.running(1000) && d.leftMs(1000) == 1500 && d.running(2499) && d.leftMs(2499) == 1, "start: runs 1500 ms");
    check(!d.running(2500) && d.leftMs(2500) == 0, "after 1500 ms: end");
    d.poll(2499);
    check(d.active, "poll before the deadline: the flag stays");
    d.poll(2500);
    check(!d.active, "poll after the deadline: the flag is cleared");
    check(!d.running(2500u + 0x80000000u) && !d.running(2500u + 0x80000001u),
          "24.9 days later: not running (a time comparison alone would say 'running' again)");
    // Refresh on every pass (amplifier hold): lasts as long as it is refreshed.
    Deadline a;
    bool on = true;
    for (uint32_t t = 0; t < 5000; t++) {
        a.start(t, 2000);
        a.poll(t);
        if (!a.running(t)) on = false;
    }
    check(on, "refreshed every 1 ms: runs all the time");
    bool heldFor = true;
    for (uint32_t t = 5000; t < 6999; t++) {
        a.poll(t);
        if (!a.running(t)) heldFor = false;
    }
    a.poll(6999);
    check(heldFor && !a.running(6999) && !a.active, "after the last refresh it holds 2000 ms and ends");
    // Millisecond counter wrapping through zero during the deadline.
    Deadline w;
    w.start(0xFFFFFF00u, 1500);
    w.poll(0xFFFFFFFFu);
    bool mid = w.running(0xFFFFFFFFu) && w.running(0x00000100u) && w.leftMs(0x00000100u) == 988;
    w.poll(0x000004DCu);
    check(mid && !w.running(0x000004DCu) && !w.active, "counter wrapping through zero: 1500 ms counted correctly");
}

static void testDriverInit() {
    std::printf("--- safe state after a driver error ---\n");
    DriverInit ok;
    ok.can = ok.audio = ok.radio = true;
    check(outputsAllowed(true, ok, true) && ampAllowed(ok), "all drivers up: outputs and amplifier allowed");
    check(!outputsAllowed(false, ok, true) && !outputsAllowed(true, ok, false),
          "beside the box or profile without outputs: outputs off");
    const char* names[3] = {"CAN", "I2S (audio)", "UART (radio)"};
    for (int i = 0; i < 3; i++) {
        DriverInit d = ok;
        if (i == 0) d.can = false;
        if (i == 1) d.audio = false;
        if (i == 2) d.radio = false;
        char what[96];
        std::snprintf(what, sizeof(what), "%s failed: outputs off, amplifier asleep, LED red", names[i]);
        // the LED stays red whatever the bus does (sound, frames, silence)
        bool red = statusLedState(d, true, true, true) == 3 && statusLedState(d, true, false, true) == 3 &&
                   statusLedState(d, true, false, false) == 3;
        check(!outputsAllowed(true, d, true) && !ampAllowed(d) && red, what);
    }
    check(statusLedState(ok, false, true, true) == 3, "drivers up, CAN controller not running: red");
    check(statusLedState(ok, true, true, true) == 2 && statusLedState(ok, true, false, true) == 1 &&
              statusLedState(ok, true, false, false) == 0,
          "drivers up: yellow = sound, green = frames, off = silence (as before)");
    DriverInit noCan = ok;
    noCan.can = false;
    DriverInit noAudio = ok;
    noAudio.audio = false;
    check(sleepBusActivity(ok, true, false) && !sleepBusActivity(ok, false, true) && !sleepBusActivity(noAudio, false, true),
          "CAN driver up: bus activity = frames received; the RX pin sample is ignored");
    check(sleepBusActivity(noCan, false, true) && !sleepBusActivity(noCan, false, false),
          "CAN driver failed: a LOW sample of the CAN RX pin (dominant bit) counts as bus activity");

    // CAN driver failed, simulated main loop: one pass per ms, one RX pin sample per pass, sleep after 30 s
    // without activity (SLEEP_AFTER_MS in src/config.h, the same logic as sleepPoll). 120 s of car bus: about one
    // sample in ten LOW (pseudo-random); then silence (pin HIGH).
    const uint32_t SLEEP_AFTER = 30000;
    uint32_t lastActive = 0, sleptAt = 0, seed = 12345;
    bool slept = false;
    for (uint32_t now = 1; now <= 200000 && !slept; now++) {
        bool low = false;
        if (now <= 120000) {
            seed = seed * 1103515245u + 12345u;
            low = ((seed >> 16) % 10) == 0;
        }
        if (sleepBusActivity(noCan, false, low)) lastActive = now;
        else if (now - lastActive >= SLEEP_AFTER) {
            slept = true;
            sleptAt = now;
        }
    }
    char what[96];
    std::snprintf(what, sizeof(what), "CAN driver failed: awake while the bus runs, asleep %lu ms after the last LOW sample",
                  (unsigned long)(sleptAt - lastActive));
    check(slept && lastActive > 119000 && lastActive <= 120000 && sleptAt - lastActive == SLEEP_AFTER, what);
}

int main() {
    testSaveDeferral();
    testDeadline();
    testDriverInit();
    std::printf("loop_policy: %d checks, %d errors\n", checks, failures);
    return failures ? 1 : 0;
}
