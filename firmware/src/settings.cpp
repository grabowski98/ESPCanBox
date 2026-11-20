// Persistent settings in NVS (rules and key layout: settings.h).
//
// All values are kept in RAM (a copy of what is in NVS). A read copies from RAM,
// a write goes first to NVS and only after a successful write changes the copy in RAM.
//
// Module lock: a portMUX critical section (spinlock), not a mutex. Held only for the time of
// copying at most 65 bytes (a fraction of a microsecond), so the main loop never really waits,
// even when the web task on the other core reads at the same moment. A mutex could be taken by
// the web task while it is preempted by Wi-Fi tasks, and then the main loop would stall for milliseconds.
// Inside the critical section it is NOT allowed to call NVS or anything that waits: copying only.
#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>
#include <ctype.h>
#include <string.h>

#include "esp_random.h"   // esp_fill_random (Arduino.h does not include it)
#include "project_identity.h"
#include "settings_backup.h"
#include "ui_lang.h"

static const char* NS = PROJECT_NVS_NAMESPACE;   // NVS namespace (at most 15 characters, project.json)
static const char* K_PASS = "pass";
static const char* K_STA_SSID = "sta_ssid";
static const char* K_STA_PASS = "sta_pass";
static const char* K_KEYMAP = "keymap";
static const char* K_VOL = "vol";
static const char* K_NET_ON = "net_on";
static const char* K_LAN_ADMIN = "lan_admin";
static const char* K_CANTX = "cantx";         // build string of the program in which "cantx on" was entered
static const char* K_VSETMASK = "vsetmask";   // mask of vehicle settings items
static const char* K_UI_LANG = "ui_lang";     // web panel language code (lib/core/ui_lang.h)
// Storing an empty button table: Preferences::putString returns 0 for an empty text and on error.
static const char* KEYMAP_EMPTY = "-";

static const size_t PASS_CAP = webauth::PASSWORD_MAX + 1;              // 64
static const size_t KEYMAP_CAP = xp::KeyCustomizer::MAP_TEXT_MAX;     // 96
static_assert(backup::KEYS_MAX == KEYMAP_CAP - 1, "the backup text must hold the longest key table");
static_assert(SET_UI_LANG_CAP == uilang::CODE_MAX + 1, "language buffer size");

static Preferences s_prefs;     // used only by the main loop (settingsBegin and writes)
static bool s_nvsOk = false;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

// Copy of the settings in RAM; changed only under s_mux, read only under s_mux.
static char s_pass[PASS_CAP] = "";
static bool s_havePass = false;
static char s_staSsid[SET_SSID_MAX + 1] = "";
static char s_staPass[SET_STA_PASS_MAX + 1] = "";
static bool s_haveSta = false;
static char s_keymap[KEYMAP_CAP] = "";
static bool s_haveKeymap = false;
static uint8_t s_vol = 0;
static bool s_haveVol = false;
static bool s_netOn = true;
static bool s_lanAdmin = false;
static char s_canTxTag[SET_CANTX_TAG_MAX] = "";
static bool s_haveCanTxTag = false;
static uint32_t s_vsetMask = 0;
static bool s_haveVsetMask = false;
static char s_uiLang[SET_UI_LANG_CAP] = "";
static bool s_haveUiLang = false;

// Table instance for the trial parseMap (text check before saving). Main loop only.
static xp::KeyCustomizer s_trialMap;

// ---------- helpers ----------

// Bounded text copy: false when it does not fit (then out = "" provided cap > 0).
static bool copyText(char* out, size_t cap, const char* src) {
    if (out == nullptr || cap == 0) return false;
    size_t n = strlen(src);
    if (n >= cap) {
        out[0] = 0;
        return false;
    }
    memcpy(out, src, n + 1);
    return true;
}

// Clearing a buffer holding a secret in a way the compiler does not optimise away.
static void wipe(char* p, size_t n) {
    volatile char* v = p;
    for (size_t i = 0; i < n; i++) v[i] = 0;
}

// Text from NVS into 'buf' (capacity 'cap' including the zero). false = missing key or does not fit.
static bool loadText(const char* key, char* buf, size_t cap) {
    buf[0] = 0;                                   // getString does not touch the buffer when the key is missing
    if (!s_prefs.isKey(key)) return false;
    size_t got = s_prefs.getString(key, buf, cap);
    if (got == 0) {
        buf[0] = 0;
        return false;
    }
    buf[cap - 1] = 0;
    return true;
}

// Text write: true only when NVS accepted the whole text. The text must not be empty (see KEYMAP_EMPTY).
static bool storeText(const char* key, const char* value) {
    size_t n = strlen(value);
    return n > 0 && s_prefs.putString(key, value) == n;
}

// Key removal: missing key = OK (Preferences::remove returns false for a key that does not exist).
static bool clearKey(const char* key) {
    if (!s_prefs.isKey(key)) return true;
    return s_prefs.remove(key);
}

static bool staTextAcceptable(const char* ssid, const char* pass) {
    if (ssid == nullptr || pass == nullptr) return false;
    size_t ns = strlen(ssid);
    if (ns < 1 || ns > SET_SSID_MAX) return false;
    for (size_t i = 0; i < ns; i++)
        if ((uint8_t)ssid[i] < 0x20) return false;
    size_t np = strlen(pass);
    if (np < 8 || np > SET_STA_PASS_MAX) return false;
    for (size_t i = 0; i < np; i++)
        if ((uint8_t)pass[i] < 0x20 || (uint8_t)pass[i] > 0x7E) return false;
    // In WPA2, 64 characters are not a passphrase but a ready key written in hex: other characters would be
    // rejected by the Wi-Fi driver, so it is better to reject them already here.
    if (np == SET_STA_PASS_MAX)
        for (size_t i = 0; i < np; i++)
            if (!isxdigit((unsigned char)pass[i])) return false;
    return true;
}

// Button table text in the parseMap format (without saving). Empty text = table without remaps.
static bool keymapTextAcceptable(const char* text) {
    if (text == nullptr) return false;
    size_t n = strlen(text);
    if (n > KEYMAP_CAP - 1) return false;
    return s_trialMap.parseMap(text, n);
}

// ---------- startup ----------

bool settingsBegin() {
    // Write mode (false): in read-only mode a namespace that does not exist yet gives an error.
    s_nvsOk = s_prefs.begin(NS, false);
    if (!s_nvsOk) return false;   // we keep working on default values

    // Static working buffers: settingsBegin is called only by setup(), and passwords are not to sit on the stack.
    static char pass[PASS_CAP];
    static char ssid[SET_SSID_MAX + 1];
    static char spass[SET_STA_PASS_MAX + 1];
    static char keymap[KEYMAP_CAP];

    // Device password: corrupted or not matching the rules = none (the network will generate a new one).
    bool havePass = loadText(K_PASS, pass, sizeof(pass)) && webauth::passwordAcceptable(pass);
    if (!havePass) pass[0] = 0;

    bool haveSsid = loadText(K_STA_SSID, ssid, sizeof(ssid));
    bool haveSpass = loadText(K_STA_PASS, spass, sizeof(spass));
    bool haveSta = haveSsid && haveSpass && staTextAcceptable(ssid, spass);
    if (!haveSta) {
        ssid[0] = 0;
        spass[0] = 0;
    }

    // Button table: "-" = stored empty; a text that parseMap does not accept = as if not stored.
    bool haveKeymap = loadText(K_KEYMAP, keymap, sizeof(keymap));
    if (haveKeymap && strcmp(keymap, KEYMAP_EMPTY) == 0) keymap[0] = 0;
    if (haveKeymap && !keymapTextAcceptable(keymap)) haveKeymap = false;
    if (!haveKeymap) keymap[0] = 0;

    bool haveVol = s_prefs.isKey(K_VOL);
    uint8_t vol = haveVol ? s_prefs.getUChar(K_VOL, 0) : 0;
    if (vol > 100) haveVol = false;
    bool netOn = s_prefs.isKey(K_NET_ON) ? (s_prefs.getUChar(K_NET_ON, 1) != 0) : true;
    bool lanAdmin = s_prefs.isKey(K_LAN_ADMIN) ? (s_prefs.getUChar(K_LAN_ADMIN, 0) != 0) : false;
    // CAN transmit arming: a string longer than the field or unreadable = none.
    static char canTxTag[SET_CANTX_TAG_MAX];
    bool haveCanTxTag = loadText(K_CANTX, canTxTag, sizeof(canTxTag));
    if (!haveCanTxTag) canTxTag[0] = 0;
    bool haveVsetMask = s_prefs.isKey(K_VSETMASK);
    uint32_t vsetMask = haveVsetMask ? s_prefs.getULong(K_VSETMASK, 0) : 0;
    // Panel language: a text that is not a well-formed code = nothing stored (English). Whether the code
    // is one of the dictionaries of THIS image is checked by the reader (net code, list WEB_UI_LANGS).
    static char uiLang[SET_UI_LANG_CAP];
    bool haveUiLang = loadText(K_UI_LANG, uiLang, sizeof(uiLang)) && uilang::wellFormed(uiLang);
    if (!haveUiLang) uiLang[0] = 0;

    portENTER_CRITICAL(&s_mux);
    memcpy(s_canTxTag, canTxTag, sizeof(s_canTxTag));
    s_haveCanTxTag = haveCanTxTag;
    s_vsetMask = vsetMask;
    s_haveVsetMask = haveVsetMask;
    memcpy(s_uiLang, uiLang, sizeof(s_uiLang));
    s_haveUiLang = haveUiLang;
    memcpy(s_pass, pass, sizeof(s_pass));
    s_havePass = havePass;
    memcpy(s_staSsid, ssid, sizeof(s_staSsid));
    memcpy(s_staPass, spass, sizeof(s_staPass));
    s_haveSta = haveSta;
    memcpy(s_keymap, keymap, sizeof(s_keymap));
    s_haveKeymap = haveKeymap;
    s_vol = vol;
    s_haveVol = haveVol;
    s_netOn = netOn;
    s_lanAdmin = lanAdmin;
    portEXIT_CRITICAL(&s_mux);

    wipe(pass, sizeof(pass));
    wipe(spass, sizeof(spass));
    return true;
}

// ---------- read ----------

bool settingsGetDevicePassword(char* out, size_t cap) {
    if (out == nullptr || cap == 0) return false;
    out[0] = 0;
    if (cap < PASS_CAP) return false;
    bool ok;
    portENTER_CRITICAL(&s_mux);
    ok = s_havePass;
    if (ok) memcpy(out, s_pass, PASS_CAP);
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

bool settingsGetSta(char* ssid, size_t ssidCap, char* pass, size_t passCap) {
    if (ssid != nullptr && ssidCap > 0) ssid[0] = 0;
    if (pass != nullptr && passCap > 0) pass[0] = 0;
    if (ssid == nullptr || pass == nullptr || ssidCap < SET_SSID_MAX + 1 || passCap < SET_STA_PASS_MAX + 1)
        return false;
    bool ok;
    portENTER_CRITICAL(&s_mux);
    ok = s_haveSta;
    if (ok) {
        memcpy(ssid, s_staSsid, SET_SSID_MAX + 1);
        memcpy(pass, s_staPass, SET_STA_PASS_MAX + 1);
    }
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

bool settingsGetStaSsid(char* ssid, size_t ssidCap) {
    if (ssid == nullptr || ssidCap == 0) return false;
    ssid[0] = 0;
    if (ssidCap < SET_SSID_MAX + 1) return false;
    bool ok;
    portENTER_CRITICAL(&s_mux);
    ok = s_haveSta;
    if (ok) memcpy(ssid, s_staSsid, SET_SSID_MAX + 1);
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

bool settingsStaConfigured() {
    portENTER_CRITICAL(&s_mux);
    bool ok = s_haveSta;
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

bool settingsGetKeyMap(char* out, size_t cap) {
    if (out == nullptr || cap == 0) return false;
    out[0] = 0;
    if (cap < KEYMAP_CAP) return false;
    bool ok;
    portENTER_CRITICAL(&s_mux);
    ok = s_haveKeymap;
    if (ok) memcpy(out, s_keymap, KEYMAP_CAP);
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

uint8_t settingsVolume(uint8_t fallback) {
    portENTER_CRITICAL(&s_mux);
    uint8_t v = s_haveVol ? s_vol : fallback;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

bool settingsNetEnabled() {
    portENTER_CRITICAL(&s_mux);
    bool v = s_netOn;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

bool settingsLanAdmin() {
    portENTER_CRITICAL(&s_mux);
    bool v = s_lanAdmin;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

bool settingsGetCanTxTag(char* out, size_t cap) {
    if (out == nullptr || cap == 0) return false;
    out[0] = 0;
    if (cap < SET_CANTX_TAG_MAX) return false;
    bool ok;
    portENTER_CRITICAL(&s_mux);
    ok = s_haveCanTxTag;
    if (ok) memcpy(out, s_canTxTag, SET_CANTX_TAG_MAX);
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

uint32_t settingsVsetMask(uint32_t fallback) {
    portENTER_CRITICAL(&s_mux);
    uint32_t v = s_haveVsetMask ? s_vsetMask : fallback;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

bool settingsGetUiLang(char* out, size_t cap) {
    if (out == nullptr || cap == 0) return false;
    out[0] = 0;
    if (cap < SET_UI_LANG_CAP) return false;
    bool ok;
    portENTER_CRITICAL(&s_mux);
    ok = s_haveUiLang;
    if (ok) memcpy(out, s_uiLang, SET_UI_LANG_CAP);
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

void settingsExport(backup::Values& v) {
    v = backup::Values();
    portENTER_CRITICAL(&s_mux);
    if (s_haveUiLang) {
        memcpy(v.lang, s_uiLang, sizeof(v.lang));
        v.present |= backup::F_LANG;
    }
    if (s_haveVol) {
        v.vol = s_vol;
        v.present |= backup::F_VOL;
    }
    if (s_haveKeymap) {
        memcpy(v.keys, s_keymap, sizeof(v.keys));          // sizes equal (static_assert above)
        v.present |= backup::F_KEYS;
    }
    v.net = s_netOn;
    v.lan = s_lanAdmin;
    portEXIT_CRITICAL(&s_mux);
    v.present |= backup::F_NET | backup::F_LAN;
}

bool settingsStaAcceptable(const char* ssid, const char* pass) { return staTextAcceptable(ssid, pass); }

bool settingsKeyMapAcceptable(const char* text) { return keymapTextAcceptable(text); }

// ---------- write (main loop only) ----------

SetResult settingsSetDevicePassword(const char* pass) {
    if (pass == nullptr || !webauth::passwordAcceptable(pass)) return SetResult::INVALID;
    if (!s_nvsOk) return SetResult::STORE_FAILED;       // NVS unavailable: every write is an error
    portENTER_CRITICAL(&s_mux);
    bool same = s_havePass && strcmp(s_pass, pass) == 0;
    portEXIT_CRITICAL(&s_mux);
    if (same) return SetResult::OK;                     // without an unnecessary flash write
    if (!storeText(K_PASS, pass)) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    copyText(s_pass, sizeof(s_pass), pass);             // fits: checked by passwordAcceptable
    s_havePass = true;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

SetResult settingsClearDevicePassword() {
    if (!s_nvsOk || !clearKey(K_PASS)) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    wipe(s_pass, sizeof(s_pass));
    s_havePass = false;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

SetResult settingsSetSta(const char* ssid, const char* pass) {
    if (!staTextAcceptable(ssid, pass)) return SetResult::INVALID;
    if (!s_nvsOk) return SetResult::STORE_FAILED;
    // Old values (for comparison and for restoring after a failed second write).
    static char oldPass[SET_STA_PASS_MAX + 1];
    portENTER_CRITICAL(&s_mux);
    bool hadSta = s_haveSta;
    bool same = hadSta && strcmp(s_staSsid, ssid) == 0 && strcmp(s_staPass, pass) == 0;
    memcpy(oldPass, s_staPass, sizeof(oldPass));
    portEXIT_CRITICAL(&s_mux);
    if (same) {
        wipe(oldPass, sizeof(oldPass));
        return SetResult::OK;
    }

    // Two keys, two writes. First the password, then the name. When the name fails to save, the password goes back
    // to the old value (or is removed), so that after a restart there is no pair "old name +
    // new password". This is a repair attempt, without a guarantee: NVS that refused a write may refuse again.
    bool ok = storeText(K_STA_PASS, pass);
    if (ok && !storeText(K_STA_SSID, ssid)) {
        if (hadSta) storeText(K_STA_PASS, oldPass);
        else clearKey(K_STA_PASS);
        ok = false;
    }
    wipe(oldPass, sizeof(oldPass));
    if (!ok) return SetResult::STORE_FAILED;

    portENTER_CRITICAL(&s_mux);
    copyText(s_staSsid, sizeof(s_staSsid), ssid);
    copyText(s_staPass, sizeof(s_staPass), pass);
    s_haveSta = true;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

SetResult settingsClearSta() {
    if (!s_nvsOk) return SetResult::STORE_FAILED;
    // Name first: without it the pair is incomplete and after a restart it is "network not stored" anyway.
    bool ok = clearKey(K_STA_SSID);
    ok = clearKey(K_STA_PASS) && ok;
    if (!ok) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    s_staSsid[0] = 0;
    wipe(s_staPass, sizeof(s_staPass));
    s_haveSta = false;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

SetResult settingsSetKeyMap(const char* text) {
    if (!keymapTextAcceptable(text)) return SetResult::INVALID;
    if (!s_nvsOk) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    bool same = s_haveKeymap && strcmp(s_keymap, text) == 0;
    portEXIT_CRITICAL(&s_mux);
    if (same) return SetResult::OK;
    if (!storeText(K_KEYMAP, text[0] ? text : KEYMAP_EMPTY)) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    copyText(s_keymap, sizeof(s_keymap), text);         // fits: checked in keymapTextAcceptable
    s_haveKeymap = true;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

SetResult settingsClearKeyMap() {
    if (!s_nvsOk || !clearKey(K_KEYMAP)) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    s_keymap[0] = 0;
    s_haveKeymap = false;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

SetResult settingsSetVolume(uint8_t percent) {
    if (percent > 100) return SetResult::INVALID;
    if (!s_nvsOk) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    bool same = s_haveVol && s_vol == percent;
    portEXIT_CRITICAL(&s_mux);
    if (same) return SetResult::OK;
    if (s_prefs.putUChar(K_VOL, percent) != 1) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    s_vol = percent;
    s_haveVol = true;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

SetResult settingsClearVolume() {
    if (!s_nvsOk) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    bool had = s_haveVol;
    portEXIT_CRITICAL(&s_mux);
    if (!had) return SetResult::OK;                     // nothing stored: no flash write
    if (!clearKey(K_VOL)) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    s_vol = 0;
    s_haveVol = false;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

// Common write of a 0/1 switch. 'flag' points to the copy in RAM.
static SetResult setFlag(const char* key, bool on, bool* flag) {
    if (!s_nvsOk) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    bool same = (*flag == on);
    portEXIT_CRITICAL(&s_mux);
    // A value equal to the default and not stored does not need a write either (the read gives the same).
    if (same) return SetResult::OK;
    if (s_prefs.putUChar(key, on ? 1 : 0) != 1) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    *flag = on;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

// CAN transmit arming: store the build string. Text of 1..23 printable ASCII characters.
SetResult settingsSetCanTxTag(const char* build) {
    if (build == nullptr) return SetResult::INVALID;
    size_t n = strlen(build);
    if (n < 1 || n > SET_CANTX_TAG_MAX - 1) return SetResult::INVALID;
    for (size_t i = 0; i < n; i++)
        if ((uint8_t)build[i] < 0x20 || (uint8_t)build[i] > 0x7E) return SetResult::INVALID;
    if (!s_nvsOk) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    bool same = s_haveCanTxTag && strcmp(s_canTxTag, build) == 0;
    portEXIT_CRITICAL(&s_mux);
    if (same) return SetResult::OK;
    if (!storeText(K_CANTX, build)) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    copyText(s_canTxTag, sizeof(s_canTxTag), build);
    s_haveCanTxTag = true;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

SetResult settingsClearCanTxTag() {
    if (!s_nvsOk || !clearKey(K_CANTX)) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    s_canTxTag[0] = 0;
    s_haveCanTxTag = false;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

SetResult settingsSetVsetMask(uint32_t mask) {
    if (!s_nvsOk) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    bool same = s_haveVsetMask && s_vsetMask == mask;
    portEXIT_CRITICAL(&s_mux);
    if (same) return SetResult::OK;
    if (s_prefs.putULong(K_VSETMASK, mask) != sizeof(uint32_t)) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    s_vsetMask = mask;
    s_haveVsetMask = true;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

// Panel language: only a well-formed code; membership in the image's dictionary list is checked by the caller.
SetResult settingsSetUiLang(const char* code) {
    if (!uilang::wellFormed(code)) return SetResult::INVALID;
    if (!s_nvsOk) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    bool same = s_haveUiLang && strcmp(s_uiLang, code) == 0;
    portEXIT_CRITICAL(&s_mux);
    if (same) return SetResult::OK;                     // no flash write for an unchanged value
    if (!storeText(K_UI_LANG, code)) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    copyText(s_uiLang, sizeof(s_uiLang), code);         // fits: wellFormed limits the length
    s_haveUiLang = true;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

SetResult settingsClearUiLang() {
    if (!s_nvsOk) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    bool had = s_haveUiLang;
    portEXIT_CRITICAL(&s_mux);
    if (!had) return SetResult::OK;                     // nothing stored: nothing to erase
    if (!clearKey(K_UI_LANG)) return SetResult::STORE_FAILED;
    portENTER_CRITICAL(&s_mux);
    s_uiLang[0] = 0;
    s_haveUiLang = false;
    portEXIT_CRITICAL(&s_mux);
    return SetResult::OK;
}

SetResult settingsSetNetEnabled(bool on) { return setFlag(K_NET_ON, on, &s_netOn); }
SetResult settingsSetLanAdmin(bool on) { return setFlag(K_LAN_ADMIN, on, &s_lanAdmin); }

// ---------- password generation ----------

bool settingsGeneratePassword(char* out, size_t cap) {
    // 32 characters: lowercase letters without 'l' and 'o' (24) and digits 2..9 (8). 32 = 2^5, so 5 bits of a random
    // byte select a character with equal probability, without rejection.
    static const char ALPHABET[] = "abcdefghijkmnpqrstuvwxyz23456789";
    static_assert(sizeof(ALPHABET) == 33, "password alphabet must have 32 characters");
    if (out == nullptr) return false;
    if (cap < SET_PASSWORD_GEN_LEN + 1) {
        if (cap > 0) out[0] = 0;
        return false;
    }
    // Truly random only with the Wi-Fi radio running (the caller takes care of that, see settings.h).
    uint8_t rnd[SET_PASSWORD_GEN_LEN];
    esp_fill_random(rnd, sizeof(rnd));
    for (size_t i = 0; i < SET_PASSWORD_GEN_LEN; i++) out[i] = ALPHABET[rnd[i] & 0x1F];
    out[SET_PASSWORD_GEN_LEN] = 0;
    wipe((char*)rnd, sizeof(rnd));
    return true;
}
