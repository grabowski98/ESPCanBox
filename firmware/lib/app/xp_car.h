// Remaining car data: CAN frames -> frames for the radio (XP protocol), for one PSA AEE2010 frame at a time.
// Since the car-neutral interface (stage F4) these are thin wrappers: the CAN side is
// lib/car_psa_aee2010/psa_signals.h (psa::decode...), the XP side lib/radio_xp/xp_signals.h (xp::encode...).
// The Decoder uses those two through CarProfile / RadioProtocol; the wrappers stay for the frozen 0.6.0
// reference (test_host/ref_glue_060.h) and the host tests, so that both paths are checked against the same
// recordings. Every conversion comes from matching a CAN recording against the box -> radio line
// (2026-11-11; radios/xp-simple-soft/protocol.md).
// The functions return false when the given CAN frame is not the one they need.
#pragma once
#include "can_frame.h"
#include "can_ids.h"
#include "vehicle_signals.h"
#include "xp_protocol.h"

namespace xp {

// Box command numbers BOX_TRIP_NOW (33), BOX_TRIP_1 (34), BOX_TRIP_2 (35) and BOX_OBSTACLE (3E): xp_protocol.h.

// --- CAN frame identifiers: aliases of constants from can_ids.h (psa::ID_*) ---
const uint32_t CAN_STEERING = psa::ID_STEERING;   // 10B, steering angle
const uint32_t CAN_TRIP_NOW = psa::ID_TRIP_NOW;   // 221
const uint32_t CAN_TRIP_1 = psa::ID_TRIP_1;       // 2A1
const uint32_t CAN_TRIP_2 = psa::ID_TRIP_2;       // 261
const uint32_t CAN_CLIMATE = psa::ID_CLIMATE;     // 350
const uint32_t CAN_LIGHTING = psa::ID_LIGHTING;   // 036
const uint32_t CAN_BODY = psa::ID_BODY_STATE;     // 0F6

// Steering angle: frame 29 = bytes 2 and 1 of frame 10B (matching in 155 of 155 frames).
bool buildSteering(const CanFrame& f, Frame& out);

// Trip computer (matching in all frames of the recording):
//   33: DATA[0..5] = bytes 2..7 of frame 221; DATA[6..8] = 0 (source of these three bytes unknown,
//       always zero in the recording);
//   34: DATA = bytes 4,5, 00, byte 1, bytes 2,3 of frame 2A1;
//   35: the same from frame 261.
bool buildTrip(const CanFrame& f, Frame& out);

// Outside temperature: frame 36 from byte 7 of frame 0F6.
// Measurement: 5C -> 06, 5D -> 07, i.e. degrees = byte/2 - 40, halves rounded up.
// Negative values: bit 7 = minus (that is how the radio reads them); no negative case in the recording.
bool buildOutsideTemp(const CanFrame& f, Frame& out);

// Car economy mode: frame 44. Source: byte 3 of frame 036, bit 7
// (measurement: 80 with ignition only -> the box sends 44 01; after engine start 44 00).
// After "01" the radio counts down 6 s and enters power-saving mode.
bool buildEcoMode(const CanFrame& f, Frame& out);

// Climate control: frame 21 from frame 350. PARTIAL mapping, from 18 pairs in the recording:
//   temperatures: DATA[2], DATA[3] = byte 4, 5 of frame 350 + 0x23 (0 stays 0);
//   fan: DATA[1] bits 3-0 = low nibble of byte 6;
//   air distribution: nibbles of byte 7 (left = high, right = low); known codes:
//       8 = windscreen + face + feet, 5 = face + feet, 3 = face, 0 = automatic;
//       other codes (4, 6 ...) have no confirmed mapping and give no indication;
//   A/C: DATA[0] bit 6 = 1 when byte 1 bit 3 is cleared;
//   rear window heating: DATA[0] bit 0 = byte 1 of frame 2AD, bit 5 (in frame 350 this state
//       disappears after a moment, while the box holds it until the heating is switched off);
//   recirculation: DATA[0] bit 5 = high nibble of byte 6 equal to 3;
//   AUTO: DATA[0] bit 3 = left distribution "automatic" and fan 0;
//   front demist: DATA[4] bit 7 = byte 1 bit 4 (one pair in the recording);
//   constants: DATA[0] bits 7 and 2 = 1, DATA[5] = 81 (right after startup the box sends FB; the radio
//       does not read this byte), DATA[6] = 40.
// Agreement with the recording: 26 of 27 frames; the only mismatch is the moment of a mode switch.
const uint32_t CAN_REAR_DEFROST = psa::ID_REAR_DEFROST;   // 2AD
class ClimateEncoder {
public:
    // Returns true when the frame concerned climate control and content for the radio is ready.
    bool onCanFrame(const CanFrame& f, Frame& out);

private:
    vehicle::ClimateState state_;   // psa::decodeClimate keeps it, xp::encodeClimate reads it
};

// Doors: frame 38 (car states). Added 2026-11-16.
//   Which doors: car message in frame 1A1 (byte 2 = DE), byte 4: 40 driver, 80 passenger,
//       10 rear left, 20 rear right, 08 boot (measurement 2026-11-10, test_data/captures/can_20261110_190421.log).
//       The message disappears after about 10 s although the doors are still open.
//   Whether any is open: door warning lamp, frame 128 byte 5 bit 6 (measurement: lights up
//       together with the message and goes out after the last door is closed).
//   The radio reads DATA[0]: bits 7 and 6 front doors, 5 and 4 rear, 3 boot (radio code).
//   Byte 4 of frame 1A1 with sides swapped: driver 40 -> bit 7, passenger 80 -> bit 6,
//   rear left 10 -> bit 5, rear right 20 -> bit 4, boot unchanged
//   (confirmed in the car 2026-11-16: without the swap the sides were reversed).
//   Known limitation: when one of several open doors is closed and the car does not show
//   a new message, the old indication stays until all are closed.
//   DATA[1..5]: vehicle setting states, supplied from outside by setSettings() (
//   computed by vset::update38 from CAN frames 260, 227, 3E1, 217; lib/car_psa_aee2010/vehicle_settings.h).
//   Until the first setSettings() all zeros, like the box before the first frame 260.
//   Up to version 0.7.0 there were constants 02 E1 A9 98 40 here, copied from the box recording of 2026-11-11.
const uint32_t CAN_CAR_MESSAGE = psa::ID_CAR_MESSAGE;   // 1A1
const uint32_t CAN_INDICATORS = psa::ID_INDICATORS;     // 128
class DoorEncoder {
public:
    // Returns true when the frame concerned the doors and content for the radio is ready.
    bool onCanFrame(const CanFrame& f, Frame& out);
    uint8_t doors() const { return state_.open; }
    // Swapping left and right in the frame for the radio (front: bits 7 and 6, rear: 5 and 4).
    // In case the radio draws the doors on the opposite side.
    void setSwapSides(bool swap) { swap_ = swap; }
    bool swapSides() const { return swap_; }
    // Current frame 38 (for re-sending after a setting change).
    void build(Frame& out) const;
    // DATA[1..5] of frame 38 (d[0] = DATA[1]): vehicle setting states. Copies 5 bytes.
    void setSettings(const uint8_t d[5]);

private:
    uint8_t settings_[5] = {0, 0, 0, 0, 0};   // DATA[1..5] of frame 38
    vehicle::DoorState state_;   // open = bits as in byte 4 of frame 1A1 (vehicle::DOOR_*), anyOpen = lamp (128)
    // Test in the car 2026-11-16: with a direct copy the radio showed the sides reversed
    // (front and rear), so the swap is permanently enabled.
    bool swap_ = true;
};

// Flag "sensors see an obstacle": frame 3E = 80 when the sensors are active and any
// zone in frame 0E1 is closer than 7; otherwise 00 (measurement: 10 of 10 changes in the recording).
Frame buildObstacleFlag(bool obstacle);

// Lighting: byte 4 of frame 036, bit 5 = lights on (measurement 2026-11-10, steps 29-30:
// the bit clears when the lights are switched off and returns when switched on). For the "illumination" output.
// A wrapper of psa::decodeLightsOn (psa_frames.h), kept for the 0.6.0 reference and the tests.
bool decodeLightsOn(const CanFrame& f, bool& lightsOn);

}  // namespace xp
