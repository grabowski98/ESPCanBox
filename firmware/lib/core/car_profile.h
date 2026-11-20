// Car profile: everything the firmware needs to know about one car
// family, as one const structure compiled into flash. No allocation, no hardware, no Arduino framework header.
//
// A profile is selected at build time (-DCAR_PROFILE=..., src/config.h) and handed to the Decoder through
// DecoderConfig::car (lib/app/decoder.h; default: the reference profile PSA AEE2010). Profiles:
//   lib/car_psa_aee2010/  Peugeot 508 SW 2015 (reference, default)
//   lib/car_generic/      unknown car: listen only, decodes nothing, no keys, no 12 V outputs
//   lib/car_example/      template with comments (compiled only by the host tests, never into the firmware)
//
// What lives here: tables (buttons, transmit allow list, privacy list) and pointers to the decoding functions
// that are not a simple table: parking sensors, ignition, lights and the car data for the radio screen
// (steering angle, trip computer, outside temperature, economy mode, climate control, doors, vehicle
// settings). A pointer may be nullptr: that signal is then never produced.
// Changing a vehicle setting (radio command -> CAN frame -> check) is not here: it stays the PSA AEE2010
// code in lib/car_psa_aee2010/settings_changer.h behind the transmit gate (docs/en/reference/architecture.md).
// The profile cannot change at run time (const); the only run-time key setting is the radio key remap
// table (xp::KeyCustomizer, used with every radio protocol). Its default entries come from the profile
// (keyRemapDefaults), and only for the radio protocol whose codes they are (keyRemapProtocol).
#pragma once
#include <stdint.h>

#include "can_frame.h"
#include "vehicle_signals.h"

namespace vehicle {

// Physical layer of the bus at the radio connector. Informational only: the firmware does not switch
// transceivers, the board determines it (see the board header and docs).
enum class PhysicalLayer : uint8_t {
    UNKNOWN = 0,
    HIGH_SPEED,           // ISO 11898-2 (both lines about 2.5 V at rest)
    LOW_SPEED_FT,         // fault-tolerant low speed, ISO 11898-3
};

// One "level" button: pressed while (data[byteIndex] & mask) == value in frame canId.
// The ORDER of the rows is the order of matching and the order of the events for the radio
// (several buttons in one frame are reported in row order). At most 32 rows are used.
struct KeyMapEntry {
    uint16_t canId;
    uint8_t byteIndex;    // 0 = first data byte
    uint8_t mask;
    uint8_t value;
    KeyId key;
};

// One rotary knob given as an 8-bit counter in frame canId, byte byteIndex (frame length must be at
// least byteIndex + 1). A counter step of +1 = one detent 'onIncrease', -1 = one detent 'onDecrease'.
// At most 4 detents are reported per frame (protection against a burst after lost frames).
struct KnobEntry {
    uint16_t canId;
    uint8_t byteIndex;
    KeyId onIncrease;
    KeyId onDecrease;
};

// Experimental transmit frames. 0 (default) = a profile lists only the
// frames confirmed for normal use; 1 = it also lists trial frames (for PSA AEE2010: the 1A9 screen button
// with its two recorded contents). Set only with -DCAN_TX_EXPERIMENTAL=1 in a build
// environment, never by a change here. The same value must reach every file (a global build flag).
#ifndef CAN_TX_EXPERIMENTAL
#define CAN_TX_EXPERIMENTAL 0
#endif
#if CAN_TX_EXPERIMENTAL != 0 && CAN_TX_EXPERIMENTAL != 1
#error "CAN_TX_EXPERIMENTAL must be 0 or 1"
#endif

// Release frame after a trial button press. 0 (default) = a button press is one frame and nothing follows;
// 1 = about 200 ms after a press acknowledged on the bus, one "release" frame with no button bit (for PSA
// AEE2010: 1A9 20 7F FF 00 FF 00 50 00, the periodic 1A9 the aftermarket box sent every ~204 ms with ignition
// on; a press of the box replaced one periodic frame and the next one followed it; that a press without such a
// following frame causes a later toggle is a hypothesis). Meaningful only together with
// CAN_TX_EXPERIMENTAL=1 (the trial frames themselves); 1 without it does not build. Set only with -DCAN_TX_1A9_RELEASE=1 in a build environment, like
// CAN_TX_EXPERIMENTAL, never by a change here. The same value must reach every file (a global build flag).
#ifndef CAN_TX_1A9_RELEASE
#define CAN_TX_1A9_RELEASE 0
#endif
#if CAN_TX_1A9_RELEASE != 0 && CAN_TX_1A9_RELEASE != 1
#error "CAN_TX_1A9_RELEASE must be 0 or 1"
#endif
#if CAN_TX_1A9_RELEASE && !CAN_TX_EXPERIMENTAL
#error "CAN_TX_1A9_RELEASE=1 needs CAN_TX_EXPERIMENTAL=1"
#endif

// One frame the profile allows the CAN transmit gate to send (lib/core/can_tx_gate.h): identifier, length
// and a content validator. The validator is C++ code in the profile and decides about the content;
// nullptr = the frame is refused. len = exact length, or 0 = the validator also checks the length (a frame
// with more than one allowed length). The gate looks the identifier up in the list (first matching row)
// and checks in this order: identifier on the list, length, validator.
// The list is const data compiled into flash: nothing can add a row at run time.
struct TxAllowEntry {
    uint16_t canId;
    uint8_t len;
    bool (*validator)(const CanFrame& f);
};

// One default entry of the radio key remap table: key code 'from' is sent to the radio as 'to'.
// Both are key codes of ONE radio protocol, named in CarProfile::keyRemapProtocol (the remap table itself,
// lib/radio_xp/xp_keys_custom.h, works with the codes of any protocol): they are the owner's preference for one
// car and one radio, not a property of the bus. The table is the starting state of the remap table and the state
// the command "map reset" returns to; a table stored by the user (console "map", web panel) replaces it.
struct KeyRemapDefault {
    uint8_t from;
    uint8_t to;
};

struct CarProfile {
    const char* id;                   // short ASCII name, e.g. "psa_aee2010" (console, panel)
    uint32_t canBitrate;              // bit/s; src/can_bus.cpp refuses to start on a speed it does not know
    PhysicalLayer physicalLayer;      // informational

    const KeyMapEntry* keys;          // level buttons, in matching order (nullptr when keyCount = 0)
    uint8_t keyCount;
    const KnobEntry* knobs;           // rotary knobs (nullptr when knobCount = 0)
    uint8_t knobCount;

    const TxAllowEntry* txAllow;      // frames the transmit gate may send (nullptr / 0 = listen-only profile)
    uint8_t txAllowCount;

    // Frames whose data identify the car (e.g. VIN): overwritten in published captures
    // (../tools/sanitize_capture.py). Informational for the firmware.
    const uint16_t* privacyIds;
    uint8_t privacyCount;

    // Decoding of the signals the generic logic uses. Each returns false when the frame is not the
    // right one (or too short). nullptr = the car profile does not provide this signal.
    bool (*decodeParking)(const CanFrame& f, ParkingState& out);   // parking sensors (sound, zones)
    bool (*decodeBody)(const CanFrame& f, BodyState& out);         // ignition and reverse gear
    bool (*decodeLights)(const CanFrame& f, bool& lightsOn);       // lights on (illumination output)

    // true = the profile decodes ignition reliably enough to drive the 12 V outputs (ACC, illumination,
    // reverse) in decoder mode. false = src/main.cpp keeps all 12 V outputs off.
    bool powerOutputs;

    // Default entries of the radio key remap table (see KeyRemapDefault), in table order; at most 16 are used.
    // nullptr / 0 = no remapping by default (every key is sent with its own code).
    const KeyRemapDefault* keyRemapDefaults;
    uint8_t keyRemapDefaultCount;

    // ---- car data for the radio screen (vehicle_signals.h) ----
    // nullptr = the car does not give this data (the radio then never gets it). The Decoder calls every
    // non-null function with every received frame, in this order, and hands the result to the encoder of
    // the radio protocol (RadioProtocol::encode...). A function returns false / 0 when the frame is not its
    // own (or too short). Functions with 'io' get the state the Decoder keeps (all defaults at startup) and
    // change only what the frame states, so a profile can combine several frames without own memory.
    bool (*decodeSteering)(const CanFrame& f, SteeringAngle& out);
    uint8_t (*decodeTrip)(const CanFrame& f, TripData& io);           // page bits TRIP_* that changed
    bool (*decodeOutsideTemp)(const CanFrame& f, OutsideTemp& out);
    bool (*decodeEcoMode)(const CanFrame& f, EcoMode& out);
    bool (*decodeClimate)(const CanFrame& f, ClimateState& io);       // true = report (state valid)
    bool (*decodeDoors)(const CanFrame& f, DoorState& io);            // true = door state (re)stated
    uint8_t (*decodeSettings)(const CanFrame& f, VehicleSettings& io);   // SETTINGS_VALUES / SETTINGS_PRESENT

    // Radio protocol whose key codes keyRemapDefaults holds (RadioProtocol::id, e.g. "xp"). The Decoder uses
    // the defaults only with that protocol: with any other radio they would be foreign codes (review: the PSA
    // defaults 9F->9B and 21->1F reached a custom protocol). nullptr = codes of whichever protocol is built
    // (a profile written for one's own radio). May be left out of an initializer (then nullptr).
    const char* keyRemapProtocol;
};

}  // namespace vehicle
