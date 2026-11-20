// Settings backup: export and import of the user settings as one flat JSON object, without passwords.
//
//   {"v":1,"lang":"pl","vol":40,"keys":"9F=9B;21=1F","net":true,"lan":false}
//
//   v     format version, always 1. Optional on import; another number = error VERSION.
//   lang  panel language code (ui_lang.h) or null = nothing stored (English).
//   vol   beep volume in percent 0..100 or null = nothing stored (the build default). Whether a value may be
//         stored (the device stores only from AUDIO_VOLUME_MIN_SAVED up) is checked by the caller.
//   keys  button remap table in the xp::KeyCustomizer::parseMap format, or null = nothing stored (the defaults
//         of the car profile). "" = a stored table without remaps. Its content is checked by the caller
//         (the trial parseMap lives in lib/radio_xp, which this layer does not know).
//   net   network switch ("wifi on|off"), information only: an import never changes it.
//   lan   consent to changes from the home network ("wifi lan on|off"), information only: an import never
//         changes it. Both decide who can reach the device, so a pasted text must not change them.
//
// NOT in the backup, on purpose: the device password and the home network (secrets; the home network name
// alone cannot be restored without its password), CAN transmit arming and the vehicle settings item
// mask (transmit domain: only the explicit commands "cantx on" and "vset on|off NN" change them).
//
// Import rules (parse): a field that is absent = unchanged; a field with null = back to the default;
// unknown field, duplicate field, wrong type or value out of range = error and NOTHING is applied. Strings
// carry printable ASCII only, without escapes (a backslash is an error). The text may hold any subset of
// the fields, so a long table can be imported in its own line: {"keys":"..."}.
//
// Pure functions, no hardware, no allocation. Test: test_host/settings_backup.cpp.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "json_writer.h"
#include "ui_lang.h"

namespace backup {

const uint8_t FORMAT_VERSION = 1;
// Longest remap table text without the terminating zero: 16 pairs "ZZ=NA" joined with ';' (95 characters).
// Equal to xp::KeyCustomizer::MAP_TEXT_MAX - 1 (checked with static_assert in src/settings.cpp).
const size_t KEYS_MAX = 95;
// Longest export text including the zero:
// {"v":1,"lang":"12345678","vol":100,"keys":"<95>","net":false,"lan":false} = 164 characters.
const size_t TEXT_MAX = 176;

// Field bits (Values::present, Values::isNull).
const uint8_t F_LANG = 0x01;
const uint8_t F_VOL = 0x02;
const uint8_t F_KEYS = 0x04;
const uint8_t F_NET = 0x08;
const uint8_t F_LAN = 0x10;

struct Values {
    uint8_t present = 0;                    // fields given (import) / stored (export)
    uint8_t isNull = 0;                     // fields given as null (import: back to the default)
    char lang[uilang::CODE_MAX + 1] = {0};
    uint8_t vol = 0;
    char keys[KEYS_MAX + 1] = {0};
    bool net = true;
    bool lan = false;
};

enum class ParseError : uint8_t {
    OK = 0,
    SYNTAX,        // not one flat JSON object (or text after the closing brace)
    VERSION,       // "v" is not 1
    UNKNOWN_KEY,   // a field name not listed above
    DUPLICATE,     // the same field twice
    BAD_VALUE,     // wrong type, out of range, too long, character not allowed
};

// Reads 'len' bytes of 'text' (need not be zero-terminated). On any error 'out' is left as {} (nothing given).
ParseError parse(const char* text, size_t len, Values& out);

// Word for messages and the API ("syntax", "version", "unknown_key", "duplicate", "bad_value", "ok").
const char* errorName(ParseError e);

// Export: lang / vol / keys as values when their bit is in 'present' and not in 'isNull', otherwise null;
// net and lan always. false = the buffer is too small (cap at least TEXT_MAX is always enough).
bool format(const Values& v, char* out, size_t cap);
// The same object written into an existing writer (e.g. for an HTTP answer); result = j.ok().
bool write(const Values& v, JsonWriter& j);

}  // namespace backup
