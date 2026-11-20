// Session with the radio on the "box" side, common to every radio protocol: what to send to the radio and
// what to do with what the radio sent. No hardware: inputs are events (key, sensors, ignition, frame from the
// radio), output is a queue of frames to send. The UART itself is in src/radio_uart.cpp.
//
// Knows no protocol: every frame is built by the RadioProtocol given to the session (lib/core/radio_protocol.h:
// encodePower, encodeKey, encodeParking, encodeVersion, decodeRadioFrame), and the rate limit of state frames
// comes from the protocol's table (RadioProtocol::rateLimits). A protocol without a frame (nullptr) simply
// gets nothing of that kind. What stays here: the queue, the ACC gate, the state slots (sent on change,
// all again when the radio asks), the repeat of the parking frame and the counters.
//
// lib/radio_xp/radio_link.h: RadioLink = this session with the XP protocol (the name used by the 0.6.0 code
// and the tests).
#pragma once
#include <stdint.h>

#include "radio_protocol.h"
#include "vehicle_signals.h"

class RadioSession {
public:
    explicit RadioSession(const RadioProtocol* protocol = nullptr) : rp_(protocol) {}
    // Protocol of this session (nullptr = none: nothing is ever queued). The Decoder sets it in begin().
    void setProtocol(const RadioProtocol* protocol) { rp_ = protocol; }

    // --- events from the car ---
    void onKey(const RadioKeyEvent& e);
    void onParking(const vehicle::ParkingState& pa, bool valid, uint32_t nowMs);
    void onPower(bool accOn, uint32_t nowMs);
    // State frame (angle, trip computer, temperature, climate...): sent when its content changes,
    // repeated in full when the radio asks for data after startup. One slot per frame number, or per frame
    // number and sub-type bytes when the protocol says so (RadioProtocol::stateKeyBytes).
    void onState(const RadioFrame& f, uint32_t nowMs);
    // Put the state frame with this number into the queue once more, with its current content (e.g. after the
    // radio reports "on"). false = no such state frame yet.
    // Without ACC it sends nothing (like every frame except power) and keeps the frame to be sent later.
    bool resendState(uint8_t cmd);

    // --- events from the radio ---
    void onRadioFrame(const RadioFrame& f, uint32_t nowMs);

    // --- periodic ---
    void update(uint32_t nowMs);

    // Frame to send to the radio; false = queue empty.
    bool popTx(RadioFrame& out);

    const RadioInfo& info() const { return info_; }
    uint32_t txDropped() const { return dropped_; }
    // Supervision counters (counters only, behaviour unchanged):
    //   txGated   = frames rejected because the radio has no ACC (keys, sensors, version; not power);
    //   slotsFull = state frames skipped because there was no room for a new frame kind (STATE_SLOTS).
    uint32_t txGated() const { return gated_; }
    uint32_t slotsFull() const { return slotsFull_; }
    // For the status view (web panel): how many frames are waiting in the radio queue now and how many fit.
    uint8_t txQueued() const { return count_; }
    static uint8_t txCapacity() { return QUEUE; }

private:
    static const uint8_t QUEUE = 24;
    static const uint8_t STATE_SLOTS = 12;
    static const uint8_t STATE_KEY_MAX = 2;     // longest sub-type key of a state frame (RadioProtocol::stateKeyBytes)

    struct StateSlot {
        bool used = false;
        bool dirty = false;        // content changed, waiting to be sent
        uint32_t lastTxMs = 0;
        RadioFrame frame;
    };
    StateSlot states_[STATE_SLOTS];

    const RadioProtocol* rp_ = nullptr;
    RadioFrame queue_[QUEUE];
    uint8_t head_ = 0, count_ = 0;
    uint32_t dropped_ = 0;
    uint32_t gated_ = 0, slotsFull_ = 0;

    RadioInfo info_;
    RadioFrame lastParking_;
    bool haveParking_ = false;
    bool parkingActive_ = false;
    uint32_t lastParkingTxMs_ = 0;
    bool accOn_ = false;

    bool sameSlot(const RadioFrame& a, const RadioFrame& b) const;
    // alwaysSend = true only for the power frame: it passes the ACC gate, and with a full queue the oldest
    // waiting frame is dropped to make room for it.
    void push(const RadioFrame& f, bool alwaysSend = false);
    void pushVersion();
    void flushStates(uint32_t nowMs, bool resendAll);
    uint16_t minInterval(uint8_t cmd) const;     // rate limit of a state frame from the protocol table, 0 = none
};
