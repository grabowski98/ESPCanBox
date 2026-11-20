// Old names of the car signal types (they used to be defined in psa_frames.h).
// Only aliases: the types themselves live in vehicle_signals.h. Thanks to these aliases the existing
// code (RadioLink::onParking, PowerPolicy::onBodyState, ParkingBeeper::onFrame, the frozen 0.6.0
// reference test_host/ref_glue_060.h, ...) compiles without a single token changed.
#pragma once
#include "../vehicle_signals.h"

namespace psa {

using ParkStatus = vehicle::ParkStatus;
using vehicle::PARK_FAULT;
using vehicle::PARK_OFF_BY_DRIVER;
using vehicle::PARK_ACTIVE;
using vehicle::PARK_WAITING;
using vehicle::PARK_OUT_OF_SERVICE;

using vehicle::INTERVAL_NONE;
using vehicle::INTERVAL_CONTINUOUS;

using ParkingAid = vehicle::ParkingState;
using BodyState = vehicle::BodyState;

using vehicle::ignitionOn;

}  // namespace psa
