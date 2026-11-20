#include "web_auth.h"

#include <string.h>

namespace webauth {

// Value of a base64 character, or -1 for a character outside the alphabet (also whitespace and '=').
static int b64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

// Zeroing a password buffer so that the compiler does not optimise it away.
static void wipe(void* p, size_t n) {
    volatile uint8_t* v = (volatile uint8_t*)p;
    while (n--) *v++ = 0;
}

size_t base64Decode(const char* in, size_t inLen, uint8_t* out, size_t cap) {
    if (in == nullptr || (out == nullptr && cap > 0)) return SIZE_MAX;
    // '=' characters are allowed only at the end (at most two); then the whole must be a multiple of 4.
    size_t dataLen = inLen;
    size_t pad = 0;
    while (dataLen > 0 && in[dataLen - 1] == '=' && pad < 2) {
        dataLen--;
        pad++;
    }
    if (pad > 0 && (inLen % 4) != 0) return SIZE_MAX;
    if ((dataLen % 4) == 1) return SIZE_MAX;               // a single character in a group carries no byte
    if (pad > 0 && (dataLen % 4) + pad != 4) return SIZE_MAX;

    const size_t outLen = (dataLen / 4) * 3 + ((dataLen % 4) == 0 ? 0 : (dataLen % 4) - 1);
    if (outLen > cap) return SIZE_MAX;

    uint32_t acc = 0;
    int bits = 0;
    size_t o = 0;
    for (size_t i = 0; i < dataLen; i++) {
        int v = b64Value(in[i]);
        if (v < 0) return SIZE_MAX;    // also '=' in the middle and whitespace
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (uint8_t)(acc >> bits);
            acc &= (1u << bits) - 1;
        }
    }
    return o;
}

// An empty 'expected' always gives false: a missing device password must not open the panel (fail-closed).
bool constantTimeEquals(const char* given, const char* expected) {
    if (given == nullptr || expected == nullptr || expected[0] == '\0') return false;
    volatile uint8_t diff = 0;
    size_t e = 0;
    for (size_t i = 0; given[i] != '\0'; i++) {
        uint8_t x = (uint8_t)expected[e];
        diff |= (uint8_t)((uint8_t)given[i] ^ x);
        e += (x != 0);    // at the end of 'expected' we stay on the zero; loop time depends only on 'given'
    }
    diff |= (uint8_t)expected[e];    // 'expected' longer than 'given'
    return diff == 0;
}

static bool isBlank(char c) { return c == ' ' || c == '\t'; }

bool parseBasic(const char* header, char* user, size_t userCap, char* pass, size_t passCap) {
    if (user == nullptr || pass == nullptr || userCap == 0 || passCap == 0) return false;
    user[0] = '\0';
    pass[0] = '\0';
    if (header == nullptr) return false;

    const char* p = header;
    while (isBlank(*p)) p++;
    // Scheme name is case-insensitive (RFC 7617).
    static const char SCHEME[] = "basic";
    for (size_t i = 0; i < 5; i++) {
        char c = p[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != SCHEME[i]) return false;
    }
    p += 5;
    if (!isBlank(*p)) return false;
    while (isBlank(*p)) p++;
    size_t len = strlen(p);
    while (len > 0 && isBlank(p[len - 1])) len--;
    if (len == 0) return false;

    uint8_t dec[192];    // user:password; more than 63 + 1 + a reasonable name is not needed
    size_t n = base64Decode(p, len, dec, sizeof(dec));
    if (n == SIZE_MAX) return false;

    size_t colon = SIZE_MAX;
    bool ok = true;
    for (size_t i = 0; i < n; i++) {
        if (dec[i] == 0) ok = false;    // zero byte in the text: reject
        if (dec[i] == ':' && colon == SIZE_MAX) colon = i;
    }
    if (colon == SIZE_MAX) ok = false;
    if (ok && (colon + 1 > userCap || (n - colon - 1) + 1 > passCap)) ok = false;
    if (ok) {
        memcpy(user, dec, colon);
        user[colon] = '\0';
        memcpy(pass, dec + colon + 1, n - colon - 1);
        pass[n - colon - 1] = '\0';
    }
    wipe(dec, sizeof(dec));
    return ok;
}

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

size_t urlDecode(const char* in, size_t inLen, char* out, size_t cap) {
    if (out == nullptr || cap == 0) return SIZE_MAX;
    out[0] = '\0';
    if (in == nullptr) return SIZE_MAX;
    size_t o = 0;
    for (size_t i = 0; i < inLen; i++) {
        char c = in[i];
        if (c == '+') c = ' ';
        else if (c == '%') {
            if (i + 2 >= inLen) {    // two digits missing after '%'
                out[0] = '\0';
                return SIZE_MAX;
            }
            int hi = hexValue(in[i + 1]);
            int lo = hexValue(in[i + 2]);
            if (hi < 0 || lo < 0) {
                out[0] = '\0';
                return SIZE_MAX;
            }
            c = (char)((hi << 4) | lo);
            i += 2;
        }
        if (c == '\0' || o + 1 >= cap) {    // zero byte in the result or no room for the character and the terminator
            out[0] = '\0';
            return SIZE_MAX;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    return o;
}

bool formValue(const char* body, size_t bodyLen, const char* key, char* out, size_t cap) {
    if (out == nullptr || cap == 0) return false;
    out[0] = '\0';
    if (body == nullptr || key == nullptr) return false;
    const size_t keyLen = strlen(key);
    size_t start = 0;
    while (start <= bodyLen) {
        size_t end = start;
        while (end < bodyLen && body[end] != '&') end++;
        // Pair "name=value" in body[start..end). A pair without '=' is not a field.
        size_t eq = start;
        while (eq < end && body[eq] != '=') eq++;
        if (eq < end && eq - start == keyLen && memcmp(body + start, key, keyLen) == 0) {
            return urlDecode(body + eq + 1, end - eq - 1, out, cap) != SIZE_MAX;
        }
        start = end + 1;
    }
    return false;
}

bool passwordAcceptable(const char* pass) {
    if (pass == nullptr) return false;
    size_t n = 0;
    while (pass[n] != '\0') {
        if (n >= PASSWORD_MAX) return false;    // we do not read beyond 64 characters
        uint8_t c = (uint8_t)pass[n];
        if (c < 0x21 || c > 0x7E) return false;
        n++;
    }
    return n >= PASSWORD_MIN;
}

// ---------------------------------------------------------------- LoginGuard

void LoginGuard::poll(uint32_t nowMs) {
    if (locked_ && (uint32_t)(nowMs - lockStartMs_) >= lockMs_) locked_ = false;
}

void LoginGuard::onFailure(uint32_t nowMs) {
    poll(nowMs);
    if (lockedForMs(nowMs) > 0) return;    // during a lockout passwords are not checked anyway
    if (fails_ == 0 || (uint32_t)(nowMs - windowStartMs_) >= FAIL_WINDOW_MS) {
        windowStartMs_ = nowMs;            // new window from the first failure
        fails_ = 0;
    }
    fails_++;
    lastFailMs_ = nowMs;
    if (fails_ >= FAILS_PER_LOCK) {
        lockMs_ = nextLockMs_;
        lockStartMs_ = nowMs;
        locked_ = true;
        nextLockMs_ = nextLockMs_ >= MAX_LOCK_MS / 2 ? MAX_LOCK_MS : nextLockMs_ * 2;
        fails_ = 0;
    }
}

// A successful login does NOT clear the failures of the window. It only restores the first lockout length when
// at least FAIL_WINDOW_MS has passed since the end of the last lockout and since the last failure.
void LoginGuard::onSuccess(uint32_t nowMs) {
    poll(nowMs);
    if (lockMs_ == 0) return;
    if (lockedForMs(nowMs) > 0) return;
    const uint32_t lockEnd = lockStartMs_ + lockMs_;
    if ((uint32_t)(nowMs - lockEnd) < FAIL_WINDOW_MS) return;
    if (fails_ > 0 && (uint32_t)(nowMs - lastFailMs_) < FAIL_WINDOW_MS) return;
    lockMs_ = 0;
    nextLockMs_ = FIRST_LOCK_MS;
}

uint32_t LoginGuard::lockedForMs(uint32_t nowMs) const {
    if (lockMs_ == 0 || !locked_) return 0;
    const uint32_t elapsed = nowMs - lockStartMs_;    // unsigned: robust to millis() wrap-around
    return elapsed >= lockMs_ ? 0 : lockMs_ - elapsed;
}

}  // namespace webauth
