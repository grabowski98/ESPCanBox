// Arming of CAN transmission and the related console commands. LOGIC ONLY, no hardware and
// no Arduino framework header: who may arm and from where, how to recognize a valid arming at startup, parsing of
// the command line. Test: test_host/can_tx_arm.cpp.
//
// Rules:
//   - arming is stored in NVS memory as the BUILD STRING of the program in which it was enabled
//     (FwIdent.build, e.g. "Nov 18 2026 01:33:28"). At startup: string equal to the string of the running
//     program = armed; different or missing = disarmed. Every newly flashed program starts disarmed;
//   - "cantx on" / "cantx off" and "vset on NN" / "vset off NN": from USB or from the web panel, but from the panel
//     only from a client connected to the unit's OWN network (never through the home network);
//   - "cantx reset" (clearing the latch) and "set NN VV" (test without the radio): USB only;
//   - plain "cantx" (status read): from anywhere.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace cantx {

const size_t BUILD_TAG_MAX = 24;      // size of the FwIdent::build field (including the terminating zero)

// Whether the stored string arms this program. Both strings are treated as untrusted: at most
// BUILD_TAG_MAX - 1 characters are compared; empty or too long (no zero within BUILD_TAG_MAX bytes) = disarmed.
bool armTagValid(const char* stored, const char* runningBuild);

enum class Source : uint8_t {
    USB,          // USB console
    PANEL_AP,     // web panel, client on the unit's own network (httpPeerIsAp)
    PANEL_OTHER,  // web panel through the home network
};

enum class Cmd : uint8_t {
    NONE,         // the line is none of these commands
    STATUS,       // "cantx"
    ARM_ON,       // "cantx on"
    ARM_OFF,      // "cantx off"
    RESET,        // "cantx reset"
    MASK_ON,      // "vset on NN" (old name "ust on NN" accepted too)
    MASK_OFF,     // "vset off NN" (old name "ust off NN" accepted too)
    SET,          // "set NN VV"
    BAD,          // starts like one of the commands but is malformed (e.g. "vset on", "set 1")
};

struct Parsed {
    Cmd cmd = Cmd::NONE;
    uint8_t id = 0;       // NN (hexadecimal, like the number in radio command 80)
    uint8_t value = 0;    // VV (hexadecimal)
};

// Parses the line. Hexadecimal numbers, 1-2 digits, no prefix. Plain "vset" (or "ust") is NONE (the settings table
// is handled elsewhere). Executes nothing.
Parsed parse(const char* line);

// Whether the command may be executed from the given source (rules in the file header).
bool allowed(Cmd cmd, Source from);

// Whether the command changes state (everything except NONE, STATUS, BAD).
bool changesState(Cmd cmd);

}  // namespace cantx
