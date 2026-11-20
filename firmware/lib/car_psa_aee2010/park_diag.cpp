#include "park_diag.h"

#include "vehicle_settings.h"

namespace psa {

void ParkDiag::begin(ParkDiagRecord* rec, bool wokeFromSleep) {
    r_ = rec ? rec : &own_;
    if (r_->magic != MAGIC) {                 // power-on (or never started): a fresh record
        *r_ = ParkDiagRecord();
        r_->magic = MAGIC;
    }
    woke_ = wokeFromSleep;
    if (woke_) r_->wakes++;
    compared_ = !woke_;                       // compare only after a wake-up
    havePrev_ = false;
}

bool ParkDiag::on126(const CarState& car, uint32_t nowMs, uint8_t& was, uint8_t& now) {
    const RawFrame& p = car.f126;
    uint8_t st = 0;
    // Counts only with the ignition seen on and settled (as the settings change), a frame from the bus and
    // a known value. Anything else breaks the "two equal frames in a row".
    const bool settled = car.ignitionOnSeen && !vset::ignitionJustOn(car, nowMs);
    if (!settled || !p.seen || p.pasted || p.len < 1 || !vset::parkingInhibited(p.d[0], st)) {
        havePrev_ = false;
        return false;
    }
    if (!havePrev_ || prev_ != st) {
        havePrev_ = true;
        prev_ = st;
        return false;
    }
    bool flipped = false;
    if (!compared_) {
        compared_ = true;
        if (r_->haveState && r_->state != st) {
            r_->flipsAsleep++;
            was = r_->state;
            now = st;
            flipped = true;
        }
    }
    r_->haveState = 1;
    r_->state = st;
    return flipped;
}

}  // namespace psa
