// Host test: login and forms (web_auth), JSON writing (json_writer),
// firmware tag scanner (fw_ident), including the canTx field and the FwIdent block of the 0.6.0 image
// (test_data/fixtures/fw_ident_060.bin). No car recordings.
//
// Build (from the firmware directory):
//   uvx --from ziglang python -m ziglang c++ -std=c++17 -O1 -Wall -I lib/core test_host/net_util.cpp
//       lib/core/web_auth.cpp lib/core/json_writer.cpp lib/core/fw_ident.cpp -o .build_host/net_util.exe
#include <cmath>
#include <cstdio>
#include <cstring>

#include "fw_ident.h"
#include "json_writer.h"
#include "send_deadline.h"
#include "web_auth.h"

using namespace webauth;

static int failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) failures++;
}

// ---------------------------------------------------------------- base64, parseBasic

static void testBase64() {
    uint8_t out[32];
    check(base64Decode("YWRtaW46aGVsbG8=", 16, out, sizeof(out)) == 11 && memcmp(out, "admin:hello", 11) == 0,
          "base64 with one '='");
    check(base64Decode("YWRtaW46aGVsbG8", 15, out, sizeof(out)) == 11 && memcmp(out, "admin:hello", 11) == 0,
          "base64 without '='");
    check(base64Decode("YQ==", 4, out, sizeof(out)) == 1 && out[0] == 'a', "base64 with '=='");
    check(base64Decode("YQ", 2, out, sizeof(out)) == 1 && out[0] == 'a', "base64 without '==' (2 characters)");
    check(base64Decode("YWJj", 4, out, sizeof(out)) == 3 && memcmp(out, "abc", 3) == 0, "base64 full group");
    check(base64Decode("", 0, out, sizeof(out)) == 0, "base64 empty text = 0 bytes");
    check(base64Decode("YW Jj", 5, out, sizeof(out)) == SIZE_MAX, "base64 space = error");
    check(base64Decode("YWJj\n", 5, out, sizeof(out)) == SIZE_MAX, "base64 end of line = error");
    check(base64Decode("YQ=", 3, out, sizeof(out)) == SIZE_MAX, "base64 bad padding");
    check(base64Decode("Y===", 4, out, sizeof(out)) == SIZE_MAX, "base64 three '=' = error");
    check(base64Decode("YQ==YQ==", 8, out, sizeof(out)) == SIZE_MAX, "base64 '=' in the middle = error");
    check(base64Decode("Y", 1, out, sizeof(out)) == SIZE_MAX, "base64 one character = error");
    check(base64Decode("YW*j", 4, out, sizeof(out)) == SIZE_MAX, "base64 character outside the alphabet");
    check(base64Decode("YWJj", 4, out, 2) == SIZE_MAX, "base64 buffer too small");
    check(base64Decode("YWJj", 4, out, 3) == 3, "base64 buffer exactly the right size");

    char u[16], p[70];
    check(parseBasic("Basic YWRtaW46aGVsbG8=", u, sizeof(u), p, sizeof(p)) && strcmp(u, "admin") == 0 &&
              strcmp(p, "hello") == 0,
          "parseBasic admin:hello");
    check(parseBasic("  basic   YWRtaW46aGVsbG8=  ", u, sizeof(u), p, sizeof(p)) && strcmp(p, "hello") == 0,
          "parseBasic lower case and spaces");
    // "a:b:c" -> user "a", password "b:c" (a colon in the password is allowed)
    check(parseBasic("Basic YTpiOmM=", u, sizeof(u), p, sizeof(p)) && strcmp(u, "a") == 0 && strcmp(p, "b:c") == 0,
          "parseBasic colon in the password");
    // ":x" -> empty user
    check(parseBasic("Basic Ong=", u, sizeof(u), p, sizeof(p)) && u[0] == 0 && strcmp(p, "x") == 0,
          "parseBasic empty user");
    check(!parseBasic("Bearer YWRtaW46aGVsbG8=", u, sizeof(u), p, sizeof(p)) && u[0] == 0 && p[0] == 0,
          "parseBasic another scheme");
    check(!parseBasic("BasicYWRtaW46aGVsbG8=", u, sizeof(u), p, sizeof(p)), "parseBasic without a space");
    check(!parseBasic("Basic YWRtaW4=", u, sizeof(u), p, sizeof(p)), "parseBasic without a colon");
    check(!parseBasic("Basic ", u, sizeof(u), p, sizeof(p)), "parseBasic empty value");
    check(!parseBasic("Basic YWRtaW46aGVs bG8=", u, sizeof(u), p, sizeof(p)), "parseBasic space in base64");
    check(!parseBasic("Basic YWRtaW46aGVsbG8=", u, 5, p, sizeof(p)) && u[0] == 0, "parseBasic user buffer too small");
    check(!parseBasic("Basic YWRtaW46aGVsbG8=", u, sizeof(u), p, 5) && p[0] == 0, "parseBasic password buffer too small");
    check(parseBasic("Basic YWRtaW46aGVsbG8=", u, 6, p, 6), "parseBasic buffers exactly the right size");
    // "a:\0b" -> zero byte in the password = error
    check(!parseBasic("Basic YToAYg==", u, sizeof(u), p, sizeof(p)), "parseBasic zero byte");
    check(!parseBasic(nullptr, u, sizeof(u), p, sizeof(p)), "parseBasic nullptr");
}

// ---------------------------------------------------------------- urlDecode, formValue

static void testForm() {
    char o[16];
    check(urlDecode("a+b%20c%2Fd", 11, o, sizeof(o)) == 7 && strcmp(o, "a b c/d") == 0, "urlDecode + and %XX");
    check(urlDecode("%4a%4A", 6, o, sizeof(o)) == 2 && strcmp(o, "JJ") == 0, "urlDecode lower and upper case");
    check(urlDecode("%G1", 3, o, sizeof(o)) == SIZE_MAX && o[0] == 0, "urlDecode bad %XX");
    check(urlDecode("ab%4", 4, o, sizeof(o)) == SIZE_MAX && o[0] == 0, "urlDecode cut %X");
    check(urlDecode("ab%", 3, o, sizeof(o)) == SIZE_MAX, "urlDecode lone %");
    check(urlDecode("a%00b", 5, o, sizeof(o)) == SIZE_MAX && o[0] == 0, "urlDecode zero byte");
    check(urlDecode("abcd", 4, o, 4) == SIZE_MAX && o[0] == 0, "urlDecode no room for the zero");
    check(urlDecode("abcd", 4, o, 5) == 4, "urlDecode exactly the right size");
    check(urlDecode("", 0, o, sizeof(o)) == 0 && o[0] == 0, "urlDecode empty");
    const char raw0[] = {'a', 0, 'b'};
    check(urlDecode(raw0, 3, o, sizeof(o)) == SIZE_MAX, "urlDecode raw zero byte");

    const char* body = "ssid=Net+2&pass=a%26b%3Dc&empty=&x";
    size_t bl = strlen(body);
    check(formValue(body, bl, "ssid", o, sizeof(o)) && strcmp(o, "Net 2") == 0, "formValue first field");
    check(formValue(body, bl, "pass", o, sizeof(o)) && strcmp(o, "a&b=c") == 0, "formValue encoded & and =");
    check(formValue(body, bl, "empty", o, sizeof(o)) && o[0] == 0, "formValue empty value");
    check(!formValue(body, bl, "x", o, sizeof(o)) && o[0] == 0, "formValue field without '='");
    check(!formValue(body, bl, "ss", o, sizeof(o)), "formValue a name prefix is not a field");
    check(!formValue(body, bl, "none", o, sizeof(o)), "formValue missing field");
    check(!formValue("a=%ZZ", 5, "a", o, sizeof(o)) && o[0] == 0, "formValue bad %XX");
    check(!formValue("a=0123456789abcdef", 18, "a", o, sizeof(o)) && o[0] == 0, "formValue value too long");
    check(formValue("a=1&a=2", 7, "a", o, sizeof(o)) && strcmp(o, "1") == 0, "formValue first of repeated ones");
    // the body length limits the read (no terminating zero in the buffer)
    char nb[8] = {'k', '=', 'v', 'v', 'X', 'X', 'X', 'X'};
    check(formValue(nb, 4, "k", o, sizeof(o)) && strcmp(o, "vv") == 0, "formValue respects the body length");
}

// ---------------------------------------------------------------- passwords

static void testPasswords() {
    check(!passwordAcceptable("123456789"), "password of 9 characters too short");
    check(passwordAcceptable("1234567890"), "password of 10 characters");
    char p63[64], p64[65];
    memset(p63, 'x', 63);
    p63[63] = 0;
    memset(p64, 'x', 64);
    p64[64] = 0;
    check(passwordAcceptable(p63), "password of 63 characters");
    check(!passwordAcceptable(p64), "password of 64 characters too long");
    check(!passwordAcceptable("12345 67890"), "password with a space");
    check(!passwordAcceptable("1234567890\x7f"), "password with character 0x7F");
    check(!passwordAcceptable("1234567890\xc5\x82"), "password with a non-ASCII letter");
    check(passwordAcceptable("!~#$%^&*()_+"), "password with special characters");
    check(!passwordAcceptable(nullptr), "password nullptr");

    check(constantTimeEquals("secretword1", "secretword1"), "comparison equal");
    check(!constantTimeEquals("secretword1", "secretword2"), "comparison different at the end");
    check(!constantTimeEquals("secretword", "secretword1"), "comparison shorter");
    check(!constantTimeEquals("secretword12", "secretword1"), "comparison longer");
    check(!constantTimeEquals("", "secretword1"), "comparison empty given");
    check(!constantTimeEquals("", ""), "comparison with an empty device password = false");
    check(!constantTimeEquals(nullptr, "a"), "comparison nullptr");
}

// ---------------------------------------------------------------- LoginGuard

static void failN(LoginGuard& g, int n, uint32_t t, uint32_t step = 0) {
    for (int i = 0; i < n; i++) g.onFailure(t + (uint32_t)i * step);
}

static void testGuardAt(uint32_t base, const char* label) {
    char name[128];
    {
        LoginGuard g;
        failN(g, 4, base, 1000);
        std::snprintf(name, sizeof(name), "%s: 4 failures without a lockout", label);
        check(g.lockedForMs(base + 4000) == 0, name);
        g.onFailure(base + 5000);
        std::snprintf(name, sizeof(name), "%s: 5th failure in 60 s -> 30 s lockout", label);
        check(g.lockedForMs(base + 5000) == 30000, name);
        std::snprintf(name, sizeof(name), "%s: the lockout ends after 30 s", label);
        check(g.lockedForMs(base + 34999) == 1 && g.lockedForMs(base + 35000) == 0, name);
        // second lockout: 60 s
        failN(g, 5, base + 36000, 100);
        std::snprintf(name, sizeof(name), "%s: second lockout 60 s", label);
        check(g.lockedForMs(base + 36400) == 60000, name);
        failN(g, 5, base + 100000, 100);
        std::snprintf(name, sizeof(name), "%s: third lockout 120 s", label);
        check(g.lockedForMs(base + 100400) == 120000, name);
        failN(g, 5, base + 300000, 100);
        failN(g, 5, base + 600000, 100);
        std::snprintf(name, sizeof(name), "%s: fifth lockout limited to 5 min", label);
        check(g.lockedForMs(base + 600400) == 300000, name);
        failN(g, 5, base + 1000000, 100);
        check(g.lockedForMs(base + 1000400) == 300000, name);
        // a success right after the end of a lockout does not restore 30 s, after 60 s of quiet it does
        uint32_t end = base + 1000400 + 300000;
        g.onSuccess(end + 1000);
        failN(g, 5, end + 2000, 10);
        std::snprintf(name, sizeof(name), "%s: a success right after a lockout does not shorten the next one", label);
        check(g.lockedForMs(end + 2040) == 300000, name);
        uint32_t end2 = end + 2040 + 300000;
        g.onSuccess(end2 + 60000);
        failN(g, 5, end2 + 61000, 10);
        std::snprintf(name, sizeof(name), "%s: after 60 s without failures a success restores 30 s", label);
        check(g.lockedForMs(end2 + 61040) == 30000, name);
    }
    {
        LoginGuard g;
        failN(g, 4, base, 1000);
        g.onSuccess(base + 4500);
        g.onFailure(base + 5000);
        std::snprintf(name, sizeof(name), "%s: 4 failures, success, 1 failure -> lockout", label);
        check(g.lockedForMs(base + 5000) == 30000, name);
    }
    {
        // interleaving "4 errors, success" for 10 min must lock out
        LoginGuard g;
        bool locked = false;
        for (uint32_t t = 0; t < 600000 && !locked; t += 2000) {
            for (int i = 0; i < 4; i++) g.onFailure(base + t + (uint32_t)i * 100);
            g.onSuccess(base + t + 500);
            if (g.lockedForMs(base + t + 500) > 0) locked = true;
        }
        std::snprintf(name, sizeof(name), "%s: interleaved 4 failures + success locks out", label);
        check(locked, name);
    }
    {
        LoginGuard g;
        bool locked = false;
        for (uint32_t t = 0; t < 600000; t += 15000) {
            g.onFailure(base + t);
            if (g.lockedForMs(base + t) > 0) locked = true;
        }
        std::snprintf(name, sizeof(name), "%s: failures every 15 s do not lock out (the window passes)", label);
        check(!locked, name);
    }
    {
        LoginGuard g;
        failN(g, 4, base, 1000);
        g.onFailure(base + 60000);    // exactly 60 s from the first: a new window
        std::snprintf(name, sizeof(name), "%s: 5th failure after 60 s starts a new window", label);
        check(g.lockedForMs(base + 60000) == 0, name);
    }
    {
        // errors during a lockout do not extend it
        LoginGuard g;
        failN(g, 5, base, 10);
        failN(g, 10, base + 1000, 10);
        std::snprintf(name, sizeof(name), "%s: failures during a lockout do not extend it", label);
        check(g.lockedForMs(base + 1100) == 30000 - 1100 + 40, name);
    }
}

static void testGuard() {
    testGuardAt(1000, "LoginGuard");
    testGuardAt(0xFFFFFFFFu - 20000, "LoginGuard with millis wrapping");
    testGuardAt(0xFFFFFFFFu - 2000000, "LoginGuard wrapping during lockouts");
    // Review: an old lockout came back after 2^32 ms (49.7 days without sleep) because nothing cleared it.
    // The server calls poll() on every request; once the lockout has been seen over, the counter wrap cannot
    // bring it back.
    {
        LoginGuard g;
        failN(g, 5, 1000, 10);                                    // 30 s lockout from 1040
        check(g.lockedForMs(1040) == 30000, "LoginGuard: lockout of 30 s (wrap test)");
        LoginGuard old = g;                                       // never polled (behaviour before the fix)
        g.poll(40000);                                            // a request after the end
        const uint32_t wrapped = 6040u;                           // 1040 + 5000 + 2^32 (mod 2^32)
        check(old.lockedForMs(wrapped) == 25000, "LoginGuard: without poll() the wrap revives the lockout (the reviewed defect)");
        g.poll(wrapped);
        check(g.lockedForMs(wrapped) == 0, "LoginGuard: 2^32 ms later the old lockout does not come back");
        g.onFailure(wrapped + 10);
        check(g.lockedForMs(wrapped + 10) == 0, "LoginGuard: after the wrap one failure does not lock out");
    }
}

// ---------------------------------------------------------------- SendDeadline
// Review: a slow reader (a few bytes every 2 s) held the only server task without limit. The total deadline
// counts from the first send of a response; every send within it goes, the first one after it fails.
static void testSendDeadline() {
    SendDeadline d;
    const uint32_t LIMIT = 20000;
    check(d.allow(5000, LIMIT), "SendDeadline: the first send starts the clock");
    bool all = true;
    for (uint32_t t = 5000; t < 25000; t += 1900) all = all && d.allow(t, LIMIT);   // slow reader, every 1.9 s
    check(all && d.allow(24999, LIMIT), "SendDeadline: sends within 20 s go through");
    check(!d.allow(25000, LIMIT), "SendDeadline: a send after 20 s fails (connection dropped)");
    d.reset();
    check(d.allow(90000, LIMIT) && d.allow(109999, LIMIT), "SendDeadline: a new response starts a new clock");
    SendDeadline w;
    check(w.allow(0xFFFFF000u, LIMIT) && w.allow(0x00000FFFu, LIMIT) && !w.allow(0xFFFFF000u + LIMIT, LIMIT),
          "SendDeadline: counted correctly across the millis() wrap");
}

// ---------------------------------------------------------------- JsonWriter

static bool jsonIs(const JsonWriter& j, const char* expect) { return j.ok() && strcmp(j.c_str(), expect) == 0; }

static void testJson() {
    char b[512];
    {
        JsonWriter j(b, sizeof(b));
        j.beginObject();
        j.key("rx");
        j.unum(123);
        j.key("ok");
        j.boolean(true);
        j.key("a");
        j.beginArray();
        j.num(-5);
        j.beginArray();
        j.endArray();
        j.beginObject();
        j.endObject();
        j.null();
        j.endArray();
        j.key("o");
        j.beginObject();
        j.key("x");
        j.str("y");
        j.endObject();
        j.endObject();
        check(jsonIs(j, "{\"rx\":123,\"ok\":true,\"a\":[-5,[],{},null],\"o\":{\"x\":\"y\"}}"), "json commas and nesting");
    }
    {
        JsonWriter j(b, sizeof(b));
        j.beginArray();
        j.num(INT32_MIN);
        j.unum(UINT32_MAX);
        j.boolean(false);
        j.str(nullptr);
        j.endArray();
        check(jsonIs(j, "[-2147483648,4294967295,false,null]"), "json extreme numbers and null");
    }
    {
        JsonWriter j(b, sizeof(b));
        j.beginArray();
        j.fixed(12.345f, 2);
        j.fixed(-0.5f, 1);
        j.fixed(7.0f, 0);
        j.fixed(1.23456f, 9);    // truncated to 3 decimal places
        j.fixed(NAN, 2);
        j.fixed(INFINITY, 1);
        j.endArray();
        check(jsonIs(j, "[12.35,-0.5,7,1.235,null,null]"), "json fixed, NaN and infinity -> null");
        if (!jsonIs(j, "[12.35,-0.5,7,1.235,null,null]")) std::printf("      result: %s\n", j.c_str());
    }
    {
        JsonWriter j(b, sizeof(b));
        const uint8_t d[] = {0xB0, 0xF0, 0x00, 0x0a, 0xff};
        j.beginArray();
        j.hex(d, sizeof(d));
        j.hex(d, 0);
        j.hexByte(0x9f);
        j.hexId(0x0E1);
        j.hexId(0x7FF);
        j.endArray();
        check(jsonIs(j, "[\"B0F0000AFF\",\"\",\"9F\",\"0E1\",\"7FF\"]"), "json hex in upper case");
    }
    {
        // longer hex across the internal buffer boundary (32 characters)
        uint8_t d[40];
        for (int i = 0; i < 40; i++) d[i] = (uint8_t)i;
        JsonWriter j(b, sizeof(b));
        j.hex(d, sizeof(d));
        bool good = j.ok() && j.length() == 82 && memcmp(j.c_str(), "\"0001020304", 11) == 0 &&
                    memcmp(j.c_str() + 77, "2627\"", 5) == 0;
        check(good, "json hex 40 bytes");
    }
    {
        JsonWriter j(b, sizeof(b));
        j.beginArray();
        j.str("a\"b\\c");
        j.str("\x01\x1f\n\t");
        j.str("za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 \xE2\x82\xAC\xF0\x9D\x84\x9E");          // 2-, 3- and 4-byte UTF-8 unchanged (Polish letters, euro sign, G clef; escaped to keep the source ASCII)
        j.endArray();
        check(jsonIs(j, "[\"a\\\"b\\\\c\",\"\\u0001\\u001F\\u000A\\u0009\",\"za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 \xE2\x82\xAC\xF0\x9D\x84\x9E\"]"),
              "json escapes, control characters, valid UTF-8");
        if (!j.ok() || strcmp(j.c_str(), "[\"a\\\"b\\\\c\",\"\\u0001\\u001F\\u000A\\u0009\",\"za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 \xE2\x82\xAC\xF0\x9D\x84\x9E\"]") != 0)
            std::printf("      result: %s\n", j.c_str());
    }
    {
        JsonWriter j(b, sizeof(b));
        j.beginArray();
        j.str("a\x80" "b");            // lone continuation byte
        j.strN("\xc5", 1);             // truncated 2-byte sequence
        j.strN("\xe2\x82", 2);         // truncated 3-byte: both bytes -> replacement
        j.str("\xc0\x80");             // overlong encoding
        j.str("\xed\xa0\x80");         // UTF-16 surrogate
        j.str("\xf4\x90\x80\x80");     // above U+10FFFF
        j.str("\xff");
        j.str("\xe2\x82\x41");         // truncated sequence followed by an ordinary character
        j.endArray();
        const char* e =
            "[\"a\\ufffdb\",\"\\ufffd\",\"\\ufffd\\ufffd\",\"\\ufffd\\ufffd\",\"\\ufffd\\ufffd\\ufffd\","
            "\"\\ufffd\\ufffd\\ufffd\\ufffd\",\"\\ufffd\",\"\\ufffd\\ufffdA\"]";
        check(jsonIs(j, e), "json bad UTF-8 -> \\ufffd");
        if (!jsonIs(j, e)) std::printf("      result: %s\n", j.c_str());
    }
    {
        // worst case: 6 output characters per input byte
        char in[64];
        memset(in, 0xff, sizeof(in));
        static char big[64 * 6 + 3];
        JsonWriter j(big, sizeof(big));
        j.strN(in, sizeof(in));
        check(j.ok() && j.length() == 64 * 6 + 2, "json worst case 6 characters per byte");
        JsonWriter k(big, sizeof(big) - 1);
        k.strN(in, sizeof(in));
        check(!k.ok() && big[k.length()] == 0, "json buffer one byte too small -> error");
        memset(in, 0x01, sizeof(in));
        JsonWriter m(big, sizeof(big));
        m.strN(in, sizeof(in));
        check(m.ok() && m.length() == 64 * 6 + 2, "json control characters also 6 per byte");
    }
    {
        // overflow: the flag drops, the buffer is zero-terminated, nothing past the boundary
        char s[20];
        memset(s, 'Z', sizeof(s));
        JsonWriter j(s, 16);
        j.beginObject();
        j.key("abc");
        j.str("0123456789");
        j.key("d");
        j.unum(1);
        j.endObject();
        bool term = false;
        for (int i = 0; i < 16; i++)
            if (s[i] == 0) term = true;
        check(!j.ok() && term && s[16] == 'Z' && s[19] == 'Z' && strlen(s) == j.length() && j.length() < 16,
              "json overflow: ok()==false, zero at the end, no write past the buffer");
        j.reset();
        j.unum(5);
        check(jsonIs(j, "5"), "json reset after an overflow");
    }
    {
        JsonWriter j(b, sizeof(b));
        for (int i = 0; i < JsonWriter::MAX_DEPTH; i++) j.beginArray();
        check(j.ok(), "json 8 nesting levels");
        j.beginArray();
        check(!j.ok(), "json 9th level = error");
    }
    {
        JsonWriter j(b, sizeof(b));
        j.beginObject();
        j.unum(1);
        check(!j.ok(), "json value in an object without a name = error");
        JsonWriter k(b, sizeof(b));
        k.beginArray();
        k.key("x");
        check(!k.ok(), "json name in an array = error");
        JsonWriter m(b, sizeof(b));
        m.beginObject();
        m.endArray();
        check(!m.ok(), "json bad closing = error");
        JsonWriter n(b, sizeof(b));
        n.beginObject();
        n.key("x");
        n.endObject();
        check(!n.ok(), "json name without a value = error");
        JsonWriter q(b, sizeof(b));
        q.unum(1);
        q.unum(2);
        check(!q.ok(), "json two values at the top level = error");
        JsonWriter r(b, sizeof(b));
        r.endObject();
        check(!r.ok(), "json closing without opening = error");
    }
    {
        JsonWriter j(b, 0);
        j.unum(1);
        check(!j.ok(), "json buffer of 0 bytes");
    }
    {
        JsonWriter j(b, sizeof(b));
        j.beginObject();
        j.key("k\"1");
        j.unum(1);
        j.endObject();
        check(jsonIs(j, "{\"k\\\"1\":1}"), "json field name with an escape");
    }
}

// ---------------------------------------------------------------- IdentScanner

// The tests use the current project identity (include/project_identity.h), so a rename of the
// project does not change them. PRODUCT = the product name the running firmware accepts.
static const char* const PRODUCT = PROJECT_FW_PRODUCT;

// Identity of the prototype firmware (up to 1.0.0, before the project got its name). Historical, fixed:
// test_data/fixtures/fw_ident_060.bin was cut from such an image. Not used by the firmware.
static const char LEGACY_FWID_MAGIC[12] = "\x7F" "C508-FWID" "\x7F";    // 11 characters + zero
static const char* const LEGACY_FW_PRODUCT = "centralka-508";

// Pattern stored as in the image: the only copy of the magic is in the structure.
static FwIdent makeIdent(const char* product, uint8_t net) {
    FwIdent f = {};
    memcpy(f.magic, FW_IDENT_MAGIC, 12);
    strncpy(f.product, product, sizeof(f.product));
    strncpy(f.version, "0.6.1", sizeof(f.version));
    strncpy(f.build, "Nov 17 2026 10:02:11", sizeof(f.build));
    f.mode = 1;
    f.net = net;
    return f;
}

static void testIdent() {
    const FwIdent ref = makeIdent(PRODUCT, 1);
    // File: garbage, false pattern starts, the real tag, garbage, a second tag (ignored).
    static uint8_t img[600];
    for (size_t i = 0; i < sizeof(img); i++) img[i] = (uint8_t)(i * 37 + 11);
    uint8_t fake1[8];     // 7F and the first 7 marker characters: cut off
    uint8_t fake2[11];    // 7F, the 9 marker characters, 7F: no zero
    memcpy(fake1, FW_IDENT_MAGIC, sizeof(fake1));
    memcpy(fake2, FW_IDENT_MAGIC, sizeof(fake2));
    memcpy(img + 50, fake1, sizeof(fake1));
    memcpy(img + 100, fake2, sizeof(fake2));
    // false start overlapping the real one: 7F + the 9 marker characters and immediately the real tag from 7F
    const size_t pos = 200;
    memcpy(img + pos - 10, fake2, 10);    // 7F + 9 characters, followed by the real 7F <marker> 7F 00
    memcpy(img + pos, &ref, sizeof(ref));
    FwIdent other = makeIdent("other-product", 0);
    memcpy(img + 400, &other, sizeof(other));

    bool allOk = true;
    for (size_t chunk = 1; chunk <= 80; chunk++) {
        IdentScanner s(ref.magic);
        for (size_t off = 0; off < sizeof(img); off += chunk) {
            size_t n = sizeof(img) - off < chunk ? sizeof(img) - off : chunk;
            s.feed(img + off, n);
        }
        if (!s.found() || memcmp(&s.ident(), &ref, sizeof(ref)) != 0) {
            std::printf("      chunk %u: found=%d\n", (unsigned)chunk, (int)s.found());
            allOk = false;
        }
    }
    check(allOk, "scanner: chunks of 1..80 bytes, false starts, the first marker wins");

    // Split at every possible position: two chunks [0, cut) and [cut, end).
    allOk = true;
    for (size_t cut = 0; cut <= sizeof(img); cut++) {
        IdentScanner s(ref.magic);
        s.feed(img, cut);
        s.feed(img + cut, sizeof(img) - cut);
        if (!s.found() || memcmp(&s.ident(), &ref, sizeof(ref)) != 0) allOk = false;
    }
    check(allOk, "scanner: split into two chunks at every position");

    {
        IdentScanner s(ref.magic);
        s.feed((const uint8_t*)&ref, sizeof(ref));
        check(s.found() && fwIdentAcceptable(s.ident(), PRODUCT, true), "scanner: marker in one chunk");
        s.reset();
        check(!s.found(), "scanner: reset");
    }
    {
        IdentScanner s(ref.magic);
        s.feed(img, 199);    // only garbage and false starts
        check(!s.found(), "scanner: no marker");
        IdentScanner t(ref.magic);
        t.feed(img, pos + sizeof(ref) - 1);    // tag without its last byte
        check(!t.found(), "scanner: incomplete marker at the end of the file");
    }
    {
        // 7F <marker> 7F 7F <marker> 7F 00 ...: the second 7F starts the real match
        uint8_t buf[200] = {0};
        memcpy(buf, fake2, sizeof(fake2));
        memcpy(buf + sizeof(fake2), &ref, sizeof(ref));
        IdentScanner s(ref.magic);
        s.feed(buf, sizeof(buf));
        check(s.found() && memcmp(&s.ident(), &ref, sizeof(ref)) == 0, "scanner: pattern without a zero right before the real one");
    }

    // fwIdentAcceptable
    check(fwIdentAcceptable(ref, PRODUCT, true), "acceptance: matching product with network");
    FwIdent noNet = makeIdent(PRODUCT, 0);
    check(!fwIdentAcceptable(noNet, PRODUCT, true), "acceptance: no network with requireNet");
    check(fwIdentAcceptable(noNet, PRODUCT, false), "acceptance: no network without requireNet");
    char longer[sizeof(PROJECT_FW_PRODUCT) + 1];    // the product with one more character
    std::snprintf(longer, sizeof(longer), "%s0", PRODUCT);
    char shorter[sizeof(PROJECT_FW_PRODUCT) - 1];   // the product without its last character
    std::memcpy(shorter, PRODUCT, sizeof(shorter) - 1);
    shorter[sizeof(shorter) - 1] = 0;
    check(!fwIdentAcceptable(makeIdent(longer, 1), PRODUCT, true), "acceptance: longer product");
    check(!fwIdentAcceptable(makeIdent(shorter, 1), PRODUCT, true), "acceptance: shorter product");
    FwIdent bad = ref;
    memset(bad.product, 'A', sizeof(bad.product));    // no zero in the field
    memset(bad.version, 'B', sizeof(bad.version));
    check(!fwIdentAcceptable(bad, "AAAAAAAAAAAAAAAA", true), "acceptance: product without a zero (16 characters) rejected");
    check(!fwIdentAcceptable(bad, "AAAAAAAAAAAAAAAAB", true), "acceptance: does not read past the field");
    FwIdent full = ref;
    memset(full.product, 0, sizeof(full.product));
    memcpy(full.product, PRODUCT, sizeof(PROJECT_FW_PRODUCT));
    full.net = 2;
    check(!fwIdentAcceptable(full, PRODUCT, true), "acceptance: net must be 1");
    check(!fwIdentAcceptable(ref, "", true), "acceptance: empty product");
    check(!fwIdentAcceptable(ref, nullptr, true), "acceptance: nullptr");
}

// ---------------------------------------------------------------- canTx tag

// Consent on update and rollback: a difference in mode OR in the ability to transmit on CAN.
static void testCanTx() {
    // The byte layout is a contract with the panel and with older firmware: mode 68, net 69, canTx 70, reserved 71.
    uint8_t raw[sizeof(FwIdent)] = {0};
    memcpy(raw, FW_IDENT_MAGIC, 12);
    memcpy(raw + 12, PRODUCT, sizeof(PROJECT_FW_PRODUCT));
    raw[68] = 1;
    raw[69] = 1;
    raw[70] = 1;
    FwIdent f;
    memcpy(&f, raw, sizeof(f));
    check(f.mode == 1 && f.net == 1 && f.canTx == 1 && f.reserved == 0, "canTx: byte 70 of the marker (mode 68, net 69, reserve 71)");

    FwIdent run0 = makeIdent(PRODUCT, 1);    // running firmware without transmission
    check(run0.canTx == 0, "canTx: zeroed marker = no transmit");
    FwIdent run1 = run0;
    run1.canTx = 1;                                   // running firmware with transmission
    check(!fwIdentModeChange(run0, 1, 0), "canTx: same mode, 0 -> 0 without consent");
    check(fwIdentModeChange(run0, 1, 1), "canTx: 0 -> 1 needs consent");
    check(fwIdentModeChange(run1, 1, 0), "canTx: 1 -> 0 needs consent");
    check(!fwIdentModeChange(run1, 1, 1), "canTx: same mode, 1 -> 1 without consent");
    check(fwIdentModeChange(run0, 0, 0), "canTx: another operating mode needs consent as before");
    check(fwIdentModeChange(run1, 0, 0), "canTx: another mode and another canTx need consent");
    check(fwIdentModeChange(run0, 1, 0xFF), "canTx: a value outside 0/1 in the other image = a difference");

    // Old image (up to 0.6.0): the place of canTx held the first reserved byte, always 0.
    uint8_t old[200];
    for (size_t i = 0; i < sizeof(old); i++) old[i] = (uint8_t)(i * 13 + 5);
    uint8_t oldId[72] = {0};
    memcpy(oldId, FW_IDENT_MAGIC, 12);
    memcpy(oldId + 12, PRODUCT, sizeof(PROJECT_FW_PRODUCT));
    memcpy(oldId + 28, "0.6.0", 6);
    memcpy(oldId + 44, "Nov 16 2026 21:14:44", 21);
    oldId[68] = 1;    // mode
    oldId[69] = 1;    // net; bytes 70 and 71 = reserved {0, 0}
    memcpy(old + 60, oldId, sizeof(oldId));
    IdentScanner s(run0.magic);
    s.feed(old, sizeof(old));
    check(s.found() && s.ident().canTx == 0 && s.ident().mode == 1 && s.ident().net == 1,
          "canTx: old marker (reserve 0, 0) = no transmit");
    check(s.found() && !fwIdentModeChange(run0, s.ident().mode, s.ident().canTx),
          "canTx: old image onto a program without transmit without consent");
    check(s.found() && fwIdentModeChange(run1, s.ident().mode, s.ident().canTx),
          "canTx: old image onto a program with transmit needs consent");
    check(fwIdentAcceptable(run1, PRODUCT, true), "canTx: does not affect product recognition");
}

// FwIdent block cut unchanged from a real 0.6.0 image of the prototype firmware (test_data/fixtures/fw_ident_060.bin,
// tools/make_fw_fixture.py). That image carries the prototype identity (LEGACY_FWID_MAGIC, LEGACY_FW_PRODUCT):
// (1) the scanner with the current marker must NOT find it, so a Wi-Fi update between the prototype firmware and
//     this project is rejected in both directions (the first flash is over USB);
// (2) the scanner with the prototype marker reads it, which checks the byte layout of a real image: the canTx
//     byte must give 0. Path relative to the firmware directory or from the first argument
//     (tools/run_host_tests.sh passes it explicitly).
static void testImage060(const char* path) {
    FILE* fp = std::fopen(path, "rb");
    check(fp != nullptr, "image 0.6.0: file opened");
    if (fp == nullptr) {
        std::printf("      no file %s\n", path);
        return;
    }
    const FwIdent ref = makeIdent(PRODUCT, 1);
    IdentScanner cur(ref.magic);            // the marker of this project
    IdentScanner old(LEGACY_FWID_MAGIC);    // the marker of the prototype firmware
    static uint8_t buf[1024];    // chunks as in an over-the-network update (OTA_CHUNK)
    size_t total = 0, n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
        cur.feed(buf, n);
        old.feed(buf, n);
        total += n;
    }
    std::fclose(fp);
    std::printf("      %s: %u B\n", path, (unsigned)total);
    check(memcmp(LEGACY_FWID_MAGIC, FW_IDENT_MAGIC, sizeof(LEGACY_FWID_MAGIC)) != 0,
          "image 0.6.0: the current marker differs from the prototype marker");
    check(!cur.found(), "image 0.6.0: prototype image not recognised as this product (no Wi-Fi update)");
    check(old.found(), "image 0.6.0: prototype marker found");
    if (!old.found()) return;
    const FwIdent& id = old.ident();
    std::printf("      product %.15s, version %.15s, built %.23s, mode %u, net %u, canTx %u, reserve %u\n",
                id.product, id.version, id.build, (unsigned)id.mode, (unsigned)id.net, (unsigned)id.canTx,
                (unsigned)id.reserved);
    check(fwIdentAcceptable(id, LEGACY_FW_PRODUCT, true), "image 0.6.0: prototype product with network");
    check(!fwIdentAcceptable(id, PRODUCT, true), "image 0.6.0: prototype product rejected by this firmware");
    check(memcmp(id.version, "0.6.0", 6) == 0, "image 0.6.0: version 0.6.0");
    check(id.mode == 1, "image 0.6.0: decoder mode");
    check(id.canTx == 0 && id.reserved == 0, "image 0.6.0: canTx = 0 (no transmit)");
    FwIdent run = ref;    // running firmware 0.7.0: decoder, canTx = 0
    check(!fwIdentModeChange(run, id.mode, id.canTx), "image 0.6.0: going back from 0.7.0 without asking about the mode");
}

int main(int argc, char** argv) {
    testBase64();
    testForm();
    testPasswords();
    testGuard();
    testSendDeadline();
    testJson();
    testIdent();
    testCanTx();
    testImage060(argc > 1 ? argv[1] : "test_data/fixtures/fw_ident_060.bin");
    if (failures == 0) std::printf("\nALL OK\n");
    else std::printf("\nERRORS: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
