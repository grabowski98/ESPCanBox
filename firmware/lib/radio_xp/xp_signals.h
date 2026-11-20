// Car data for the radio screen -> XP frames: the RadioProtocol::encode... functions of the XP protocol
// (protocol_xp.cpp). Input: the car-neutral types of lib/core/vehicle_signals.h; no CAN frame and no
// CAN identifier here (the CAN side is in the car profile, e.g. lib/car_psa_aee2010/psa_signals.h).
//
// Every layout comes from matching a CAN recording against the box -> radio line (2026-11-11;
// radios/xp-simple-soft/protocol.md); the rules were moved here unchanged from lib/app/xp_car.cpp and
// vset::update38 / vset::features361. Each function returns true (it always has a frame to send).
#pragma once
#include "radio_protocol.h"
#include "vehicle_signals.h"

namespace xp {

// 29: DATA[0] = low byte, DATA[1] = high byte of the angle (the box: CAN 10B byte 2, byte 1).
bool encodeSteering(const vehicle::SteeringAngle& s, RadioFrame& out);

// 33 (current page): DATA[0..5] = the three values, high byte first; DATA[6..8] = 0 (source of these
//     three bytes unknown, always zero in the recording);
// 34 / 35 (trip 1 / 2): DATA = c (high, low), 00, a, b (high, low).
bool encodeTrip(const vehicle::TripData& t, uint8_t page, RadioFrame& out);

// 36: whole degrees, half degrees rounded up (measured: 6.0 C -> 06, 6.5 C -> 07). Negative values:
// bit 7 = minus (that is how the radio reads them; no negative case in the recording).
bool encodeOutsideTemp(const vehicle::OutsideTemp& t, RadioFrame& out);

// 44: 01 = economy mode (after 01 the radio counts down 6 s and enters power-saving mode), 00 = not.
bool encodeEcoMode(const vehicle::EcoMode& e, RadioFrame& out);

// 21 (climate), PARTIAL mapping from 18 pairs in the recording (26 of 27 frames match; the only mismatch
// is the moment of a mode switch):
//   DATA[0]: bits 7 and 2 = 1 (constant in the recording), bit 6 A/C, bit 5 recirculation,
//            bit 3 AUTO (left distribution automatic and fan 0), bit 0 rear window heating;
//   DATA[1]: bits 7-5 left distribution (windscreen, face, feet), bits 3-0 fan level;
//   DATA[2], DATA[3]: left / right temperature in half degrees (0 stays 0);
//   DATA[4]: bit 7 front demist (one pair in the recording);
//   DATA[5] = 81 (right after startup the box sends FB; the radio does not read this byte), DATA[6] = 40,
//   DATA[7] = 00, DATA[8] = right distribution as in DATA[1] bits 7-5.
bool encodeClimate(const vehicle::ClimateState& c, RadioFrame& out);

// 38 (6 bytes): doors and vehicle setting states.
//   DATA[0]: doors, bits 7 and 6 front, 5 and 4 rear, 3 boot; bit 2 = 1 (set with ignition on in the
//            recording). sideSwap = true (default, confirmed in the car 2026-11-16): driver -> bit 7,
//            passenger -> bit 6, rear left -> bit 5, rear right -> bit 4; false = the mirrored mapping.
//            No bonnet bit.
//   DATA[1..5]: setting values (settings-map.md, section "XP frame `38`"), written only when the settings
//            are valid and only for 'known' items; every other bit keeps its content from 'io'.
//            Before the first valid settings 'io' stays all zeros, like the box before the first frame 260.
bool encodeCarState(const vehicle::DoorState& d, const vehicle::VehicleSettings& s, bool sideSwap, RadioFrame& io);

// 3C (2 bytes): which rows of the settings screen the radio shows (bit = 0: the row is hidden).
//   DATA[0]: bit 7 UNASSIGNED_1 (the radio does not read it), 6 daytime lights, 5 ambient, 4 welcome
//            lighting, 3 "follow me home", 2 rear wiper in reverse, 1 UNASSIGNED_2, 0 welcome function;
//   DATA[1]: bit 7 door unlocking, bit 6 UNASSIGNED_3.
// Bit assignment: firmware analysis / needs owner decision (one value recorded: 9E 80).
bool encodeSettingsPresent(const vehicle::VehicleSettings& s, RadioFrame& out);

}  // namespace xp
