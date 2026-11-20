// Radio protocol XP (Simple Soft box <-> FYT radio) as a RadioProtocol:
// UART parameters, framing (2E CMD LEN DATA CHK, xp_protocol.h), the parser of the radio frames and the
// acknowledgement F3, key events and the logical key -> XP key code table (command 20), the frames of the
// common session (EA power, 20 key, 32 parking, 3E obstacle, 71 version), the radio frames (A6 time, C4 volume,
// C0 source, 81 state, 80 / 8A setting and climate, 90 request) and the protocol tables (latch key A4 moon,
// rate limit of 29 steering angle).
// Knows no CAN identifiers: which CAN bits are which button is in the car profile (CarProfile::keys / knobs).
#pragma once
#include <stdint.h>

#include "radio_protocol.h"
#include "vehicle_signals.h"

namespace xp {

// One key event for the radio (command 20 of the XP protocol): the neutral RadioKeyEvent
// (lib/core/radio_protocol.h; state 1 pressed, 2 held, 0 released).
typedef RadioKeyEvent KeyEvent;

const uint8_t KEY_STATE_RELEASED = 0;
const uint8_t KEY_STATE_PRESSED = 1;
const uint8_t KEY_STATE_HELD = 2;

// XP key code of a logical key; 0 = the key has no XP code (not sent).
// The codes come from the measurement of 2026-11-11 (CAN and the box -> radio line on one clock;
// radios/xp-simple-soft/protocol.md). Earlier they were the code column of LEVEL_KEYS in xp_keys.cpp.
uint8_t keyCode(vehicle::KeyId key);

// Frames of the common session (lib/core/radio_session.h) in XP form.
bool encodePower(bool accOn, RadioFrame& out);                                        // EA 01|00 0A
bool encodeKey(const RadioKeyEvent& e, RadioFrame& out);                              // 20 code state
bool encodeParkingFrame(const vehicle::ParkingState& p, bool valid, RadioFrame& out); // 32 (xp_parking.h)
bool encodeObstacle(bool obstacle, RadioFrame& out);                                  // 3E 80|00
bool encodeVersion(RadioFrame& out);                                                  // 71 (box V2.29.101)
// Radio frames A6, 81, C4, C0, 90, 80, 8A ... -> RadioInfo; 90 71 -> RADIO_RX_SEND_ALL.
uint8_t decodeRadioFrame(const RadioFrame& f, RadioInfo& info);
// 80 / 8A / 84 / 90 / 81 -> RadioCmd (parseRadioCmd, radio_commands.h).
bool decodeCommand(const RadioFrame& f, ::RadioCmd& out);

}  // namespace xp

extern const RadioProtocol kRadioXp;
