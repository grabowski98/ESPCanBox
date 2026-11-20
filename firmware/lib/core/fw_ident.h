// Firmware identity marker: a small structure compiled into the program image.
// During an update over the network the receiver looks for it in the uploaded file and rejects files
// that are not the unit's program (a common mistake: flashing another .bin file or a version without Wi-Fi,
// after which the device could no longer be updated without a cable).
//
// This is NOT a signature: it does not protect against a deliberately crafted file. It protects against a mistake.
// Image integrity (checksum and SHA-256) is checked separately by ESP-IDF.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "project_identity.h"   // PROJECT_FWID_MAGIC (include/, from project.json)

struct FwIdent {
    char magic[12];        // fixed recognition string
    char product[16];      // PROJECT_FW_PRODUCT (include/project_identity.h), zero-terminated
    char version[16];      // e.g. "0.6.0"
    char build[24];        // build date and time
    uint8_t mode;          // 0 = alongside the box, 1 = decoder
    uint8_t net;           // 1 = the image has the web panel and network update
    uint8_t canTx;         // 1 = the image can transmit on the car's CAN (since 0.7.0; in older images the byte = 0)
    uint8_t reserved;
};

// Whether switching from the running program to another image requires a separate confirmation (parameter mode=1,
// response 409 mode_change): a different operating mode OR a different CAN transmit capability (in both directions).
// Bytes from a foreign image are untrusted: any difference in value counts as a change.
inline bool fwIdentModeChange(const FwIdent& running, uint8_t otherMode, uint8_t otherCanTx) {
    return otherMode != running.mode || otherCanTx != running.canTx;
}

// Recognition string. It starts and ends with byte 0x7F so that it cannot occur in ordinary text.
// EXACTLY ONE copy of it must exist in the program image (in the FwIdent structure); that is why the scanner
// gets a pointer to the pattern rather than its own constant.
#define FW_IDENT_MAGIC "\x7F" PROJECT_FWID_MAGIC "\x7F" /* 11 characters + zero = 12 bytes */

// Looks for the FwIdent structure in a byte stream fed in chunks of any size.
class IdentScanner {
public:
    // 'magic' points to the 12 bytes of the pattern (the magic field of the reference structure).
    explicit IdentScanner(const char* magic) : magic_(magic) {}

    void feed(const uint8_t* data, size_t len);
    bool found() const { return found_; }
    const FwIdent& ident() const { return ident_; }    // valid only when found()
    void reset();

private:
    const char* magic_;
    FwIdent ident_ = {};
    uint8_t matched_ = 0;      // how many pattern bytes already match
    size_t filled_ = 0;        // how many structure bytes were collected after the pattern matched
    bool collecting_ = false;
    bool found_ = false;
};

// Whether the found image may be flashed: matching product and, with requireNet, also the network panel present.
// Strings in the structure are treated as untrusted (they may lack the terminating zero).
bool fwIdentAcceptable(const FwIdent& found, const char* product, bool requireNet);
