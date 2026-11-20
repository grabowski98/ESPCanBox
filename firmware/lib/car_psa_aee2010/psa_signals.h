// Car data for the radio screen from the Peugeot 508 INFO-DIV bus (PSA AEE2010): CAN frames -> the
// car-neutral types of lib/core/vehicle_signals.h. These are the CarProfile::decode... functions of the
// reference profile (profile_psa_aee2010.cpp). No radio protocol here: the XP frames are built from the
// neutral types in lib/radio_xp/xp_signals.h.
//
// The rules were moved here unchanged from the PSA -> XP glue (lib/app/xp_car.cpp, vset::update38,
// vset::features361); the sources are next to each function. Every function checks the identifier and
// the length first; a frame longer than 8 bytes is read as 8 bytes.
#pragma once
#include "can_frame.h"
#include "vehicle_signals.h"

namespace psa {

// Steering angle: frame 10B bytes 1-2 (the box copied them to the radio in 155 of 155 frames; scale
// not determined).
bool decodeSteering(const CanFrame& f, vehicle::SteeringAngle& out);

// Trip computer (copied byte for byte by the box; meaning unverified):
//   221 (current page, at least 7 bytes): now[] = bytes 2-3, 4-5, 6-7;
//   2A1 (trip 1) and 261 (trip 2), at least 5 bytes: a = byte 1, b = bytes 2-3, c = bytes 4-5.
// Returns the page bit (vehicle::TRIP_*) or 0 when the frame is not a trip frame (or too short).
uint8_t decodeTrip(const CanFrame& f, vehicle::TripData& io);

// Outside temperature: frame 0F6 byte 7, degrees = byte / 2 - 40 (measured: the box sent 06 for 5C and
// 07 for 5D, i.e. half degrees rounded up).
bool decodeOutsideTemp(const CanFrame& f, vehicle::OutsideTemp& out);

// Economy mode: frame 036 byte 3 bit 7 (measured: 1 with ignition only, 0 after engine start).
bool decodeEcoMode(const CanFrame& f, vehicle::EcoMode& out);

// Climate control: frame 350 (at least 7 bytes) and the rear window heating from frame 2AD byte 1 bit 5.
// 'io' is the state kept by the Decoder: 2AD changes only rearDefrost. Returns true when the frame was
// one of these two and the state is valid (frame 350 has arrived at least once).
//   350 byte 1 bit 3 = 1: A/C off; bit 4: front demist (one observation);
//   bytes 4, 5: temperature left / right, value + 0x23 = half degrees (0A = 22.5 C); 0 stays 0;
//   byte 6: low nibble fan level (0 = automatic), high nibble 3 = recirculation;
//   byte 7: air distribution left (high nibble) / right (low nibble): 8 = windscreen + face + feet,
//           5 = face + feet, 3 = face, 0 = automatic; other codes (4, 6) not determined: no flag.
bool decodeClimate(const CanFrame& f, vehicle::ClimateState& io);

// Doors. Measured 2026-11-10 (test_data/captures/can_20261110_190421.log):
//   which doors: car message in frame 1A1 (byte 2 = DE), byte 4 = the vehicle::DOOR_* bits (the neutral
//   values were chosen equal to this byte); the message disappears after about 10 s while the doors are
//   still open, so the last indication is kept;
//   any door open: warning lamp, frame 128 byte 5 bit 6; when it goes out, every door is closed.
// The message stays on the bus for about 0.2 s after the lamp goes out: it is ignored while the lamp is off.
// Known limitation: when one of several open doors is closed and the car shows no new message, the old
// indication stays until all are closed. Returns true when the door state was (re)stated.
bool decodeDoors(const CanFrame& f, vehicle::DoorState& io);

// Vehicle settings (vehicles/peugeot-508-sw-2015/settings-map.md, section "XP frame `38`"):
//   260 (BSI personalisation, 7 bytes), 227 (button LEDs, at least 3), 3E1 (Stop and Start, at least 4),
//   217 (at least 8) -> item values; 361 (at least 4 bytes) -> which items the car has.
// 'io' is the state kept by the Decoder. A value is 'known' when the car's latest frame of its source
// states it; "no change" rules of the source (language outside the table, sound without its validity bit,
// fuel unit combinations, Stop and Start bits 00 / 11, a frame too short) leave it unknown. The settings
// become valid with the first full frame 260 (before it the box sent zeros although 227, 3E1 and 217 had
// arrived: test_data/captures/can_20261111_190551.log).
// Returns vehicle::SETTINGS_VALUES for 260 / 227 / 3E1 / 217 (any length), vehicle::SETTINGS_PRESENT for a
// 361 with at least 4 bytes, 0 otherwise.
uint8_t decodeSettings(const CanFrame& f, vehicle::VehicleSettings& io);

}  // namespace psa
