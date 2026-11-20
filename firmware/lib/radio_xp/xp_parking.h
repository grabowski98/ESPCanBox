// Parking sensors: CAN frame 0E1 -> frame 32 for the radio.
// Mapping from the measurement of 2026-11-11 (397 box frames matched against CAN;
// radios/xp-simple-soft/protocol.md).
#pragma once
#include "vehicle_signals.h"   // vehicle::ParkingState (old name psa::ParkingAid: compat/legacy_060.h)
#include "xp_protocol.h"

namespace xp {

// Zone from the car (0 = closest, 7 = far) -> value for the radio (0 = closest, 5 = far).
uint8_t parkingZoneToRadio(uint8_t canZone);

// Builds frame 32. 'valid' = false when frames 0E1 are not arriving (then "no data").
// DATA[0]: 00 sensors inactive (zones FF), 01 only front active, 02 rear active.
Frame buildParkingFrame(const vehicle::ParkingState& pa, bool valid);

}  // namespace xp
