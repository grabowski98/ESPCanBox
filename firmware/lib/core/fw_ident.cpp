#include "fw_ident.h"

#include <string.h>

// The structure layout is part of the contract with the panel (docs/PANEL_API.md, section on the unit marker in the file):
// 72 bytes, byte fields only, so the compiler inserts no padding.
static_assert(sizeof(FwIdent) == 72, "FwIdent must be 72 bytes");
// The canTx field sits where the first reserved byte was up to 0.6.0 (always 0): the old program does not read it,
// and an old image read by the new program gives canTx = 0 (no transmission).
static_assert(offsetof(FwIdent, mode) == 68 && offsetof(FwIdent, net) == 69 && offsetof(FwIdent, canTx) == 70,
              "FwIdent layout: mode 68, net 69, canTx 70");
static const size_t MAGIC_LEN = sizeof(((FwIdent*)nullptr)->magic);    // 12 (including the terminating zero)

void IdentScanner::reset() {
    ident_ = FwIdent();
    matched_ = 0;
    filled_ = 0;
    collecting_ = false;
    found_ = false;
}

void IdentScanner::feed(const uint8_t* data, size_t len) {
    if (data == nullptr || magic_ == nullptr) return;
    const uint8_t* m = (const uint8_t*)magic_;
    uint8_t* dst = (uint8_t*)&ident_;
    size_t i = 0;
    while (i < len && !found_) {
        if (collecting_) {
            // Collect the rest of the structure after the pattern; a chunk may end at any position.
            size_t take = sizeof(FwIdent) - filled_;
            if (take > len - i) take = len - i;
            memcpy(dst + filled_, data + i, take);
            filled_ += take;
            i += take;
            if (filled_ == sizeof(FwIdent)) {
                found_ = true;      // the first marker found wins
                collecting_ = false;
            }
            continue;
        }
        if (matched_ == 0) {
            // Fast skip to the nearest first byte of the pattern (the file is about 400 KB..3 MB).
            const void* hit = memchr(data + i, m[0], len - i);
            if (hit == nullptr) return;
            i = (size_t)((const uint8_t*)hit - data);
        }
        const uint8_t c = data[i++];
        if (c == m[matched_]) {
            matched_++;
        } else {
            // Mismatch after part of the pattern: look for the longest prefix of the pattern that ends
            // at this byte (the pattern has 0x7F at the start and at position 10, so a "false start"
            // 7F <magic> 7F 7F... must give a match from the second 7F).
            uint8_t k = matched_;    // new match length <= matched_
            while (k > 0) {
                // The matched bytes are m[0..matched_), the last k-1 of them are m[matched_-k+1 .. matched_).
                if (m[k - 1] == c && memcmp(m, m + matched_ - k + 1, (size_t)k - 1) == 0) break;
                k--;
            }
            matched_ = k;
        }
        if (matched_ == MAGIC_LEN) {
            memcpy(dst, m, MAGIC_LEN);
            filled_ = MAGIC_LEN;
            collecting_ = true;
            matched_ = 0;
        }
    }
}

// Strings in the found structure are untrusted: we compare only within the field.
bool fwIdentAcceptable(const FwIdent& found, const char* product, bool requireNet) {
    if (product == nullptr) return false;
    const size_t cap = sizeof(found.product);
    const size_t plen = strlen(product);
    if (plen == 0 || plen >= cap) return false;
    if (memcmp(found.product, product, plen) != 0) return false;
    if (found.product[plen] != '\0') return false;    // longer product or no zero in the field
    if (requireNet && found.net != 1) return false;
    return true;
}
