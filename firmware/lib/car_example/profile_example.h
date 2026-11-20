// Car profile "example": a commented template for writing the profile of another car.
// ALL FRAMES HERE ARE INVENTED: this is not a real car. It is compiled only by the host tests
// (test_host/example_car.cpp with the synthetic capture test_host/synthetic/example_car.log), so that the
// template keeps compiling and working; platformio.ini keeps it out of the firmware (lib_ignore).
//
// How to make a real profile from it:
//   1. copy lib/car_example to lib/car_<your_car>, rename the files and the kCar... object, and change
//      "name" in its library.json to car_<your_car> (lib_deps below finds the library by that name);
//   2. record the bus with the "generic" profile (environment listen_generic_wifi) and find, one change at a
//      time, which bits move with which button / sensor / ignition state (vehicles/_template/README.md);
//   3. fill in only what you measured; leave a pointer nullptr or a table empty when you do not know;
//   4. add a host test on your recording (test_host/), like test_host/example_car.cpp;
//   5. build it without editing src/: -DCAR_PROFILE=CUSTOM -DCAR_PROFILE_CUSTOM_HEADER='"profile_<your_car>.h"'
//      -DCAR_PROFILE_CUSTOM_OBJECT=kCar<YourCar> and lib_deps = car_<your_car> (platformio_override.sample.ini,
//      src/config.h). A custom profile is listen only; transmitting needs a named profile row in src/config.h.
#pragma once
#include "car_profile.h"

extern const vehicle::CarProfile kCarExample;

namespace example {

// Invented frame identifiers of the template.
const uint16_t ID_BODY = 0x100;      // byte 1: ignition (0x80 off, 0x88 on, 0x8E engine); byte 2 bit 0: reverse
const uint16_t ID_KEYS = 0x200;      // byte 1: buttons (bit 0 volume +, bit 1 volume -, bit 2 source); byte 2: knob counter
const uint16_t ID_PARKING = 0x300;   // byte 1: rear status; byte 2: nearest rear zone 0..7; byte 3 bit 0: sound request
const uint16_t ID_LIGHTS = 0x400;    // byte 1 bit 0: lights on
const uint16_t ID_DOORS = 0x500;     // byte 1: bit 0 driver, 1 passenger, 2 rear left, 3 rear right, 4 boot, 5 bonnet
const uint16_t ID_TRIP = 0x510;      // trip 1: byte 1 = value a, bytes 2-3 = value b (high byte first)

bool decodeBody(const CanFrame& f, vehicle::BodyState& out);
bool decodeParking(const CanFrame& f, vehicle::ParkingState& out);
bool decodeLights(const CanFrame& f, bool& lightsOn);
// Car data for the radio screen (vehicle_signals.h): doors and one trip computer page.
bool decodeDoors(const CanFrame& f, vehicle::DoorState& io);
uint8_t decodeTrip(const CanFrame& f, vehicle::TripData& io);

}  // namespace example
