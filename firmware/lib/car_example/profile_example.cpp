// Template car profile with INVENTED frames (see profile_example.h). Every decoder checks the identifier
// and the length first and never trusts a length from the frame beyond 8 bytes.
#include "profile_example.h"

namespace example {

// Ignition and reverse. The template's invented ignition byte (0x80 off, 0x88 on, 0x8E engine running) is
// converted into the neutral vehicle::Ignition; a real profile converts its own encoding the same way and
// keeps its raw value in ignitionRaw only for display.
bool decodeBody(const CanFrame& f, vehicle::BodyState& out) {
    if (f.id != ID_BODY || f.len < 2) return false;
    out.ignitionRaw = f.data[0];
    switch (f.data[0]) {
        case 0x00:
        case 0x80: out.ignition = vehicle::Ignition::OFF; break;
        case 0x88: out.ignition = vehicle::Ignition::ON; break;
        case 0x8E: out.ignition = vehicle::Ignition::ENGINE; break;
        default: out.ignition = vehicle::Ignition::TRANSITION; break;
    }
    out.reverseGear = (f.data[1] & 0x01) != 0;
    return true;
}

// Parking sensors: only the rear centre zone and the sound request are filled in; the other zones stay
// 7 (= no obstacle), the front stays inactive (status 0). A real profile fills what its car reports.
bool decodeParking(const CanFrame& f, vehicle::ParkingState& out) {
    if (f.id != ID_PARKING || f.len < 3) return false;
    out = vehicle::ParkingState();
    out.rearStatus = f.data[0];
    out.rearCenter = f.data[1] > 7 ? 7 : f.data[1];
    out.soundRequest = (f.data[2] & 0x01) != 0;
    out.soundFront = false;
    // Beep interval: here a simple scale from the zone (0 = continuous tone); a real car usually sends it.
    out.intervalRaw = !out.soundRequest ? vehicle::INTERVAL_NONE
                      : out.rearCenter == 0 ? vehicle::INTERVAL_CONTINUOUS
                                            : (uint8_t)(0x0B + out.rearCenter * 6);
    return true;
}

bool decodeLights(const CanFrame& f, bool& lightsOn) {
    if (f.id != ID_LIGHTS || f.len < 1) return false;
    lightsOn = (f.data[0] & 0x01) != 0;
    return true;
}

// Doors: one byte with one bit per door, converted into the neutral DOOR_* bits (the radio protocol then
// draws them its own way). This invented car reports every change at once, so no state is needed.
bool decodeDoors(const CanFrame& f, vehicle::DoorState& io) {
    if (f.id != ID_DOORS || f.len < 1) return false;
    const uint8_t b = f.data[0];
    uint8_t open = 0;
    if (b & 0x01) open |= vehicle::DOOR_DRIVER;
    if (b & 0x02) open |= vehicle::DOOR_PASSENGER;
    if (b & 0x04) open |= vehicle::DOOR_REAR_LEFT;
    if (b & 0x08) open |= vehicle::DOOR_REAR_RIGHT;
    if (b & 0x10) open |= vehicle::DOOR_BOOT;
    if (b & 0x20) open |= vehicle::DOOR_BONNET;
    io.open = open;
    io.anyOpen = open != 0;
    io.valid = true;
    return true;
}

// Trip computer: only page "trip 1", values a and b; c stays 0. A real profile fills a page only when its
// car uses the encoding described in vehicle_signals.h (TripData): the radio shows the values as they are.
uint8_t decodeTrip(const CanFrame& f, vehicle::TripData& io) {
    if (f.id != ID_TRIP || f.len < 3) return 0;
    io.trip1.valid = true;
    io.trip1.a = f.data[0];
    io.trip1.b = (uint16_t)((f.data[1] << 8) | f.data[2]);
    io.trip1.c = 0;
    return vehicle::TRIP_1;
}

}  // namespace example

namespace {

using vehicle::KeyId;

// Level buttons: pressed while (byte & mask) == value. Row order = matching order = event order.
const vehicle::KeyMapEntry KEYS[] = {
    {example::ID_KEYS, 0, 0x01, 0x01, KeyId::VOLUME_UP},
    {example::ID_KEYS, 0, 0x02, 0x02, KeyId::VOLUME_DOWN},
    {example::ID_KEYS, 0, 0x04, 0x04, KeyId::SOURCE},
};

// One knob: byte 2 of the key frame is a counter, +1 per detent = "next".
const vehicle::KnobEntry KNOBS[] = {
    {example::ID_KEYS, 1, KeyId::NEXT, KeyId::PREV},
};

}  // namespace

const vehicle::CarProfile kCarExample = {
    "example",
    500000,                                  // invented; a real profile states the measured speed
    vehicle::PhysicalLayer::UNKNOWN,
    KEYS, (uint8_t)(sizeof(KEYS) / sizeof(KEYS[0])),
    KNOBS, (uint8_t)(sizeof(KNOBS) / sizeof(KNOBS[0])),
    nullptr, 0,                              // never transmits
    nullptr, 0,                              // no identifying frames in the invented bus
    example::decodeParking,
    example::decodeBody,
    example::decodeLights,
    true,                                    // shows how ACC follows ignition (host test only)
    // Default radio key remaps (key codes of your radio protocol, e.g. with XP {0x21, 0x1F} sends SOURCE as
    // "voice assistant"; name that protocol in the last field, keyRemapProtocol): none, every key reaches the
    // radio with its own code. A remap is a preference, not a fact about the car.
    nullptr, 0,
    // Car data for the radio screen: this template gives doors and one trip computer page; nullptr for the
    // rest (steering angle, outside temperature, economy mode, climate control, vehicle settings).
    nullptr,
    example::decodeTrip,
    nullptr,
    nullptr,
    nullptr,
    example::decodeDoors,
    nullptr,
};
