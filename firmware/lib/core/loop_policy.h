// Small main-loop decisions, no hardware:
//   - SaveDeferral: when a setting may be written to flash memory (NVS);
//   - Deadline: a deadline with an "active" flag (test beep, keeping the amplifier awake).
// Test on a PC: test_host/loop_policy.cpp.
#pragma once
#include <stdint.h>

// A write to flash memory stops CAN reception for a few ms (time not measured). When the car needs
// the unit (reverse or active parking sensors: Decoder::carBusy), a write from the USB console is
// deferred and executed when that state ends. The class remembers only the KIND of write; the value (volume,
// button table) is kept and written by src/console_cmds.cpp.
class SaveDeferral {
public:
    enum : uint8_t {
        VOLUME = 0x01,         // volume (command "vol")
        KEYMAP_SAVE = 0x02,    // button swap table in its current form (command "map Z NA")
        KEYMAP_CLEAR = 0x04,   // removal of the table from the settings (command "map reset")
    };

    // Request to write one kind. true = write now; false = deferred (carBusy).
    // A new request replaces a pending one of the same kind; KEYMAP_SAVE and KEYMAP_CLEAR replace each
    // other (the last command counts). A write executed immediately also cancels the pending one.
    bool request(uint8_t what, bool carBusy);

    // Once per loop pass. Returns ONE kind of write to execute now (and stops remembering it)
    // or 0: nothing pending or the car still needs the unit. One per pass, so that two writes
    // do not stop CAN reception one after the other.
    uint8_t takeDue(bool carBusy);

    uint8_t pending() const { return pending_; }       // mask of pending writes
    uint32_t deferred() const { return deferred_; }    // how many requests were deferred since startup

private:
    uint8_t pending_ = 0;
    uint32_t deferred_ = 0;
};

// An "until when" deadline counted with the millisecond counter, which wraps every 49.7 days. The bare comparison
// (int32_t)(untilMs - now) > 0 gives "running" again about 24.9 days after the deadline was set
// (found in a code review). That is why the deadline has an "active" flag: after the deadline passes poll()
// clears it and the deadline does not come back until someone calls start().
struct Deadline {
    bool active = false;
    uint32_t untilMs = 0;

    void start(uint32_t nowMs, uint32_t durationMs) {
        active = true;
        untilMs = nowMs + durationMs;
    }
    // Call on every loop pass: clears the flag after the deadline passes.
    void poll(uint32_t nowMs) {
        if (active && (int32_t)(untilMs - nowMs) <= 0) active = false;
    }
    bool running(uint32_t nowMs) const { return active && (int32_t)(untilMs - nowMs) > 0; }
    // How much time is left (0 when the deadline is not running).
    uint32_t leftMs(uint32_t nowMs) const { return running(nowMs) ? (uint32_t)(untilMs - nowMs) : 0; }
};

// Driver start results from setup() and what they allow the main loop to switch on. CLAUDE.md: after a driver
// error the device stays in the safe state (12 V outputs off, amplifier asleep, nothing sent to the radio).
// Found in a code review: before, the outputs followed only the CAN result and the amplifier none, and the red
// LED from setup() was overwritten on the first loop pass.
struct DriverInit {
    bool can = false, audio = false, radio = false;
    bool allOk() const { return can && audio && radio; }
};

// 12 V radio outputs (ACC, lights, reverse): decoder mode, every driver up, and the car profile allows them.
inline bool outputsAllowed(bool decoderMode, const DriverInit& d, bool profileAllows) {
    return decoderMode && d.allOk() && profileAllows;
}

// Amplifier (pin SD): may be woken only when every driver is up.
inline bool ampAllowed(const DriverInit& d) { return d.allOk(); }

// Status LED: 0 off (silence on the bus), 1 green (frames), 2 yellow (sound), 3 red (a driver failed at
// startup, or the CAN controller is not running). Red has priority over everything else.
inline uint8_t statusLedState(const DriverInit& d, bool canRunning, bool sound, bool busAlive) {
    if (!d.allOk() || !canRunning) return 3;
    return sound ? 2 : (busAlive ? 1 : 0);
}
