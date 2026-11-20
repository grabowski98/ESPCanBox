// Car profile PSA AEE2010 (Peugeot 508 SW 2015). Sources: vehicles/peugeot-508-sw-2015/README.md.
//
// Key map: the CAN column of the former table LEVEL_KEYS (formerly lib/radio_xp/xp_keys.cpp), in the
// SAME ORDER: the order is the matching order and the order of events for the radio. The XP key codes
// moved to lib/radio_xp/protocol_xp.cpp (xp::keyCode); test_host/key_tables.cpp checks that every row of
// the old table gives the same code through the two tables.
// The table comes from the measurement of 2026-11-11 (CAN and the box -> radio line recorded on one clock,
// test_data/captures/can_20261111_190817.log, can_20261111_191045.log).
#include "profile_psa_aee2010.h"

#include "can_ids.h"
#include "psa_frames.h"
#include "psa_signals.h"

namespace {

using vehicle::KeyId;
using vehicle::KeyMapEntry;
using vehicle::KnobEntry;

const KeyMapEntry KEYS[] = {
    // --- steering wheel, frame 21F ---
    {psa::ID_WHEEL_KEYS, 0, 0x0C, 0x08, KeyId::VOLUME_UP},
    {psa::ID_WHEEL_KEYS, 0, 0x0C, 0x04, KeyId::VOLUME_DOWN},
    {psa::ID_WHEEL_KEYS, 0, 0x0C, 0x0C, KeyId::MUTE},            // both at once
    {psa::ID_WHEEL_KEYS, 0, 0x02, 0x02, KeyId::PHONE},
    {psa::ID_WHEEL_KEYS, 0, 0x01, 0x01, KeyId::LIST},
    {psa::ID_WHEEL_KEYS, 2, 0x80, 0x80, KeyId::WHEEL_KNOB_PRESS},
    // --- steering wheel, frame 0A2 ---
    {psa::ID_WHEEL_KNOB, 1, 0x04, 0x04, KeyId::SOURCE},
    // --- wiper stalk, frame 221 ---
    {psa::ID_TRIP_NOW, 0, 0x01, 0x01, KeyId::TRIP},
    // --- panel above the climate control, frame 122 ---
    {psa::ID_PANEL_KEYS, 1, 0x80, 0x80, KeyId::NAV},
    {psa::ID_PANEL_KEYS, 1, 0x20, 0x20, KeyId::MEDIA},
    {psa::ID_PANEL_KEYS, 1, 0x01, 0x01, KeyId::SCROLL_LEFT},
    {psa::ID_PANEL_KEYS, 2, 0x40, 0x40, KeyId::ZOOM_IN},
    {psa::ID_PANEL_KEYS, 2, 0x80, 0x80, KeyId::SCROLL_RIGHT},
    {psa::ID_PANEL_KEYS, 1, 0x08, 0x08, KeyId::TEL},
    {psa::ID_PANEL_KEYS, 1, 0x02, 0x02, KeyId::SETUP},
    {psa::ID_PANEL_KEYS, 1, 0x10, 0x10, KeyId::DRIVE},
    {psa::ID_PANEL_KEYS, 1, 0x40, 0x40, KeyId::MOON},
    {psa::ID_PANEL_KEYS, 2, 0x20, 0x20, KeyId::ZOOM_OUT},
    {psa::ID_PANEL_KEYS, 1, 0x04, 0x04, KeyId::MIC_MUTE},
    {psa::ID_PANEL_KEYS, 0, 0x01, 0x01, KeyId::WEB},
    {psa::ID_PANEL_KEYS, 0, 0x02, 0x02, KeyId::PANEL_EXTRA},     // name to be confirmed
    // --- panel power knob: press, frame 036 ---
    // Measurement: byte 7 has value 01, and 02 while pressed.
    {psa::ID_LIGHTING, 6, 0x0F, 0x02, KeyId::PANEL_KNOB_PRESS},
};

// Knobs: the counter in the frame changes by 1 per detent.
//   21F byte 2: counter down (00 -> FF) = "next", up = "previous";
//   122 byte 7: counter up = panel knob to the right, down = to the left.
const KnobEntry KNOBS[] = {
    {psa::ID_WHEEL_KEYS, 1, KeyId::PREV, KeyId::NEXT},
    {psa::ID_PANEL_KEYS, 6, KeyId::PANEL_KNOB_UP, KeyId::PANEL_KNOB_DOWN},
};

// Frames 336, 3B6, 2B6 carry the VIN: overwritten in published captures (../tools/sanitize_capture.py).
const uint16_t PRIVACY_IDS[] = {0x336, 0x3B6, 0x2B6};

// Default radio key remaps (XP codes; owner's choice for this car with the radio profile XP Simple Soft ->
// 508 -> Type 2, tests in the car on 2026-11-16; what the codes do: lib/radio_xp/xp_keys_custom.h).
// They belong to this profile because the buttons are this car's: another car may not have them.
// Order = order in the remap table.
const vehicle::KeyRemapDefault KEY_REMAP_DEFAULTS[] = {
    {0x9F, 0x9B},    // KeyId::TEL (panel TEL) -> 9B phone (9F did nothing without a connection)
    {0x21, 0x1F},    // KeyId::SOURCE (frame 0A2 byte 2 bit 2, "the button between SRC and LIST") -> 1F voice assistant (Siri)
};

}  // namespace

const vehicle::CarProfile kCarPsaAee2010 = {
    "psa_aee2010",
    125000,                                   // INFO-DIV 125 kbit/s (measured 10.11.2026)
    // UNKNOWN on purpose: the line levels measured on the radio connector look like low-speed fault-tolerant,
    // but the bus type is unverified until an oscilloscope check (vehicles/peugeot-508-sw-2015/README.md).
    vehicle::PhysicalLayer::UNKNOWN,
    KEYS, (uint8_t)(sizeof(KEYS) / sizeof(KEYS[0])),
    KNOBS, (uint8_t)(sizeof(KNOBS) / sizeof(KNOBS[0])),
    psa::TX_ALLOW, psa::TX_ALLOW_COUNT,       // transmit allow list (tx_allowlist.cpp)
    PRIVACY_IDS, (uint8_t)(sizeof(PRIVACY_IDS) / sizeof(PRIVACY_IDS[0])),
    psa::decodeParkingAid,
    psa::decodeBodyState,
    psa::decodeLightsOn,
    true,                                     // ignition from frame 0F6 drives ACC / illumination / reverse
    KEY_REMAP_DEFAULTS, (uint8_t)(sizeof(KEY_REMAP_DEFAULTS) / sizeof(KEY_REMAP_DEFAULTS[0])),
    // car data for the radio screen (psa_signals.h)
    psa::decodeSteering,                      // 10B
    psa::decodeTrip,                          // 221, 2A1, 261
    psa::decodeOutsideTemp,                   // 0F6 byte 7
    psa::decodeEcoMode,                       // 036 byte 3 bit 7
    psa::decodeClimate,                       // 350, 2AD
    psa::decodeDoors,                         // 1A1, 128
    psa::decodeSettings,                      // 260, 227, 3E1, 217, 361
    "xp",                                     // KEY_REMAP_DEFAULTS are XP key codes
};
