// Car buttons (CAN frames) -> key codes for the radio (command 20 of the XP protocol).
//
// The table is composed of two parts:
//   - the car profile says which CAN bits are which logical button (CarProfile::keys, CarProfile::knobs;
//     lib/car_psa_aee2010/profile_psa_aee2010.cpp for the reference car);
//   - the radio protocol says which code a logical button has (RadioProtocol::keyCode; xp::keyCode in
//     lib/radio_xp/protocol_xp.cpp).
// Default (constructor): PSA AEE2010 + XP, i.e. exactly the former table LEVEL_KEYS (formerly in
// lib/radio_xp/xp_keys.cpp); test_host/key_tables.cpp checks the composition row by row. The class moved to
// lib/app because it joins a car profile and a radio protocol.
//
// The table comes from the measurement of 2026-11-11: CAN and the box -> radio line recorded on one clock
// (test_data/captures/can_20261111_190817.log, can_20261111_191045.log; radios/xp-simple-soft/protocol.md).
// Timing behaviour copied from the Simple Soft box:
//   - press: immediately (code, 01);
//   - every 200 ms from the press: button still held -> (code, 02), released -> (code, 00).
//   This gives a shortest "press" of 200 ms, as in the box.
//   - rotary knobs (counters): one detent = one (code, 01), without release;
//   - a held key whose CAN frame has not come for STALE_MS counts as released (review: when reception stopped
//     with the bit set, "held" went to the radio every 200 ms until ACC went off, e.g. the volume kept rising).
#pragma once
#include "can_frame.h"
#include "car_profile.h"
#include "profile_psa_aee2010.h"
#include "protocol_xp.h"

namespace xp {

class KeyEncoder {
public:
    // Key code of a logical key (RadioProtocol::keyCode); 0 = not sent.
    typedef uint8_t (*CodeFn)(vehicle::KeyId key);

    // Car profile and key code table. car = nullptr or codeOf = nullptr: no key is ever reported.
    // Call before the first frame (Decoder::begin); the default is PSA AEE2010 + XP.
    void configure(const vehicle::CarProfile* car, CodeFn codeOf);

    // Pass every received CAN frame; frames of other types are ignored.
    void onCanFrame(const CanFrame& f, uint32_t nowMs);
    // Call often (every few ms): generates "held" and "released".
    void update(uint32_t nowMs);
    // Returns true and the event if one is waiting in the queue.
    bool pop(KeyEvent& out);

    uint32_t dropped() const { return dropped_; }   // events lost because the queue was full

private:
    static const uint8_t MAX_KEYS = 32;
    static const uint8_t MAX_KNOBS = 4;
    static const uint8_t QUEUE = 16;
    static const uint16_t TICK_MS = 200;
public:
    // A held key without a fresh CAN frame for this long is released. PSA AEE2010, measured in the captures of
    // test_data/captures/ (median gap between frames): 21F, 0A2, 036 every 100 ms, 122 every 200 ms, 221 (TRIP key)
    // every 1000 ms. 2500 ms = more than two periods of the slowest key frame, and longer than the 500 ms loop
    // stall of the golden test.
    static const uint16_t STALE_MS = 2500;
private:

    const vehicle::CarProfile* car_ = &kCarPsaAee2010;
    CodeFn codeOf_ = &keyCode;

    struct Slot {
        bool down = false;        // state reported to the radio
        bool canDown = false;     // state from the last CAN frame
        uint32_t nextTickMs = 0;
        uint32_t lastFrameMs = 0; // time of the last CAN frame of this key (freshness of canDown)
    };
    Slot slots_[MAX_KEYS];

    // knob counters (one per CarProfile::knobs row): previous value, -1 = not known yet
    // (PSA AEE2010: row 0 = 21F byte 2, row 1 = 122 byte 7)
    int16_t knobPrev_[MAX_KNOBS] = {-1, -1, -1, -1};

    KeyEvent queue_[QUEUE];
    uint8_t head_ = 0, count_ = 0;
    uint32_t dropped_ = 0;

    uint8_t keyCount() const;
    uint8_t codeAt(uint8_t index) const;   // code of key row 'index'; 0 = not sent
    void push(uint8_t code, uint8_t state);
    void setLevel(uint8_t index, bool down, uint32_t nowMs);
};

}  // namespace xp
