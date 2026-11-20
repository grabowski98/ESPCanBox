#include "xp_keys.h"

namespace xp {

namespace {

// Signed difference of 8-bit counters: +1 = one detent up, -1 = down.
int8_t counterDelta(uint8_t now, uint8_t before) { return (int8_t)(uint8_t)(now - before); }

}  // namespace

void KeyEncoder::configure(const vehicle::CarProfile* car, CodeFn codeOf) {
    car_ = car;
    codeOf_ = codeOf;
}

uint8_t KeyEncoder::keyCount() const {
    if (!car_ || !codeOf_ || !car_->keys) return 0;
    return car_->keyCount < MAX_KEYS ? car_->keyCount : MAX_KEYS;
}

uint8_t KeyEncoder::codeAt(uint8_t index) const { return codeOf_(car_->keys[index].key); }

void KeyEncoder::push(uint8_t code, uint8_t state) {
    if (count_ >= QUEUE) {
        dropped_++;
        return;
    }
    uint8_t tail = (uint8_t)((head_ + count_) % QUEUE);
    queue_[tail].code = code;
    queue_[tail].state = state;
    count_++;
}

bool KeyEncoder::pop(KeyEvent& out) {
    if (count_ == 0) return false;
    out = queue_[head_];
    head_ = (uint8_t)((head_ + 1) % QUEUE);
    count_--;
    return true;
}

void KeyEncoder::setLevel(uint8_t index, bool down, uint32_t nowMs) {
    Slot& s = slots_[index];
    s.canDown = down;
    s.lastFrameMs = nowMs;
    if (down && !s.down) {                       // new press: straight to the radio
        s.down = true;
        s.nextTickMs = nowMs + TICK_MS;
        push(codeAt(index), KEY_STATE_PRESSED);
    }
    // the release is reported by update() on the next 200 ms tick
}

void KeyEncoder::onCanFrame(const CanFrame& f, uint32_t nowMs) {
    // Level buttons, in the row order of the car profile (= order of the events for the radio).
    const uint8_t n = keyCount();
    for (uint8_t i = 0; i < n; i++) {
        const vehicle::KeyMapEntry& k = car_->keys[i];
        if (k.canId != f.id || k.byteIndex >= f.len) continue;
        if (codeAt(i) == 0) continue;            // this radio has no code for the button
        setLevel(i, (f.data[k.byteIndex] & k.mask) == k.value, nowMs);
    }

    // Knobs (counters), in row order.
    if (!car_ || !codeOf_ || !car_->knobs) return;
    for (uint8_t j = 0; j < car_->knobCount && j < MAX_KNOBS; j++) {
        const vehicle::KnobEntry& kn = car_->knobs[j];
        if (kn.canId != f.id || f.len < kn.byteIndex + 1) continue;
        uint8_t v = f.data[kn.byteIndex];
        if (knobPrev_[j] >= 0) {
            int8_t d = counterDelta(v, (uint8_t)knobPrev_[j]);
            const uint8_t code = codeOf_(d > 0 ? kn.onIncrease : kn.onDecrease);
            // Each detent separately; the limit of 4 protects against a burst after lost frames.
            if (code != 0)
                for (int8_t m = 0; m < 4 && m < (d < 0 ? -d : d); m++) push(code, KEY_STATE_PRESSED);
        }
        knobPrev_[j] = v;
    }
}

void KeyEncoder::update(uint32_t nowMs) {
    const uint8_t n = keyCount();
    for (uint8_t i = 0; i < n; i++) {
        Slot& s = slots_[i];
        if (!s.down) continue;
        if ((int32_t)(nowMs - s.nextTickMs) < 0) continue;
        if (s.canDown && (uint32_t)(nowMs - s.lastFrameMs) >= STALE_MS) s.canDown = false;   // no frame: released
        if (s.canDown) {
            push(codeAt(i), KEY_STATE_HELD);
            s.nextTickMs += TICK_MS;
        } else {
            push(codeAt(i), KEY_STATE_RELEASED);
            s.down = false;
        }
    }
}

}  // namespace xp
