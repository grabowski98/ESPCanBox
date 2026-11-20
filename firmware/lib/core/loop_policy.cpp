#include "loop_policy.h"

bool SaveDeferral::request(uint8_t what, bool carBusy) {
    // Button table: write and removal exclude each other, the last command stays.
    const uint8_t KEYMAP = KEYMAP_SAVE | KEYMAP_CLEAR;
    if (what & KEYMAP) pending_ &= (uint8_t)~KEYMAP;
    pending_ &= (uint8_t)~what;
    if (!carBusy) return true;      // write immediately; a pending one of this kind is already cancelled
    pending_ |= what;
    deferred_++;
    return false;
}

uint8_t SaveDeferral::takeDue(bool carBusy) {
    if (carBusy || pending_ == 0) return 0;
    for (uint8_t bit = 1; bit != 0; bit = (uint8_t)(bit << 1)) {
        if (pending_ & bit) {
            pending_ &= (uint8_t)~bit;
            return bit;
        }
    }
    return 0;
}
