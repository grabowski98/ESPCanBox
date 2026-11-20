// Parking sensor beeping logic: turns the decoded parking sensor state into "play / do not play".
// No hardware and no car frames: the inputs are vehicle::ParkingState (CarProfile::decodeParking;
// PSA AEE2010: frame 0E1) and the time in milliseconds.
//
// The rhythm depends on the nearest zone (design decision 15.11.2026):
//   - beeping starts from the first detected zone, as in the Raise box,
//     not only when the car sets the sound request bit;
//   - each zone has its own, clearly different tempo (table periodMs);
//   - the continuous tone plays only when the car itself asks for it (interval 0x00).
//
// Protection against a "stuck tone": sound plays only with fresh frames.
// When frames stop arriving (ignition switched off, bus asleep),
// silence follows after frameTimeoutMs.
#pragma once
#include "vehicle_signals.h"   // vehicle::ParkingState

struct BeeperConfig {
    uint16_t beepMs = 45;           // with the decay this gives about 50 ms, as in the car
    uint16_t frameTimeoutMs = 400;  // PSA AEE2010: frame 0E1 arrives every 100 ms; 4 lost = silence
    uint16_t freqFrontHz = 498;     // tone of the car's buzzer (measured from a recording 16.11.2026)
    uint16_t freqRearHz = 498;      // the car plays the same tone for front and rear
    // Time from the start of one beep to the start of the next, for zones 0..6
    // (0 = nearest, 6 = first detected). Each step is about 1.45 times faster.
    // Zone 0 beeps only at transitional moments; normally the car then asks for the continuous tone.
    uint16_t periodMs[7] = {110, 150, 220, 320, 460, 670, 970};
    // true: we beep from the first zone. false: only when the car asks for sound (factory behaviour).
    bool beepWithoutCarRequest = true;
};

struct BeeperOutput {
    bool toneOn = false;      // the tone should be playing right now
    bool active = false;      // beeping in progress (also in the pause between beeps)
    bool continuous = false;  // continuous tone
    uint16_t freqHz = 0;
    uint8_t zone = 7;         // zone the tempo was taken from (7 = none)
};

class ParkingBeeper {
public:
    explicit ParkingBeeper(const BeeperConfig& cfg = BeeperConfig()) : cfg_(cfg) {}

    void onFrame(const vehicle::ParkingState& pa, uint32_t nowMs);
    BeeperOutput update(uint32_t nowMs);   // call often (every few ms)

    bool frameFresh(uint32_t nowMs) const;
    const vehicle::ParkingState& last() const { return last_; }

private:
    BeeperConfig cfg_;
    vehicle::ParkingState last_;
    bool haveFrame_ = false;
    uint32_t lastFrameMs_ = 0;
    bool wasActive_ = false;
    uint32_t cycleStartMs_ = 0;   // start of the current beep
};
