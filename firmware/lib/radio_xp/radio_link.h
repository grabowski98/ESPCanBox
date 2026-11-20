// RadioLink: the common radio session (lib/core/radio_session.h) with the XP protocol (kRadioXp).
// The name of the 0.6.0 code: the reference glue (test_host/ref_glue_060.h) and the tests use it; the Decoder
// uses RadioSession with the protocol of its DecoderConfig.
//
// What the XP session sends and receives (protocol_xp.cpp; only what has a mapping confirmed by measurement):
//   sent: 20 keys, 32 sensors, 3E obstacle, 29 steering angle, 33-35 trip
//   computer, 36 temperature, 21 climate control, 44 economy mode, 71 version, EA power;
//   received and stored: A6 time, C4 volume, C0 source, 81 radio state,
//   80 vehicle setting, 8A climate command, 90 frame request.
//   38 (doors and car setting states) and 3C (which settings the car has) arrive here as state
//   frames through onState (built by xp::encodeCarState and xp::encodeSettingsPresent, xp_signals.h).
// Not sent: 3B, 3D, 01, 37, 39: no reliable mapping from CAN.
#pragma once
#include "protocol_xp.h"
#include "radio_session.h"
#include "vehicle_signals.h"   // vehicle::ParkingState (old name psa::ParkingAid: compat/legacy_060.h)
#include "xp_protocol.h"

class RadioLink : public RadioSession {
public:
    RadioLink() : RadioSession(&kRadioXp) {}
};
