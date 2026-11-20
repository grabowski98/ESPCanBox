// Car state collected from CAN frames.
// CarModel only ADDS state: it remembers the last content of selected frames and the time they were received.
// It sends nothing and does not replace the decoders (psa_signals.h for the radio data, PowerPolicy,
// ParkingBeeper): they still receive every frame separately in Decoder::onCanFrame.
// No hardware and no Arduino framework header; test: test_host/car_state.cpp.
#pragma once
#include <stdint.h>

#include "can_frame.h"
#include "can_ids.h"
#include "psa_frames.h"

// Last content of one CAN frame.
struct RawFrame {
    uint8_t d[8] = {0};   // bytes beyond len are zeroed
    uint8_t len = 0;      // length of the last frame (0..8): the reader checks it before using a byte
    bool seen = false;    // the frame arrived at least once
    uint32_t ms = 0;      // time of the last reception
    // true = the last frame was pasted from the web panel, not received from the bus.
    // Such a frame must not be used to build 15B or to evaluate transmit conditions (review 18.11.2026).
    bool pasted = false;
};

struct CarState {
    // Bus traffic: only frames from the bus or pasted over USB (fromBus = true).
    bool haveAnyFrame = false;
    uint32_t msAnyFrame = 0;
    // 0F6: ignition and reverse in 'body'; outside temperature = f0F6.d[6] when f0F6.len >= 7.
    // Only a frame accepted by psa::decodeBodyState (8 bytes) is stored; a shorter one leaves the state untouched.
    RawFrame f0F6;
    psa::BodyState body;
    RawFrame f036;                           // economy mode, lights
    RawFrame f260, f361, f227, f3E1, f217;   // personalization and its surroundings (read for frame 38)
    // There was a door frame from which psa::decodeDoors built the door state (128 with the lamp off
    // or 1A1 with a door message while the lamp is lit).
    bool haveDoors = false;
};

class CarModel {
public:
    // Copies min(len, 8) bytes of the frame into the proper field. fromBus = false (frame pasted
    // from the web panel) does not count as bus traffic. Sends nothing.
    void onCanFrame(const CanFrame& f, uint32_t nowMs, bool fromBus);
    // Only the bus traffic part of onCanFrame (haveAnyFrame, msAnyFrame). Used by the Decoder for a car
    // profile other than PSA AEE2010: bus activity counts, the frame content is not stored.
    void onBusTraffic(uint32_t nowMs, bool fromBus);
    const CarState& state() const { return s_; }

private:
    CarState s_;
    bool doorLampOn_ = false;   // door-open lamp (128 byte 5 bit 6), as in psa::decodeDoors
    static void store(RawFrame& r, const CanFrame& f, uint32_t nowMs, bool fromBus);
};

// The frame was received and is younger than limitMs. Unsigned subtraction: works also when the
// millisecond counter wraps through zero (after 49.7 days).
inline bool fresh(const RawFrame& r, uint32_t nowMs, uint32_t limitMs) {
    return r.seen && (uint32_t)(nowMs - r.ms) < limitMs;
}
