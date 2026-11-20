// Host test: panel language codes (lib/core/ui_lang.h) and the settings backup text
// (lib/core/settings_backup.h): validation of the stored language against the list of dictionaries
// built into the image, export format and every refusal of the import parser. No car recordings.
//
// TRANSMITS NOTHING: pure functions only, no hardware.
//
// Run: bash tools/run_host_tests.sh
#include <cstdio>
#include <cstring>

#include "settings_backup.h"
#include "ui_lang.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

static const char* const LIST = "de,en,es,pl,ru,uk";   // as tools/embed_web.py writes WEB_UI_LANGS

static void testLang() {
    check(uilang::wellFormed("pl"), "lang: pl well formed");
    check(uilang::wellFormed("pt-BR"), "lang: pt-BR well formed");
    check(uilang::wellFormed("abcdefgh"), "lang: 8 characters well formed");
    check(!uilang::wellFormed("abcdefghi"), "lang: 9 characters refused");
    check(!uilang::wellFormed("p"), "lang: 1 character refused");
    check(!uilang::wellFormed(""), "lang: empty refused");
    check(!uilang::wellFormed(nullptr), "lang: nullptr refused");
    check(!uilang::wellFormed("1a"), "lang: starts with a digit refused");
    check(!uilang::wellFormed("pl-"), "lang: ends with '-' refused");
    check(!uilang::wellFormed("p l"), "lang: space refused");
    check(!uilang::wellFormed("pl\"x"), "lang: quote refused");

    check(uilang::inList("pl", LIST), "list: pl is built in");
    check(uilang::inList("de", LIST), "list: first item");
    check(uilang::inList("uk", LIST), "list: last item");
    check(!uilang::inList("fr", LIST), "list: fr is not built in");
    check(!uilang::inList("e", LIST), "list: part of an item does not match");
    check(!uilang::inList("en,pl", LIST), "list: two items do not match");
    check(!uilang::inList("PL", LIST), "list: case matters (file names are lower case)");
    check(!uilang::inList("pl", ""), "list: empty list (build without the panel)");
    check(!uilang::inList("pl", nullptr), "list: no list");
    check(uilang::inList("en", "en"), "list: one item");
}

static backup::ParseError parseText(const char* s, backup::Values& v) { return backup::parse(s, strlen(s), v); }

static bool err(const char* s, backup::ParseError want) {
    backup::Values v;
    backup::ParseError e = parseText(s, v);
    // On any error nothing is given back.
    return e == want && (want == backup::ParseError::OK || (v.present == 0 && v.isNull == 0));
}

static void testParse() {
    using backup::ParseError;
    backup::Values v;
    check(parseText("{\"v\":1,\"lang\":\"pl\",\"vol\":40,\"keys\":\"9F=9B;21=1F\",\"net\":true,\"lan\":false}", v) ==
                  ParseError::OK &&
              v.present == (backup::F_LANG | backup::F_VOL | backup::F_KEYS | backup::F_NET | backup::F_LAN) &&
              v.isNull == 0 && strcmp(v.lang, "pl") == 0 && v.vol == 40 && strcmp(v.keys, "9F=9B;21=1F") == 0 &&
              v.net && !v.lan,
          "parse: full export");
    check(parseText("  { \"lang\" : null , \"keys\":null }\r\n", v) == ParseError::OK &&
              v.present == (backup::F_LANG | backup::F_KEYS) && v.isNull == (backup::F_LANG | backup::F_KEYS),
          "parse: null = back to the default, spaces allowed");
    check(parseText("{\"keys\":\"\"}", v) == ParseError::OK && v.present == backup::F_KEYS && v.isNull == 0 &&
              v.keys[0] == 0,
          "parse: empty table is a value, not null");
    check(parseText("{}", v) == ParseError::OK && v.present == 0, "parse: empty object = nothing changes");
    check(parseText("{\"vol\":100}", v) == ParseError::OK && v.vol == 100, "parse: vol 100");
    check(parseText("{\"vol\":0}", v) == ParseError::OK && v.vol == 0 && v.present == backup::F_VOL,
          "parse: vol 0 accepted here (the caller applies the stored minimum)");
    // Longest table: 16 pairs, 95 characters.
    char longKeys[200];
    std::strcpy(longKeys, "{\"keys\":\"");
    for (int i = 0; i < 16; i++) std::strcat(longKeys, i ? ";AA=BB" : "AA=BB");
    std::strcat(longKeys, "\"}");
    check(parseText(longKeys, v) == ParseError::OK && strlen(v.keys) == backup::KEYS_MAX,
          "parse: table of 95 characters fits");
    std::strcpy(longKeys + strlen(longKeys) - 2, ";\"}");
    check(err(longKeys, ParseError::BAD_VALUE), "parse: table of 96 characters refused");

    check(err("", ParseError::SYNTAX), "refuse: empty text");
    check(err("[]", ParseError::SYNTAX), "refuse: array");
    check(err("{\"vol\":40", ParseError::SYNTAX), "refuse: no closing brace");
    check(err("{\"vol\":40}x", ParseError::SYNTAX), "refuse: text after the object");
    check(err("{\"vol\":40,}", ParseError::SYNTAX), "refuse: comma before the brace");
    check(err("{\"vol\":-1}", ParseError::SYNTAX), "refuse: negative number");
    check(err("{\"vol\":4.5}", ParseError::SYNTAX), "refuse: fraction");
    check(err("{\"vol\":{}}", ParseError::SYNTAX), "refuse: nested object");
    check(err("{\"lang\":\"pl}", ParseError::SYNTAX), "refuse: unterminated string");
    check(err("{\"v\":2,\"vol\":40}", ParseError::VERSION), "refuse: format version 2");
    check(err("{\"v\":\"1\"}", ParseError::BAD_VALUE), "refuse: version as a string");
    check(err("{\"pass\":\"secret123\"}", ParseError::UNKNOWN_KEY), "refuse: password field (never in a backup)");
    check(err("{\"cantx\":\"x\"}", ParseError::UNKNOWN_KEY), "refuse: transmit arming field");
    check(err("{\"vsetmask\":2}", ParseError::UNKNOWN_KEY), "refuse: long unknown field name");
    check(err("{\"vol\":40,\"vol\":50}", ParseError::DUPLICATE), "refuse: duplicate field");
    check(err("{\"v\":1,\"v\":1}", ParseError::DUPLICATE), "refuse: duplicate version");
    check(err("{\"vol\":101}", ParseError::BAD_VALUE), "refuse: vol 101");
    check(err("{\"vol\":1000}", ParseError::BAD_VALUE), "refuse: vol with 4 digits");
    check(err("{\"vol\":\"40\"}", ParseError::BAD_VALUE), "refuse: vol as a string");
    check(err("{\"vol\":true}", ParseError::BAD_VALUE), "refuse: vol as a boolean");
    check(err("{\"lang\":\"p\"}", ParseError::BAD_VALUE), "refuse: lang too short");
    check(err("{\"lang\":\"abcdefghijk\"}", ParseError::BAD_VALUE), "refuse: lang too long");
    check(err("{\"lang\":3}", ParseError::BAD_VALUE), "refuse: lang as a number");
    check(err("{\"keys\":\"9F=9B\\\"\"}", ParseError::BAD_VALUE), "refuse: backslash in a string");
    check(err("{\"keys\":\"9F=9B\x01\"}", ParseError::BAD_VALUE), "refuse: control character in a string");
    check(err("{\"net\":1}", ParseError::BAD_VALUE), "refuse: net as a number");
    check(err("{\"lan\":null}", ParseError::BAD_VALUE), "refuse: lan null");
    check(err("{\"keys\":[]}", ParseError::SYNTAX), "refuse: keys as an array");

    // The length limit really stops reading: "{"vol":40}" with len 6 is cut and gives syntax.
    check(backup::parse("{\"vol\":40}", 6, v) == ParseError::SYNTAX, "parse: reads only 'len' bytes");
    check(backup::parse(nullptr, 0, v) == ParseError::SYNTAX, "parse: nullptr");
    std::printf("error words: %s %s %s %s %s %s\n", backup::errorName(ParseError::OK),
                backup::errorName(ParseError::SYNTAX), backup::errorName(ParseError::VERSION),
                backup::errorName(ParseError::UNKNOWN_KEY), backup::errorName(ParseError::DUPLICATE),
                backup::errorName(ParseError::BAD_VALUE));
}

static void testFormat() {
    backup::Values v;
    char out[backup::TEXT_MAX];
    check(backup::format(v, out, sizeof(out)) &&
              strcmp(out, "{\"v\":1,\"lang\":null,\"vol\":null,\"keys\":null,\"net\":true,\"lan\":false}") == 0,
          "format: nothing stored");
    std::printf("export empty: %s\n", out);

    v.present = backup::F_LANG | backup::F_VOL | backup::F_KEYS;
    std::strcpy(v.lang, "pl");
    v.vol = 55;
    std::strcpy(v.keys, "9F=9B;21=1F");
    v.lan = true;
    check(backup::format(v, out, sizeof(out)), "format: values");
    std::printf("export: %s\n", out);

    // Round trip: the export parses back to the same values.
    backup::Values back;
    check(backup::parse(out, strlen(out), back) == backup::ParseError::OK && strcmp(back.lang, "pl") == 0 &&
              back.vol == 55 && strcmp(back.keys, "9F=9B;21=1F") == 0 && back.lan && back.net,
          "format: export parses back");

    // Worst case fits in TEXT_MAX.
    std::strcpy(v.lang, "abcdefgh");
    v.vol = 100;
    std::memset(v.keys, 'A', backup::KEYS_MAX);
    v.keys[backup::KEYS_MAX] = 0;
    v.net = false;
    v.lan = false;
    check(backup::format(v, out, sizeof(out)), "format: longest text fits in TEXT_MAX");
    std::printf("longest export: %u characters (TEXT_MAX %u)\n", (unsigned)strlen(out), (unsigned)backup::TEXT_MAX);
    check(!backup::format(v, out, 100), "format: too small buffer = false");
    check(!backup::format(v, nullptr, 0), "format: no buffer = false");
}

int main() {
    testLang();
    testParse();
    testFormat();
    std::printf("settings_backup: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
