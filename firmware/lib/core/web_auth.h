// Helper functions for logging in to the web panel and reading form data.
// No hardware: checked by a host test (test_host/net_util.cpp).
//
// Rule for everything that comes from the network: input length and output capacity are always
// given explicitly, the result is always zero-terminated, and on error the output is empty.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace webauth {

const size_t PASSWORD_MIN = 10;      // WPA2 requires at least 8 characters; we take a margin
const size_t PASSWORD_MAX = 63;      // upper limit of a WPA2 password

// Base64 -> bytes. Returns the number of bytes, or SIZE_MAX when the input is invalid or the result
// does not fit in 'out'. Missing '=' characters at the end are allowed; whitespace is an error.
size_t base64Decode(const char* in, size_t inLen, uint8_t* out, size_t cap);

// Password comparison in time independent of the character at which they differ
// (the time depends only on the length of 'given'). Both strings zero-terminated.
bool constantTimeEquals(const char* given, const char* expected);

// Header "Authorization: Basic <base64(user:password)>". 'header' is the header value only.
// false = bad format or the data does not fit in the buffers.
bool parseBasic(const char* header, char* user, size_t userCap, char* pass, size_t passCap);

// Decodes %XX and '+' (space) from a URL or a form. Returns the length, or SIZE_MAX on error
// (malformed %XX, zero byte in the result, no room).
size_t urlDecode(const char* in, size_t inLen, char* out, size_t cap);

// Looks up field 'key' in text "a=1&b=2" and writes the decoded value to 'out'.
// false = no field, encoding error, or value longer than cap-1.
bool formValue(const char* body, size_t bodyLen, const char* key, char* out, size_t cap);

// Device password: PASSWORD_MIN..PASSWORD_MAX printable ASCII characters (0x21..0x7E, no space).
bool passwordAcceptable(const char* pass);

// Lockout after wrong passwords (change after the review of 2026-11-17): 5 failures in a 60 s window (counted
// from the first failure in the window) give a 30 s pause; each following lockout doubles the pause up to 5 minutes.
// A successful login does NOT clear the failures of the current window (otherwise the owner's polling panel
// would reset the counter between a guesser's attempts). After the lockout expires, onSuccess() only restores
// the first pause length (30 s), when at least FAIL_WINDOW_MS has passed since the end of the last lockout.
// Time is supplied from outside; differences are computed on unsigned numbers (robust to millis() wrap-around).
// Who is exempt from the lockout (clients of the device's own network) and which failures are not counted
// (the same password repeated) is decided by the caller (src/net/http_server.h, httpRequireAuth).
class LoginGuard {
public:
    void onFailure(uint32_t nowMs);
    void onSuccess(uint32_t nowMs);
    // Call before lockedForMs on every request (http_server.cpp): ends a lockout whose time has passed, so that
    // it cannot come back when the millisecond counter wraps (review: after 2^32 ms, about 49.7 days, an old
    // lockout looked active again). onFailure and onSuccess call it themselves.
    void poll(uint32_t nowMs);
    uint32_t lockedForMs(uint32_t nowMs) const;   // 0 = may try

    static const uint8_t FAILS_PER_LOCK = 5;
    static const uint32_t FAIL_WINDOW_MS = 60000;
    static const uint32_t FIRST_LOCK_MS = 30000;
    static const uint32_t MAX_LOCK_MS = 300000;

private:
    uint8_t fails_ = 0;            // failures in the current window
    uint32_t windowStartMs_ = 0;   // first failure of the current window
    uint32_t lockMs_ = 0;          // length of the current lockout (0 = none)
    uint32_t lockStartMs_ = 0;
    bool locked_ = false;          // the lockout started at lockStartMs_ is still running (cleared by poll)
    uint32_t nextLockMs_ = FIRST_LOCK_MS;
    uint32_t lastFailMs_ = 0;      // last failure (condition for restoring 30 s in onSuccess)
};

}  // namespace webauth
