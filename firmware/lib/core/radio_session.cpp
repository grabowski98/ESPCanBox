#include "radio_session.h"

#include <string.h>

void RadioSession::push(const RadioFrame& f, bool alwaysSend) {
    // Without ACC the radio is off: we send it nothing except the power information.
    // This way keys pressed while the radio is off do not wait in the queue for it to start.
    if (!accOn_ && !alwaysSend) {
        gated_++;
        return;
    }
    if (count_ >= QUEUE) {
        dropped_++;
        if (!alwaysSend) return;
        // The power frame is never lost (review: with a full queue it used to be dropped silently, and accOn_ was
        // already changed, so nobody sent it again): the oldest waiting frame makes room for it instead.
        head_ = (uint8_t)((head_ + 1) % QUEUE);
        count_--;
    }
    queue_[(uint8_t)((head_ + count_) % QUEUE)] = f;
    count_++;
}

bool RadioSession::popTx(RadioFrame& out) {
    if (count_ == 0) return false;
    out = queue_[head_];
    head_ = (uint8_t)((head_ + 1) % QUEUE);
    count_--;
    return true;
}

void RadioSession::pushVersion() {
    RadioFrame f;
    if (rp_ && rp_->encodeVersion && rp_->encodeVersion(f)) push(f);
}

void RadioSession::onKey(const RadioKeyEvent& e) {
    RadioFrame f;
    if (rp_ && rp_->encodeKey && rp_->encodeKey(e, f)) push(f);
}

void RadioSession::onParking(const vehicle::ParkingState& pa, bool valid, uint32_t nowMs) {
    RadioFrame f;
    if (!rp_ || !rp_->encodeParking || !rp_->encodeParking(pa, valid, f)) return;
    // "Active" = the sensors report something to show: only then the frame is repeated (update()).
    bool active = valid && pa.anyActive();
    bool changed = !haveParking_ || lastParking_.cmd != f.cmd || lastParking_.len != f.len ||
                   memcmp(f.data, lastParking_.data, f.len) != 0;
    // We send on every change; update() does the repeat.
    if (changed) {
        lastParking_ = f;
        haveParking_ = true;
        parkingActive_ = active;
        lastParkingTxMs_ = nowMs;
        push(f);
    }
}

// Same state slot: same frame number and, when the protocol keys its state frames by sub-type bytes
// (RadioProtocol::stateKeyBytes), the same first data bytes. XP: 0, i.e. the frame number alone, as before.
bool RadioSession::sameSlot(const RadioFrame& a, const RadioFrame& b) const {
    if (a.cmd != b.cmd) return false;
    uint8_t k = rp_ ? rp_->stateKeyBytes : 0;
    if (k > STATE_KEY_MAX) k = STATE_KEY_MAX;
    for (uint8_t i = 0; i < k; i++) {
        const bool ha = i < a.len, hb = i < b.len;
        if (ha != hb) return false;
        if (ha && a.data[i] != b.data[i]) return false;
    }
    return true;
}

void RadioSession::onState(const RadioFrame& f, uint32_t nowMs) {
    StateSlot* slot = nullptr;
    for (uint8_t i = 0; i < STATE_SLOTS; i++) {
        if (states_[i].used && sameSlot(states_[i].frame, f)) {
            slot = &states_[i];
            break;
        }
        if (!states_[i].used && !slot) slot = &states_[i];
    }
    if (!slot) {                                    // more frame kinds than slots: skip
        slotsFull_++;
        return;
    }
    if (slot->used && slot->frame.len == f.len && memcmp(slot->frame.data, f.data, f.len) == 0) return;
    slot->used = true;
    slot->frame = f;
    slot->dirty = true;
    (void)nowMs;
}

bool RadioSession::resendState(uint8_t cmd) {
    // Every slot with this frame number (more than one only when the protocol keys its state frames by
    // sub-type bytes, RadioProtocol::stateKeyBytes), in slot order.
    bool found = false;
    for (uint8_t i = 0; i < STATE_SLOTS; i++) {
        StateSlot& s = states_[i];
        if (!s.used || s.frame.cmd != cmd) continue;
        if (accOn_) {              // without ACC push() would reject it: it stays "to be sent" for later
            s.dirty = false;
            push(s.frame);
        }
        found = true;
    }
    return found;
}

uint16_t RadioSession::minInterval(uint8_t cmd) const {
    if (!rp_ || !rp_->rateLimits) return 0;
    for (uint8_t i = 0; i < rp_->rateLimitCount; i++)
        if (rp_->rateLimits[i].cmd == cmd) return rp_->rateLimits[i].minMs;
    return 0;
}

void RadioSession::flushStates(uint32_t nowMs, bool resendAll) {
    if (!accOn_) return;
    for (uint8_t i = 0; i < STATE_SLOTS; i++) {
        StateSlot& s = states_[i];
        if (!s.used) continue;
        if (!resendAll) {
            if (!s.dirty) continue;
            const uint16_t minMs = minInterval(s.frame.cmd);
            if (minMs != 0 && (uint32_t)(nowMs - s.lastTxMs) < minMs) continue;
        }
        s.dirty = false;
        s.lastTxMs = nowMs;
        push(s.frame);
    }
}

void RadioSession::onPower(bool accOn, uint32_t nowMs) {
    (void)nowMs;
    if (accOn == accOn_) return;
    accOn_ = accOn;
    RadioFrame f;
    if (rp_ && rp_->encodePower && rp_->encodePower(accOn, f)) push(f, true);
    // After ACC off the remembered parking frame is forgotten. NOTE (code review 19.11.2026): this does NOT mean
    // "the state is sent again after ACC on". In the same pass the Decoder reports stale sensors (onParking with
    // valid = false), that frame is remembered although push() rejects it without ACC, and after ACC on an
    // identical frame counts as unchanged. Kept on purpose: sending it again would change the XP stream
    // recorded from the box (golden test: one more frame 32 after ACC on). The radio gets the sensor state when
    // it asks for everything (RADIO_RX_SEND_ALL, XP 90 71) and on the next change.
    if (!accOn) haveParking_ = false;
}

void RadioSession::onRadioFrame(const RadioFrame& f, uint32_t nowMs) {
    info_.everHeard = true;
    info_.lastFrameMs = nowMs;
    const uint8_t flags = (rp_ && rp_->decodeRadioFrame) ? rp_->decodeRadioFrame(f, info_) : 0;
    // The radio asks for everything (after startup): version, the sensor state right away (the radio needs it
    // for the chart) and every state frame.
    if (flags & RADIO_RX_SEND_ALL) {
        pushVersion();
        if (haveParking_) push(lastParking_);
        flushStates(nowMs, true);
    }
}

void RadioSession::update(uint32_t nowMs) {
    flushStates(nowMs, false);
    // While the sensors are active the parking frame is repeated (XP: about every 1 s, like the box) so that
    // the radio does not close the chart.
    if (rp_ && rp_->parkingRepeatMs != 0 && accOn_ && haveParking_ && parkingActive_ &&
        (uint32_t)(nowMs - lastParkingTxMs_) >= rp_->parkingRepeatMs) {
        lastParkingTxMs_ = nowMs;
        push(lastParking_);
    }
}
