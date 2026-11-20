// Vehicle settings (personalisation) of the PSA AEE2010 car: the language table, the settings item table
// and the pure logic of the setting change frames 15B / 1A9 (used by settings_changer.h).
// Reading the settings for the radio is psa::decodeSettings (psa_signals.h) -> car-neutral
// vehicle::VehicleSettings; the XP frames 38 / 3C are built from it in lib/radio_xp/xp_signals.h.
// The former XP-format functions update38 / features361 are kept as a reference for the tests in
// lib/app/xp_settings.h.
//
// CAN bytes in comments are counted from 1, bits 7..0 (notation "5.7" = byte 5 bit 7), as in settings-map.md.
// No hardware and no Arduino framework header.
#pragma once
#include <stdint.h>

#include "car_state.h"

namespace vset {

// Language code of the car (frame 260 bits 1.6-1.2) -> position in the reference radio's language list
// (vehicle::SettingId::LANGUAGE). Only Polish is in the table (measured). 0xFF = code not in the table
// (the previous value stays).
uint8_t radioLanguage(uint8_t psa);

// ====================== Settings table and pure logic of frame 15B ======================
// Everything below is hardware-free functions: they compute bytes and transmit nothing. The program has
// no function that transmits on CAN (tools/check_no_can_tx.sh). build15B is called by the tests, the
// console command "vset" (Decoder::formatSettingRow) and by SettingsChanger (settings_changer.h),
// which in a build without transmit hardware ends every command with a refusal.
// Table and groups: numbers and names recorded 2026-11-19, 15B fields 2026-11-17 / 18 (sources per constant:
// radios/xp-simple-soft/provenance.md).

enum class Kind : uint8_t {
    SENDABLE,        // groups A-C: field of frame 15B known; sendable when enabled in the mask and the car has the feature
    FIXED_REFUSE,    // group D: field known only from a description, or the car lacks the feature; refusal fixed in code
    KNOWN_BLOCKED,   // 80 05, 80 0C: "button" in frame 1A9 (toggles the state); since 2026-11-18 sendable through buildButton
    NO_FRAME,        // group E: no known frame
};

struct SettingDef {
    uint8_t id;          // number from radio command 80
    const char* name;    // text from the radio screen without Polish diacritics (console)
    Kind kind;
    uint8_t minValue;    // smallest command value (for 09: 1, the radio sends 1-4)
    uint8_t maxValue;    // largest command value
    // Bit of frame 361 "the car has this feature": featByte = byte number from 1 (0 = assignment unknown,
    // no condition), featMask = bit mask. Assignments from radios/xp-simple-soft/provenance.md.
    uint8_t featByte;
    uint8_t featMask;
    const char* note;    // "measured" (measured) / "state 17.11" (state 2026-11-17) / "code+description" (code+description) / "description only" (description only) / "frame 1A9" (frame 1A9) / "no frame" (no frame)
};
const SettingDef* find(uint8_t id);   // nullptr = number not in the table
uint8_t count();
const SettingDef& at(uint8_t i);      // i < count()

// Mask of enabled items: the bit whose number equals the item number (item 01 = bit 1).
// By default only the measured item (parking assistant). Applies only to SENDABLE items.
const uint32_t ENABLED_DEFAULT = 1UL << 0x01;
// Mask of all items that can be enabled at all (Kind::SENDABLE). No mask enables items of groups D
// and E: the refusal is a constant in code.
uint32_t sendableMask();
// Mask read from storage: only the bits of SENDABLE items are kept.
inline uint32_t sanitizeMask(uint32_t stored) { return stored & sendableMask(); }
// Command "vset on NN" / "vset off NN": new mask in 'out'. For any result other than OK, 'out' = 'mask'.
enum class MaskResult : uint8_t {
    OK,
    UNKNOWN_ID,      // number not in the table
    NOT_SENDABLE,    // item from group D or E: it cannot be enabled
};
MaskResult maskChange(uint32_t mask, uint8_t id, bool on, uint32_t& out);
const uint32_t FRESH_260_MS = 1500;   // 260 arrives every 500 ms: older than 1.5 s = we do not assemble 15B

enum class Refuse : uint8_t {
    OK,
    UNKNOWN_ID,            // number not in the table
    NO_FRAME_FOR_ID,       // Kind::NO_FRAME
    BLOCKED_OTHER_FRAME,   // Kind::KNOWN_BLOCKED: frame known (1A9), transmission blocked until measured
    FIXED_REFUSE,          // Kind::FIXED_REFUSE
    DISABLED_BY_MASK,
    BAD_VALUE,             // value outside minValue..maxValue
    NO_361,                // the item has a condition from frame 361, and that frame has not arrived yet or is too short
    NO_FEATURE,            // "the car lacks the feature": bit of frame 361 for this item = 0
    NO_260,
    STALE_260,             // 260 older than FRESH_260_MS
    BAD_LEN_260,           // length other than 7
    INVALID_260,           // 2.2 = 0 or byte 7 != 0 or bits 6.1-6.0 != 0 (a state never seen in recordings)
    NO_STATE,              // button item (1A9): no fresh state frame 227 from the bus
};
const char* refuseName(Refuse r);     // short console text, no Polish diacritics

// Pure function: bytes of frame 15B for one radio command (80 id value).
// Formula (settings-map.md, section "Changing a setting (measured)"): 00, b2..b5 from frame 260, b6 & F4, 00, 00
// and one field changed. Byte 1 = 00 (language and units validity bit cleared), bit 6.3 (tyre calibration
// request) always 0. On refusal 'out' is zeroed.
Refuse build15B(const RawFrame& f260, const RawFrame& f361, uint32_t nowMs, uint8_t id, uint8_t value,
                uint32_t enabledMask, uint8_t out[8]);
// Mask of bits of 260 / 15B (bytes 2..6, mask[0] = byte 2) that the command (id) may change.
// All zeros for items other than SENDABLE.
void fieldMask(uint8_t id, uint8_t mask[5]);
// Item state read from frame 260, in radio command units (e.g. 09: 1-4; 0F: 1 = all
// doors, i.e. bit 3.6 = 0; 06 / 07: 0 = off, 1-3 = duration). false = no frame 260 of length 7
// or the item has no field in 260.
bool stateOf(const RawFrame& f260, uint8_t id, uint8_t& value);
// Whether 260 already shows the state requested by the command (id, value). SENDABLE items only; handles the
// inverted field (0F) and multi-bit fields (06, 07: for value 0 only the enable bit counts).
bool accepted(const RawFrame& f260, uint8_t id, uint8_t value);

// ====================== Button items: frame 1A9 (design decision 2026-11-18) ======================
// Two items of the radio screen have no field in 15B: in the car they are toggled by "pressing a screen button",
// i.e. ONE frame 1A9 with one bit. The frame TOGGLES the state (does not set it), so before sending
// the state from the bus must be known, and nothing is sent when the car already has what the radio wants. No repetitions.
//   80 05 parking sensors:     1A9 = 00 7F FF 04 FF 00 10 00, state 227 byte 1 bit 6
//         (measurement: Simple Soft box recording from 2026-11-10, 4 presses and 4 reactions of the car);
//   80 0C blind spot:          1A9 = 00 7F FF 20 FF 00 10 00, state 227 byte 3 bit 0 (same recording).
// 80 0D (Start/Stop) is NOT a button item: no 1A9 content for it was ever recorded on this car (Kind::NO_FRAME).
// Its state (3E1 byte 4) is only read for the radio screen (psa::decodeSettings).
// The "state" is computed the same way as this item in the radio data (psa::decodeSettings, psa_signals.h), so the value
// from the radio command is compared with it directly.
const uint32_t ID_BUTTONS = 0x1A9;
const uint32_t FRESH_227_MS = 1500;   // 227 every 500 ms
bool isButton(uint8_t id);            // item of Kind::KNOWN_BLOCKED
// State of a button item from frame 227. false = no fresh frame from the bus (a pasted one does not count).
bool buttonState(const CarState& car, uint32_t nowMs, uint8_t id, uint8_t& value);
// Bytes of frame 1A9 for a button item. Refusals: UNKNOWN_ID, FIXED_REFUSE (the item is not
// a button), DISABLED_BY_MASK, BAD_VALUE, NO_STATE. On refusal 'out' is zeroed.
Refuse buildButton(const CarState& car, uint32_t nowMs, uint8_t id, uint8_t value, uint32_t enabledMask,
                   uint8_t out[8]);

}  // namespace vset
