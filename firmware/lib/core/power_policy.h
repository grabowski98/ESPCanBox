// When to give the radio ACC and the reverse gear signal (12 V outputs).
// No hardware and no car frames: the input is the neutral body state decoded by the car profile
// (vehicle::BodyState, CarProfile::decodeBody), the lights state (CarProfile::decodeLights) and the time;
// the output is logic states. "Body frame" below = the frame the profile decodes the ignition from.
//
// Reference profile PSA AEE2010 (body frame 0F6, lights frame 036), measured 11.11.2026 (CAN capture
// together with the box -> radio UART line); the timings below come from that car:
//   - ignition on: 0F6 byte 1 changes from 80 to 88; the box reports "power
//     on" after about 30 ms, the radio responds after about 0.9 s;
//   - cranking: byte 1 goes through 90, 91, 89, 8A to 8E; the radio does not switch off meanwhile;
//   - switch-off: 86, 84, 80; the box reports "power off" 2.5 s after the value 80.
// Hence the rule: ignition = any state except Ignition::OFF (vehicle_signals.h); ACC goes off 2.5 s after
// the ignition is switched off or after frames disappear.
//
// Illumination (ILL): byte 4 of frame 036, bit 5 = lights on (measured 10.11.2026,
// steps 29-30 of the capture). We give it to the radio only with the ignition on. How the box does it
// was not measured (the ILL wire was not recorded).
#pragma once
#include "vehicle_signals.h"   // vehicle::BodyState (neutral ignition state)

struct PowerConfig {
    uint16_t accOffDelayMs = 2500;    // ACC switch-off delay after the ignition is switched off
    uint16_t frameTimeoutMs = 1000;   // this long without a body frame = the bus is silent
    // Limit in the lost-frame tolerant mode (setLossTolerant). Frame 0F6 arrives every 500 ms
    // (capture test_data/captures/can_20261111_191045.log); during a write to flash memory some frames are lost
    // in the CAN controller, and 1000 ms is a margin of only one lost frame.
    uint16_t lossTolerantTimeoutMs = 5000;
};

struct PowerOutputs {
    bool ignition = false;   // ignition on (from the frame)
    bool acc = false;        // give ACC to the radio
    bool reverse = false;    // give the reverse gear signal to the radio
    bool illumination = false;   // give the radio the lights-on signal
};

class PowerPolicy {
public:
    explicit PowerPolicy(const PowerConfig& cfg = PowerConfig()) : cfg_(cfg) {}

    void onBodyState(const vehicle::BodyState& b, uint32_t nowMs);
    void onLights(bool lightsOn, uint32_t nowMs);
    PowerOutputs update(uint32_t nowMs);

    // Lost-frame tolerant mode: for the duration of a program update over Wi-Fi (and a moment after it)
    // missing 0F6 and 036 frames are judged with the lossTolerantTimeoutMs limit instead of frameTimeoutMs, so
    // lost frames do not switch off the ignition, ACC, illumination or reverse. A frame with byte 1 = 80
    // (ignition off) works normally. Call before update(); off by default. In the program
    // without networking nobody enables it, so the behaviour is as before.
    void setLossTolerant(bool on) { lossTolerant_ = on; }

private:
    PowerConfig cfg_;
    vehicle::BodyState last_;
    bool haveFrame_ = false;
    uint32_t lastFrameMs_ = 0;
    bool acc_ = false;
    bool offPending_ = false;
    uint32_t offAtMs_ = 0;
    bool lightsOn_ = false;
    bool haveLights_ = false;
    uint32_t lastLightsMs_ = 0;
    bool lossTolerant_ = false;
};
