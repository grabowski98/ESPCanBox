#include "profile_generic.h"

const vehicle::CarProfile kCarGeneric = {
    "generic",
    // Bit rate of the bus to listen to. 125 kbit/s is only the default of this firmware (the speed of the
    // reference car), not a fact about your car: measure it first (vehicles/_template/README.md) and change it
    // here or in your own profile. src/can_bus.cpp starts the controller in listen-only mode.
    125000,
    vehicle::PhysicalLayer::UNKNOWN,
    nullptr, 0,          // no buttons
    nullptr, 0,          // no knobs
    nullptr, generic::TX_ALLOW_COUNT,   // transmit allow list: empty (this profile never transmits)
    nullptr, 0,          // privacy list: none known
    nullptr,             // no parking sensors
    nullptr,             // no ignition / reverse: ACC never on
    nullptr,             // no lights
    false,               // never drive the 12 V outputs
    nullptr, 0,          // no default radio key remaps
    // no car data for the radio screen: steering, trip, outside temperature, economy mode, climate,
    // doors, vehicle settings
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};
