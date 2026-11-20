#include "can_tx_arm.h"

#include <string.h>

namespace cantx {

// String length if it ends with a zero within the first BUILD_TAG_MAX bytes; otherwise BUILD_TAG_MAX.
static size_t boundedLen(const char* s) {
    size_t n = 0;
    while (n < BUILD_TAG_MAX && s[n] != 0) n++;
    return n;
}

bool armTagValid(const char* stored, const char* runningBuild) {
    if (stored == nullptr || runningBuild == nullptr) return false;
    const size_t a = boundedLen(stored);
    const size_t b = boundedLen(runningBuild);
    if (a == 0 || a >= BUILD_TAG_MAX || b == 0 || b >= BUILD_TAG_MAX) return false;
    return a == b && memcmp(stored, runningBuild, a) == 0;
}

// One hexadecimal number of 1-2 digits starting at *p; moves p past it. false = no digit or too long.
static bool hexByte(const char*& p, uint8_t& out) {
    unsigned v = 0;
    int digits = 0;
    for (;; p++) {
        char c = *p;
        unsigned d;
        if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A' + 10);
        else break;
        v = v * 16 + d;
        if (++digits > 2) return false;
    }
    if (digits == 0) return false;
    out = (uint8_t)v;
    return true;
}

static void skipSpaces(const char*& p) {
    while (*p == ' ') p++;
}

Parsed parse(const char* line) {
    Parsed r;
    if (line == nullptr) return r;
    if (strcmp(line, "cantx") == 0) {
        r.cmd = Cmd::STATUS;
        return r;
    }
    if (strncmp(line, "cantx ", 6) == 0) {
        const char* p = line + 6;
        skipSpaces(p);
        if (strcmp(p, "on") == 0) r.cmd = Cmd::ARM_ON;
        else if (strcmp(p, "off") == 0) r.cmd = Cmd::ARM_OFF;
        else if (strcmp(p, "reset") == 0) r.cmd = Cmd::RESET;
        else r.cmd = Cmd::BAD;
        return r;
    }
    // "vset on|off NN". "ust" is the old (Polish) name of the same command, kept as an alias.
    const char* vsetArgs = nullptr;
    if (strncmp(line, "vset ", 5) == 0) vsetArgs = line + 5;
    else if (strncmp(line, "ust ", 4) == 0) vsetArgs = line + 4;
    if (vsetArgs != nullptr) {
        const char* p = vsetArgs;
        skipSpaces(p);
        bool on;
        if (strncmp(p, "on", 2) == 0 && (p[2] == ' ' || p[2] == 0)) {
            on = true;
            p += 2;
        } else if (strncmp(p, "off", 3) == 0 && (p[3] == ' ' || p[3] == 0)) {
            on = false;
            p += 3;
        } else {
            r.cmd = Cmd::BAD;
            return r;
        }
        skipSpaces(p);
        if (!hexByte(p, r.id) || *p != 0) {
            r.cmd = Cmd::BAD;
            return r;
        }
        r.cmd = on ? Cmd::MASK_ON : Cmd::MASK_OFF;
        return r;
    }
    if (strcmp(line, "set") == 0 || strncmp(line, "set ", 4) == 0) {
        const char* p = line + 3;
        skipSpaces(p);
        if (!hexByte(p, r.id) || *p != ' ') {
            r.cmd = Cmd::BAD;
            return r;
        }
        skipSpaces(p);
        if (!hexByte(p, r.value) || *p != 0) {
            r.cmd = Cmd::BAD;
            return r;
        }
        r.cmd = Cmd::SET;
        return r;
    }
    return r;
}

bool allowed(Cmd cmd, Source from) {
    switch (cmd) {
        case Cmd::STATUS:
            return true;
        case Cmd::ARM_ON:
        case Cmd::ARM_OFF:
        case Cmd::MASK_ON:
        case Cmd::MASK_OFF:
            return from == Source::USB || from == Source::PANEL_AP;
        case Cmd::RESET:
        case Cmd::SET:
            return from == Source::USB;
        default:
            return false;
    }
}

bool changesState(Cmd cmd) {
    return cmd == Cmd::ARM_ON || cmd == Cmd::ARM_OFF || cmd == Cmd::RESET || cmd == Cmd::MASK_ON ||
           cmd == Cmd::MASK_OFF || cmd == Cmd::SET;
}

}  // namespace cantx
