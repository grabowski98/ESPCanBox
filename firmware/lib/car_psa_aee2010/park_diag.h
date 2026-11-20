// Overnight evidence about the parking sensors state, without anyone at the console.
//
// The device deep-sleeps when the bus is silent (src/sleep_ctrl.cpp), so RAM and the console buffer are lost
// every night. A small record (ParkDiagRecord) is kept in RTC memory by src/ (RTC_DATA_ATTR: survives deep
// sleep, lost on power loss); this class holds the decisions, without hardware (test: test_host/park_diag.cpp).
//
// What it records:
//   - the last stable parking sensors state from frame 126 (byte 1: 18 = off, 00 = on): two equal 126 frames
//     in a row from the bus, at least vset::IGNITION_SETTLE_MS after the ignition came on;
//   - counters since the record was started: wake-ups from deep sleep, "state differed after a wake-up",
//     1A9 presses and 1A9 releases handed to the CAN controller.
// After a wake-up the first stable state is compared ONCE with the stored one; a difference is counted and
// reported (the caller prints "[park] sensors state changed while asleep: was ON, now OFF").
// Sends nothing.
#pragma once
#include <stdint.h>

#include "car_state.h"

namespace psa {

// Plain data, no constructor: it lives in RTC memory (src/), whose content after power-on is not trusted;
// ParkDiag::begin checks 'magic' and starts a fresh record when it does not match.
struct ParkDiagRecord {
    uint32_t magic;
    uint8_t haveState;      // 1 = 'state' holds a stable state
    uint8_t state;          // 1 = sensors off (126 byte 1 = 18), 0 = on (00)
    uint8_t reserved[2];
    uint32_t wakes;         // starts that were a wake-up from deep sleep
    uint32_t flipsAsleep;   // after a wake-up the state differed from the one stored before sleep
    uint32_t presses;       // 1A9 button presses handed to the controller
    uint32_t releases;      // 1A9 release frames handed to the controller
};

class ParkDiag {
public:
    static const uint32_t MAGIC = 0x50444731UL;   // "PDG1"

    // rec = record in RTC memory (src/); nullptr = an own record in RAM (host tests, golden replays).
    // wokeFromSleep = this start is a wake-up from deep sleep: counted, and the state is compared once.
    void begin(ParkDiagRecord* rec, bool wokeFromSleep);
    // Every frame 126 from the bus (not pasted). Returns true once per wake-up, when the first stable state
    // differs from the stored one; 'was' and 'now' then hold the two states (1 = off, 0 = on).
    bool on126(const CarState& car, uint32_t nowMs, uint8_t& was, uint8_t& now);
    void notePress() { r_->presses++; }
    void noteRelease() { r_->releases++; }
    const ParkDiagRecord& record() const { return *r_; }

private:
    ParkDiagRecord own_ = {};
    ParkDiagRecord* r_ = &own_;
    bool woke_ = false;
    bool compared_ = false;     // the comparison after this wake-up is done (or not needed)
    bool havePrev_ = false;     // previous 126 value counted for stability
    uint8_t prev_ = 0;
};

}  // namespace psa
