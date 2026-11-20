// Deliberate deviations from the Simple Soft box for buttons (design decisions, 2026-11-16).
// KeyEncoder (xp_keys) stays a faithful copy of the box and is checked against recordings;
// here the events are adjusted just before they are sent to the radio.
//
// How the radio treats a key frame (status of this rule: radios/xp-simple-soft/provenance.md):
//   state 1 = "key pressed", state 0 = "key released", the radio ignores state 2.
//
// 1. Code remapping table (code from the box -> code sent to the radio). The default entries come from the
//    car profile (vehicle::CarProfile::keyRemapDefaults, set with setDefaults()); this class has none of its
//    own, so a profile without defaults (generic, example) sends every key with its own code.
//    The reference profile PSA AEE2010 (lib/car_psa_aee2010/profile_psa_aee2010.cpp) has two, from tests in
//    the car on 2026-11-16 (profile XP Simple Soft -> 508 -> Type 2):
//      9F (KeyId::TEL, TEL on the panel) -> 9B: opens the phone (9F did nothing without a connection);
//      21 (KeyId::SOURCE; frame 0A2 byte 2 bit 2, called "the button between SRC and LIST" in the notes of
//         that test) -> 1F: voice assistant (Siri in CarPlay).
//    Further remappings can be added from the console (command "map") or the web panel; they are stored
//    in the settings (src/settings.h) and come back after a restart. "map reset" restores the defaults of
//    the car profile.
//    What the codes do in the radio (tests in the car 2026-11-16): 08 source change, 1F voice assistant
//    (Siri in CarPlay), 9A screen brightness, A5 climate screen, A6 back, B5 FM/AM,
//    B9 and BF tuning down by 0.05 MHz, 30 source change, 9B phone; B1, 91-96 no reaction.
// 2. Keys with a latch (RadioProtocol::latchKeys, a property of the radio protocol): a press toggles, the first
//    press sends only "pressed", the second only "released". XP: moon (A4, function 69), because the radio
//    turns on the screen blanker on "pressed" and returns on "released" (observation in the car 2026-11-16).
#pragma once
#include <stddef.h>

#include "car_profile.h"
#include "protocol_xp.h"

namespace xp {

const uint8_t KEY_PANEL_MOON = 0xA4;

class KeyCustomizer {
public:
    // Empty table, no defaults. The Decoder calls setDefaults() with the table of its car profile.
    KeyCustomizer();

    // Default entries (vehicle::CarProfile::keyRemapDefaults; nullptr / 0 = none). The table is kept as a
    // pointer (const data of the profile, never copied or freed), at most MAP_SIZE rows are used.
    // Also calls resetMap(): the current table becomes the defaults.
    void setDefaults(const vehicle::KeyRemapDefault* table, uint8_t count);

    // Returns true when 'out' must be sent to the radio; false = event skipped.
    bool onEvent(const KeyEvent& in, KeyEvent& out);
    // Ignition off: if a latch key is latched (XP: the screen was blanked with the moon button), returns
    // "released" for it, so that after the next start the radio does not have a key remembered as pressed.
    // One key per call: call again until it returns false.
    bool onIgnitionOff(KeyEvent& out);

    bool screenOff() const { return latched_ != 0; }    // any latch key latched

    // Radio codes with a latch: the table of the radio protocol (RadioProtocol::latchKeys; Decoder). nullptr / 0
    // = no latch, every code goes through the remap table. At most LATCH_MAX codes are used. Default: A4 (XP).
    // Clears the latch state.
    void setLatchKeys(const uint8_t* codes, uint8_t count);
    static const uint8_t LATCH_MAX = 4;

    // Remap code 'from' to 'to'. to == from removes the remapping. false = no room in the table.
    bool setMap(uint8_t from, uint8_t to);
    uint8_t mapped(uint8_t code) const;

    static const uint8_t MAP_SIZE = 16;
    struct Entry {
        bool used = false;
        uint8_t from = 0, to = 0;
    };
    const Entry& entry(uint8_t i) const { return map_[i]; }

    // --- remapping table as text (web panel, settings storage, command "map") ---
    // Format: pairs "FROM=TO" separated by semicolons, each code exactly two hex digits,
    // e.g. "9F=9B;21=1F" (the button with code 9F sends 9B to the radio). The text describes the WHOLE table,
    // including the default entries of the car profile; an empty text means a table without any remapping.
    // Note: a remapping for a latch key code (XP: A4, moon) has no effect, because that key is handled separately.

    // Buffer for the text of the full table including the terminator: 16 entries of 5 characters, 15 semicolons and the zero.
    static const size_t MAP_TEXT_MAX = MAP_SIZE * 6;

    // Writes the table to 'out': upper case, entries in table order, no trailing semicolon,
    // always zero-terminated. An empty table gives an empty text. false = 'cap' too small (then out[0] = 0,
    // provided cap > 0); a buffer of size MAP_TEXT_MAX is always enough.
    bool serializeMap(char* out, size_t cap) const;

    // Replaces the whole table with the content of the text ('len' characters; a terminating zero is not required).
    // Accepts lower case and one trailing semicolon; whitespace is an error. An entry "XX=XX" is skipped
    // (means: no remapping). false = syntax error, more than MAP_SIZE entries, or two entries with the
    // same button code; the table then stays UNCHANGED. The moon state is not touched.
    bool parseMap(const char* text, size_t len);

    // Restores the default entries (the ones given to setDefaults(); none without it) and removes the rest.
    void resetMap();

    // Default entries, to be shown in the panel as "default". defaultEntry(i) for i >= defaultCount()
    // returns an entry with used = false.
    uint8_t defaultCount() const { return defaultCount_; }
    Entry defaultEntry(uint8_t i) const;

    uint8_t mapCount() const;      // number of entries in use (0..MAP_SIZE)

private:
    uint8_t latch_[LATCH_MAX] = {KEY_PANEL_MOON, 0, 0, 0};
    uint8_t latchCount_ = 1;
    uint8_t latched_ = 0;          // bit i = latch_[i] is latched
    Entry map_[MAP_SIZE];
    const vehicle::KeyRemapDefault* defaults_ = nullptr;
    uint8_t defaultCount_ = 0;
};

}  // namespace xp
