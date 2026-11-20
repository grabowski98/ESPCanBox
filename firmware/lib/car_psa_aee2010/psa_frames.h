// Decoding of frames on the Peugeot 508 INFO-DIV bus (AEE2010, 125 kbit/s).
// Field layout confirmed by a capture from the car of 10.11.2026: test_data/captures/can_20261110_190421.log.
#pragma once
#include "can_frame.h"
#include "can_ids.h"   // psa::ID_PARKING_AID (0E1, every 100 ms), psa::ID_BODY_STATE (0F6, every 500 ms)
// psa::ParkingAid, psa::BodyState, psa::PARK_*, psa::INTERVAL_*, psa::ignitionOn: aliases of
// the car-neutral types in lib/core/vehicle_signals.h (same fields, same defaults).
#include "compat/legacy_060.h"

namespace psa {

// Returns false if this is not the right frame or it is too short.
bool decodeParkingAid(const CanFrame& f, ParkingAid& out);

bool decodeBodyState(const CanFrame& f, BodyState& out);
// Byte 1 of frame 0F6 -> vehicle::Ignition (decodeBodyState fills both ignitionRaw and ignition).
vehicle::Ignition ignitionFromByte(uint8_t b);

// Lighting: byte 4 of frame 036, bit 5 = lights on (measurement 2026-11-10: the bit clears when the lights
// are switched off and returns when switched on). For the "illumination" output. Moved here from
// xp::decodeLightsOn (lib/app/xp_car.cpp), unchanged; xp::decodeLightsOn now calls this one.
inline bool decodeLightsOn(const CanFrame& f, bool& lightsOn) {
    if (f.id != ID_LIGHTING || f.len < 4) return false;
    lightsOn = (f.data[3] & 0x20) != 0;
    return true;
}

}  // namespace psa
