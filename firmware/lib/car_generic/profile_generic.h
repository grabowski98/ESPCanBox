// Car profile "generic": the entry point for a car without its own profile.
// Listen only: no frame is decoded, there are no buttons, the transmit allow list is empty and the
// 12 V outputs stay off. The firmware still receives every frame (USB "dump", web panel, sleep on a quiet
// bus), so the bus of another car can be recorded and studied. Selected with -DCAR_PROFILE=GENERIC
// (environment listen_generic_wifi). How to write a real profile: lib/car_example/profile_example.cpp.
#pragma once
#include "car_profile.h"

extern const vehicle::CarProfile kCarGeneric;

namespace generic {
// Rows of the transmit allow list, known at compile time (src/config.h: a transmitting environment does not
// build with an empty list). Always 0: this profile never transmits.
constexpr uint8_t TX_ALLOW_COUNT = 0;
}  // namespace generic
