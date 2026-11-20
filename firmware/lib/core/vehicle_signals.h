// Car-neutral signal types shared by the generic logic (lib/core), the car profiles (lib/car_*)
// and the radio protocols (lib/radio_*). No hardware, no Arduino framework header.
//
// The types were moved here unchanged from lib/car_psa_aee2010/psa_frames.h, so that lib/core and lib/radio_* no longer need a car header. The old names
// psa::ParkingAid, psa::BodyState, psa::PARK_*, psa::INTERVAL_* and psa::ignitionOn stay valid as
// aliases in compat/legacy_060.h.
//
// Field meanings follow the first (and reference) profile, PSA AEE2010 (e.g. zones 0..7, the beep interval
// field): a car profile that decodes a different car fills these fields with the same meaning. Ignition is
// neutral (enum Ignition); the raw ignition byte is only for display. The logical
// keys (KeyId) used by CarProfile (car_profile.h) and RadioProtocol (radio_protocol.h).
// The car data for the radio screen (steering angle, trip computer, outside temperature, economy mode,
// climate control, doors, vehicle settings) are at the end of this file: a car profile decodes CAN into
// them, a radio protocol encodes them for its radio, and the Decoder (lib/app) only connects the two.
#pragma once
#include <stdint.h>

namespace vehicle {

// Front / rear state of the parking sensor system.
enum ParkStatus : uint8_t {
    PARK_FAULT = 1,
    PARK_OFF_BY_DRIVER = 2,
    PARK_ACTIVE = 4,
    PARK_WAITING = 5,
    PARK_OUT_OF_SERVICE = 6,
};

const uint8_t INTERVAL_NONE = 0x3F;        // no beeps
const uint8_t INTERVAL_CONTINUOUS = 0x00;  // continuous tone (nearest zone)

struct ParkingState {
    uint8_t rearStatus = 0;
    uint8_t frontStatus = 0;
    bool soundRequest = false;   // the car asks for sound
    bool soundFront = false;     // true = obstacle in front, false = behind
    uint8_t soundSides = 0;      // 0 none, 1 left, 2 right, 3 both
    uint8_t beepDurationCode = 0;
    uint8_t intervalRaw = INTERVAL_NONE;   // interval field from the car: 0x39..0x0B closer and closer, 0x00 continuous tone
    // zones 0..7: 0 = very close, 7 = far / no obstacle
    uint8_t rearLeft = 7, rearCenter = 7, rearRight = 7;
    uint8_t frontLeft = 7, frontCenter = 7, frontRight = 7;
    bool displayRequest = false;

    bool anyActive() const { return rearStatus == PARK_ACTIVE || frontStatus == PARK_ACTIVE; }
    uint8_t nearestZone() const;
    uint8_t nearestFront() const;
    uint8_t nearestRear() const;
};

// Ignition state in car-neutral form. A car profile converts its own encoding into it (CarProfile::decodeBody).
//   OFF         ignition off (also: nothing decoded yet);
//   ON          ignition on, engine not running (steady state);
//   ENGINE      engine running (steady state);
//   TRANSITION  any other "not off" value: cranking, switching off, a value the profile does not name.
// The generic logic uses only this field: PowerPolicy (ACC: everything except OFF) and the CAN transmit
// gate conditions (only ON or ENGINE, never a transition).
enum class Ignition : uint8_t {
    OFF = 0,
    ON,
    ENGINE,
    TRANSITION,
};

struct BodyState {
    bool reverseGear = false;
    Ignition ignition = Ignition::OFF;
    // The car's own ignition value, only shown to the user (console, panel). Its encoding belongs to the
    // profile (PSA AEE2010: byte 1 of frame 0F6); no generic logic reads it.
    uint8_t ignitionRaw = 0;
};

// Ignition on: every state except OFF (cranking and switching off count as on, so ACC does not drop).
inline bool ignitionOn(const BodyState& b) { return b.ignition != Ignition::OFF; }

// Logical buttons: a car profile maps CAN bits to these names (CarProfile::keys / knobs,
// car_profile.h), a radio protocol maps them to its own key codes (RadioProtocol::keyCode,
// radio_protocol.h). The names describe the button in the reference car (Peugeot 508, PSA AEE2010);
// another car reuses the name with the closest meaning. NONE = no button (never sent).
// New names are appended at the end only, so that the numbers of existing names never change.
enum class KeyId : uint8_t {
    NONE = 0,
    VOLUME_UP,          // steering wheel volume +
    VOLUME_DOWN,        // steering wheel volume -
    MUTE,               // volume + and - together
    PHONE,              // steering wheel phone button
    LIST,               // steering wheel LIST button
    WHEEL_KNOB_PRESS,   // steering wheel knob pressed
    SOURCE,             // SRC
    TRIP,               // trip computer button (wiper stalk)
    NAV,                // panel above the climate control: NAV
    MEDIA,              // panel: MEDIA
    SCROLL_LEFT,        // panel: scroll left
    ZOOM_IN,            // panel: magnifier +
    SCROLL_RIGHT,       // panel: scroll right
    TEL,                // panel: TEL
    SETUP,              // panel: SETUP
    DRIVE,              // panel: DRIVE
    MOON,               // panel: moon (screen off)
    ZOOM_OUT,           // panel: magnifier -
    MIC_MUTE,           // panel: microphone mute
    WEB,                // panel: WEB
    PANEL_EXTRA,        // panel: further button (name to be confirmed)
    PANEL_KNOB_PRESS,   // panel power knob pressed
    NEXT,               // steering wheel knob: one detent towards "next"
    PREV,               // steering wheel knob: one detent towards "previous"
    PANEL_KNOB_UP,      // panel knob: one detent to the right
    PANEL_KNOB_DOWN,    // panel knob: one detent to the left
    COUNT               // number of names (not a button)
};

// ====================== Car data for the radio screen ======================
// Produced by the decode functions of a car profile (CarProfile::decodeSteering ... decodeSettings,
// car_profile.h), consumed by the encode functions of a radio protocol (RadioProtocol::encodeSteering ...,
// radio_protocol.h). Each type has a validity flag: false = the car has not reported it yet.
// Only the fields the reference profile (PSA AEE2010) really decodes are here. Where the meaning or the
// scale of a value is not verified in the reference car, the field keeps the reference car's encoding and
// says so: a profile for another car fills such a field only when its car uses the same encoding.

// Steering wheel angle.
struct SteeringAngle {
    bool valid = false;
    // 16-bit value in the reference car's encoding (PSA AEE2010: frame 10B bytes 1-2, byte 1 = high byte).
    // Scale and sign convention NOT determined (vehicles/peugeot-508-sw-2015/README.md).
    uint16_t value = 0;
};

// Outside temperature in half degrees Celsius (e.g. 13 = 6.5 C, -3 = -1.5 C).
struct OutsideTemp {
    bool valid = false;
    int16_t halfDegC = 0;
};

// Economy (energy saving) mode of the car: the car is about to cut the accessories while the engine is off.
struct EcoMode {
    bool valid = false;
    bool on = false;
};

// Trip computer. The reference car sends three pages and the aftermarket box copies them byte for byte;
// the meaning and the units of the values are NOT verified (vehicles/peugeot-508-sw-2015/508.dbc, frames
// 221, 2A1, 261). The values keep the reference car's encoding, in the order the car sends them.
struct TripPage {
    bool valid = false;
    uint8_t a = 0;    // 8-bit value  (DBC comment for trip 1: "probably average speed", unverified)
    uint16_t b = 0;   // 16-bit value (DBC comment for trip 1: "probably distance", unverified)
    uint16_t c = 0;   // 16-bit value (DBC comment for trip 1: "probably consumption x10", unverified)
};
struct TripData {
    bool nowValid = false;
    // Current page: three 16-bit values (reference car: frame 221 bytes 2-3, 4-5, 6-7, high byte first).
    // The grouping in 16-bit values follows trips 1 and 2; it is not verified either.
    uint16_t now[3] = {0, 0, 0};
    TripPage trip1, trip2;
};
// Which page a decoded frame updated (bits of the CarProfile::decodeTrip result).
const uint8_t TRIP_NOW = 0x01;
const uint8_t TRIP_1 = 0x02;
const uint8_t TRIP_2 = 0x04;

// Air distribution flags (ClimateState::airLeft / airRight).
const uint8_t AIR_WINDSCREEN = 0x01;
const uint8_t AIR_FACE = 0x02;
const uint8_t AIR_FEET = 0x04;
const uint8_t AIR_AUTO = 0x08;      // automatic distribution (then no other flag)

// Climate control state as shown by the car's own panel. A car code with no known meaning gives no flag.
struct ClimateState {
    bool valid = false;            // the climate frame has arrived (rearDefrost alone does not make it valid)
    bool acOn = false;             // air conditioning compressor enabled
    bool recirculation = false;
    bool frontDemist = false;      // windscreen demist (reference car: one observation)
    bool rearDefrost = false;      // rear window heating
    uint8_t fan = 0;               // fan level 0..15, 0 = automatic (reference car)
    uint8_t airLeft = 0;           // AIR_* flags; 0 = no known distribution
    uint8_t airRight = 0;
    // Set temperature in half degrees Celsius (45 = 22.5 C). 0 = no value: the reference car then sends 0;
    // what that means (off, LO / HI) is not determined.
    uint8_t tempLeft = 0;
    uint8_t tempRight = 0;
};

// Doors, boot and bonnet. The bit values are those of the panel API field car.doors
// (docs/en/reference/panel-api.md), so the panel and the console line "[car] doors=0x.." keep their meaning.
const uint8_t DOOR_PASSENGER = 0x80;
const uint8_t DOOR_DRIVER = 0x40;
const uint8_t DOOR_REAR_RIGHT = 0x20;
const uint8_t DOOR_REAR_LEFT = 0x10;
const uint8_t DOOR_BOOT = 0x08;
const uint8_t DOOR_BONNET = 0x04;   // not reported by the reference car
struct DoorState {
    bool valid = false;            // a door frame has arrived (the state is known)
    uint8_t open = 0;              // DOOR_* bits of the open doors
    // The car shows its "door open" warning. A profile may use it to decide when 'open' is stale
    // (reference car: the door message lasts about 10 s, the warning lamp stays on).
    bool anyOpen = false;
};

// Vehicle settings (personalisation): a list of items with neutral identifiers. The names follow the items
// of the reference radio's settings screen; values are given per identifier below. A car that has no such
// item leaves it unknown.
enum class SettingId : uint8_t {
    PARKING_ASSIST = 0,     // 0 / 1
    REAR_WIPER_REVERSE,     // 0 / 1: rear wiper in reverse
    AMBIENT_LIGHT,          // 0 = off, 1..8 = level
    PARKING_SENSORS,        // 0 / 1: the car's "radar" state bit (reference car: frame 227, polarity unverified)
    FOLLOW_ME_HOME,         // 0 = off, 1..4 = duration step
    WELCOME_LIGHTING,       // 0 = off, 1..4 = duration step
    DAYTIME_LIGHTS,         // 0 / 1
    SOUND,                  // 0..3: sound set as the car reports it
    WELCOME_FUNCTION,       // 0 / 1
    DOOR_UNLOCK_ALL,        // 1 = unlocking opens all doors, 0 = driver's door only
    // Position in the reference radio's language list (0 English, 3 French, 4 German, 5 Spanish, 6 Italian,
    // 7 Dutch, 8 Polish; other positions not named). Only Polish is measured
    // (radios/xp-simple-soft/provenance.md).
    LANGUAGE,
    BLIND_SPOT,             // 0 / 1
    STOP_START,             // 0 / 1: 1 = Stop and Start switched off with the button (reference car, measured)
    FUEL_UNITS,             // 0 = km/L, 1 = L/100 km, 2 = MPG
    FATIGUE_DETECTION,      // 0 / 1
    INSTRUMENT_COLOUR,      // 0 / 1
    // "The car has this item" bits whose item is not known. The reference car reports them (frame 361
    // byte 1 bit 7, byte 3 bit 6, byte 4 bit 4) and the reference radio receives them; no value.
    UNASSIGNED_1,
    UNASSIGNED_2,
    UNASSIGNED_3,
    COUNT
};
struct SettingItem {
    uint8_t value = 0;
    // The car's latest frame stated this value. false = no information: a radio keeps what it shows.
    bool known = false;
    bool present = false;          // the car has this item (meaningful when presentKnown)
    bool presentKnown = false;     // the car stated whether it has this item
};
struct VehicleSettings {
    // The values are valid: the car has sent its settings at least once (reference car: a full frame 260).
    // Before that a radio gets "no settings" (reference radio: zeros).
    bool valid = false;
    SettingItem items[(uint8_t)SettingId::COUNT];
    SettingItem& at(SettingId id) { return items[(uint8_t)id]; }
    const SettingItem& at(SettingId id) const { return items[(uint8_t)id]; }
};
// What a frame changed (bits of the CarProfile::decodeSettings result).
const uint8_t SETTINGS_VALUES = 0x01;     // values (or their 'known' flags)
const uint8_t SETTINGS_PRESENT = 0x02;    // which items the car has

}  // namespace vehicle
