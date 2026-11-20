// Persistent settings in the chip's NVS memory (Preferences library): they survive a restart and flashing
// a new program over Wi-Fi. Flashing over USB with the "upload" command does not erase them either
// (the NVS area at address 0x9000 is not written then). They are erased only by flashing the merged
// merged image (all partitions) from address 0, because that file also covers the NVS area.
//
// What is stored:
//   - device password: one password for the device's Wi-Fi network and for the web panel;
//   - home network (name and password), if the device is to connect to it;
//   - button remap table as text ("9F=9B;21=1F", format in lib/radio_xp/xp_keys_custom.h);
//   - beep volume (stored only from AUDIO_VOLUME_MIN_SAVED upwards, enforced by main.cpp:
//     sensor beeping must not be muted permanently by a single "vol 0" command);
//   - network switch (commands "wifi off" / "wifi on");
//   - consent to changes from the home network (command "wifi lan on|off"), off by default;
//   - CAN transmit arming (command "cantx on"): the build string of the program in which it was
//     enabled; valid only for the same program (lib/core/can_tx_arm.h). None by default;
//   - mask of enabled vehicle settings items (commands "vset on|off NN"), by default only item 01;
//   - web panel language (command "lang", POST /api/lang), by default none = English.
// The full list of settings and where each one lives (NVS, RAM only, browser): docs/en/reference/settings.md.
//
// The module is compiled in ALL environments: the button table and volume are to be remembered
// also without network. Wi-Fi related functions are simply not called in the program without network.
//
// Usage rules:
//   - Read: from any task. Every function copies the value under the module's own static lock
//     (held for the time of copying at most approx. 100 bytes), so nothing returns a pointer to the inside.
//   - Write: ONLY from the main loop (also the password generated at the first network start: the
//     net_wifi task turns on the radio and signals it, and wifiMgrPoll() in the main loop generates and saves it).
//     The module lock is NOT held during the NVS write (only while copying to RAM), so that
//     readers from other tasks do not wait for the flash memory.
//     First the write to NVS, only after a successful write the value in RAM changes; on a write
//     error the old value still applies (result STORE_FAILED).
//   - A write to NVS is a write to flash memory: it usually takes a few ms, during page compaction up to
//     approx. 100 ms (not measured on this hardware). During that time CAN frame reception stops (the CAN
//     driver interrupt does not run during a flash write). Hence writes only on explicit user request,
//     never periodically, and (for panel requests) not when the car needs the device
//     (enforced by net.cpp). Writing a value equal to the stored one writes nothing.
//   - The lock of this module must not be held together with the src/net/diag_hub lock; the module's functions
//     release it before returning, so it is enough not to call them from under someone else's lock.
//
// The file does not depend on Arduino. Implementation: src/settings.cpp.
//
// NVS layout (for tools and for whoever writes settings.cpp): namespace PROJECT_NVS_NAMESPACE (limit 15 characters),
// keys: "pass" text, "sta_ssid" text, "sta_pass" text (always 8..63 characters or 64 hex digits: open networks are
// rejected), "keymap" text, "vol" number 0..255 (putUChar), "net_on" 0/1 (putUChar),
// "lan_admin" 0/1 (putUChar), "cantx" text (build string, at most 23 characters),
// "vsetmask" 32-bit number (putULong), "ui_lang" text (language code, 2..8 characters). An empty button table is stored as the text "-",
// because Preferences::putString returns 0 both for an empty text and on error.
// The Clear functions first check isKey(): missing key = result OK without calling remove()
// (Preferences::remove returns false for a key that does not exist).
// settingsBegin: a stored "pass" that does not satisfy webauth::passwordAcceptable (e.g. empty or
// corrupted) is treated as no password (the network will generate a new one); likewise "sta_ssid"/"sta_pass"
// not satisfying settingsStaAcceptable give "home network not stored".
// Open the namespace in write mode (begin(PROJECT_NVS_NAMESPACE, false)): in read-only mode opening
// a namespace that does not exist yet ends with an error.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "settings_backup.h"
#include "web_auth.h"
#include "xp_keys_custom.h"

const size_t SET_SSID_MAX = 32;          // longest Wi-Fi network name (bytes, without the terminating zero)
const size_t SET_STA_PASS_MAX = 64;      // home network password: 8..63 characters or 64 hex digits
// Length of the password generated at first startup: 12 characters from a 32-character alphabet
// (lowercase letters without 'l' and 'o', digits 2..9), i.e. 60 bits. Without characters that are easy to confuse
// when copying from the console to a phone.
const size_t SET_PASSWORD_GEN_LEN = 12;

static_assert(SET_PASSWORD_GEN_LEN >= webauth::PASSWORD_MIN && SET_PASSWORD_GEN_LEN <= webauth::PASSWORD_MAX,
              "random password length out of the allowed range");

enum class SetResult : uint8_t {
    OK = 0,
    INVALID,        // value does not satisfy the rules; nothing was written
    STORE_FAILED,   // NVS refused the write; the old value applies
};

// ---------- startup ----------

// Call once in setup() from the main loop, before starting the drivers that use the settings.
// Creates the lock and loads all values into RAM. Blocks for the duration of the NVS read (a few ms).
// false = NVS unavailable: the module keeps working on default values, reads return "none",
// writes return STORE_FAILED. The program is then to work normally (no restart loop).
bool settingsBegin();

// ---------- read (any task, does not block longer than copying) ----------

// Device password. false = there is no valid password (first startup, "wifi newpass" with the radio
// off, NVS unavailable); then out[0] = 0. With true the password always satisfies webauth::passwordAcceptable.
// 'cap' at least webauth::PASSWORD_MAX + 1 (64); a smaller buffer gives false.
// The result must not be printed other than through conUsbPrintf nor put into an HTTP response.
bool settingsGetDevicePassword(char* out, size_t cap);

// Home network. false = none stored (then the buffers get empty texts).
// 'ssidCap' at least SET_SSID_MAX + 1, 'passCap' at least SET_STA_PASS_MAX + 1.
// The password must not be printed or sent.
bool settingsGetSta(char* ssid, size_t ssidCap, char* pass, size_t passCap);
bool settingsGetStaSsid(char* ssid, size_t ssidCap);   // name only, to show in the status
bool settingsStaConfigured();

// Button remap table as text. false = never stored (the default entries of the car profile apply,
// vehicle::CarProfile::keyRemapDefaults). true with an empty text = a table without remaps was stored.
// 'cap' at least xp::KeyCustomizer::MAP_TEXT_MAX (96).
bool settingsGetKeyMap(char* out, size_t cap);

// Beep volume in percent; 'fallback' is returned when nothing is stored (pass AUDIO_VOLUME_DEFAULT).
uint8_t settingsVolume(uint8_t fallback);

// User network switch. Default true (network on).
bool settingsNetEnabled();

// Consent to state-changing requests from clients on the home network (http_server.h, HttpAccess::WRITE).
// Default false: from the home network the panel only reads, changes only from the device's own network.
bool settingsLanAdmin();

// CAN transmit arming: the string stored by the "cantx on" command. false = nothing stored (out[0] = 0).
// 'cap' at least SET_CANTX_TAG_MAX. Whether the string arms THIS program is decided by cantx::armTagValid.
const size_t SET_CANTX_TAG_MAX = 24;     // like the FwIdent::build field, including the zero
bool settingsGetCanTxTag(char* out, size_t cap);

// Mask of enabled vehicle settings items (bit = item number); 'fallback' when nothing is stored.
uint32_t settingsVsetMask(uint32_t fallback);

// Panel language code (lib/core/ui_lang.h). false = nothing stored (out[0] = 0): the panel uses English.
// 'cap' at least SET_UI_LANG_CAP. The code is well formed; whether this image has that dictionary is checked
// by the reader against WEB_UI_LANGS (src/net).
const size_t SET_UI_LANG_CAP = 9;        // uilang::CODE_MAX + 1 (static_assert in settings.cpp)
bool settingsGetUiLang(char* out, size_t cap);

// Backup export (lib/core/settings_backup.h), any task: the stored panel language, volume and key table
// (bit in v.present only when stored) and the network switches. No passwords, no home network, no CAN transmit
// settings. The caller drops a language that is not in this image's list before formatting.
void settingsExport(backup::Values& v);

// ---------- value checks (pure functions, any task) ----------

// Home network: name 1..32 bytes without control characters (below 0x20); password MANDATORY, 8..63 printable
// ASCII characters or 64 hex digits (a ready WPA2 key). An open network cannot be stored: the device would connect to any access
// point with that name. The device password is checked by webauth::passwordAcceptable.
bool settingsStaAcceptable(const char* ssid, const char* pass);
// Button table text in the xp::KeyCustomizer::parseMap format (the same check as settingsSetKeyMap, nothing
// written). MAIN LOOP ONLY: it uses the module's single trial table instance.
bool settingsKeyMapAcceptable(const char* text);

// ---------- write (main loop ONLY; every function may stop CAN reception for the duration of the flash write) ----------

// Device password: rules of webauth::passwordAcceptable (10..63 printable ASCII characters without spaces).
// After a change the caller must restart the Wi-Fi network (wifiMgrReconfigure), otherwise the network
// still accepts the old password, while the panel already the new one.
SetResult settingsSetDevicePassword(const char* pass);
// Removes the password: at the next Wi-Fi start a new one will be generated. Allowed only with the radio
// OFF (command "wifi newpass"; with the radio running that command immediately stores a new password).
SetResult settingsClearDevicePassword();

SetResult settingsSetSta(const char* ssid, const char* pass);   // rules of settingsStaAcceptable
SetResult settingsClearSta();                                   // forget the home network

// 'text' in the xp::KeyCustomizer::parseMap format, at most MAP_TEXT_MAX - 1 characters. The function checks
// the text with a trial parseMap on its own table instance; an invalid text gives INVALID.
SetResult settingsSetKeyMap(const char* text);
SetResult settingsClearKeyMap();                                // back to the car profile defaults

SetResult settingsSetVolume(uint8_t percent);                   // 0..100; the limit to AUDIO_VOLUME_MAX is applied by audio_out
SetResult settingsClearVolume();                                // back to the build default (missing key = OK)
SetResult settingsSetNetEnabled(bool on);
SetResult settingsSetLanAdmin(bool on);
// Transmit arming: 'build' = build string of the running program (1..23 printable characters).
SetResult settingsSetCanTxTag(const char* build);
SetResult settingsClearCanTxTag();                              // disarming (missing key = OK)
SetResult settingsSetVsetMask(uint32_t mask);
// Panel language: 'code' must be uilang::wellFormed (otherwise INVALID); the caller checks it is one of the
// image's dictionaries. Equal to the stored value = no flash write. Clear = back to English (missing key = OK).
SetResult settingsSetUiLang(const char* code);
SetResult settingsClearUiLang();

// ---------- password generation ----------

// Generates a password of length SET_PASSWORD_GEN_LEN into 'out' (zero terminated); writes nothing.
// 'cap' at least SET_PASSWORD_GEN_LEN + 1, otherwise false.
// Who calls: the main loop (wifiMgrPoll in step 1 of the network start, command "wifi newpass"), and ONLY
// when the Wi-Fi radio is already running: esp_fill_random
// gives truly random numbers only with the radio on. Do not use bootloader_random_enable
// (it conflicts with the ADC converter used to measure the battery voltage).
// Each character from a separate random byte: the 5 low bits select a character from the 32-character alphabet
// "abcdefghijkmnpqrstuvwxyz23456789" (no rejection, uniform distribution).
bool settingsGeneratePassword(char* out, size_t cap);
