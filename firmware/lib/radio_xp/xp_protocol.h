// Protocol between a Simple Soft ("XP") type CAN box and the FYT radio.
// Description and sources: radios/xp-simple-soft/protocol.md (radio code + measurement of 2026-11-11).
//
// Transport layer: UART 38400 8N1, 3.3 V levels.
// Frame:  2E  CMD  LEN  DATA[LEN]  CHK      CHK = ~(CMD + LEN + sum of DATA) & 0xFF
// After every valid frame the receiver sends back one acknowledgement byte
// (radio: FF, box: F3). Commands 01..7F are sent by the box, 80..FF by the radio.
//
// This file does not depend on hardware: it also compiles on the host (test_host).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "radio_protocol.h"   // RadioFrame (xp::Frame)

namespace xp {

const uint8_t HEADER = 0x2E;
const uint8_t ACK_FROM_RADIO = 0xFF;   // measurement: the radio acknowledges box frames with byte FF
const uint8_t ACK_FROM_BOX = 0xF3;     // measurement: the box acknowledges radio frames with byte F3
const uint8_t MAX_DATA = 40;           // the longest known frame is CB: 33 data bytes
const uint32_t UART_BAUD = 38400;

// Box -> radio
enum BoxCmd : uint8_t {
    BOX_KEY = 0x20,            // key code, state (1 pressed, 2 held, 0 released)
    BOX_CLIMATE = 0x21,
    BOX_STEERING_ANGLE = 0x29,
    BOX_PARKING = 0x32,        // state + 6 zones
    BOX_TRIP_NOW = 0x33,       // trip computer, current page
    BOX_TRIP_1 = 0x34,         // trip 1
    BOX_TRIP_2 = 0x35,         // trip 2
    BOX_OUTSIDE_TEMP = 0x36,
    BOX_CAR_STATE = 0x38,      // doors and vehicle setting states
    BOX_FEATURES = 0x3C,       // which rows of the vehicle settings screen the radio shows
    BOX_OBSTACLE = 0x3E,       // 80 = parking sensors see an obstacle
    BOX_ECO_MODE = 0x44,
    BOX_VERSION = 0x71,
    BOX_POWER = 0xEA,          // 01 0A after ignition on, 00 0A after ignition off (measurement)
};

// Radio -> box
enum RadioCmd : uint8_t {
    RADIO_SETTING = 0x80,      // vehicle setting: number, value
    RADIO_STATE = 0x81,        // 01 radio on, 00 radio shutting down
    RADIO_AIR = 0x8A,          // climate command from the screen: number, value
    RADIO_REQUEST = 0x90,      // frame request: DATA[0] = box command number
    RADIO_TIME = 0xA6,         // year-2000, month, day, hour, minute
    RADIO_SOURCE = 0xC0,
    RADIO_TUNER = 0xC2,
    RADIO_VOLUME = 0xC4,       // 0..36, 0x80 = mute
    RADIO_VARIANT = 0xCA,
    RADIO_TEXT = 0xCB,
};

// One XP frame: the neutral radio frame of lib/core/radio_protocol.h (command, length, data).
typedef RadioFrame Frame;
static_assert(MAX_DATA == RADIO_FRAME_MAX_DATA, "xp::Frame holds MAX_DATA data bytes");

uint8_t checksum(uint8_t cmd, uint8_t len, const uint8_t* data);

// Writes the frame to the buffer. Returns the number of bytes, or 0 when the buffer is too small
// or the data length exceeds MAX_DATA.
size_t encode(const Frame& f, uint8_t* out, size_t capacity);

// Helper: frame from a list of bytes.
Frame makeFrame(uint8_t cmd, const uint8_t* data, uint8_t len);

// Parser counters: the neutral RadioRxStats of lib/core/radio_protocol.h (badLength = LEN greater than MAX_DATA).
typedef RadioRxStats ParserStats;

// Byte-stream parser: state machine with checksum and frame timeout.
// Robust to garbage on the line (e.g. the box start-up text, noise with the car switched off).
// It is the RadioRxParser of the XP protocol (RadioProtocol::rxParser); Result (NONE, FRAME, ACK) comes from there.
class Parser : public RadioRxParser {
public:
    // frameTimeoutMs: the longest gap between bytes of one frame.
    // At 38400 bit/s a byte takes 0.26 ms, so 30 ms is a large margin.
    explicit Parser(uint16_t frameTimeoutMs = 30) : timeoutMs_(frameTimeoutMs) {}

    Result feed(uint8_t byte, uint32_t nowMs, Frame& out) override;
    void reset() override;
    const ParserStats& stats() const override { return stats_; }

private:
    enum State : uint8_t { WAIT_HEADER, WAIT_CMD, WAIT_LEN, WAIT_DATA, WAIT_CHECKSUM };
    State state_ = WAIT_HEADER;
    Frame cur_;
    uint8_t got_ = 0;
    uint32_t lastByteMs_ = 0;
    uint16_t timeoutMs_;
    ParserStats stats_;
};

}  // namespace xp
