#include "settings_changer.h"

#include <string.h>

#include "profile_psa_aee2010.h"

namespace vset {

void SettingsChanger::begin(CanTxGate* gate, uint32_t enabledMask) {
    gate_ = gate;
    mask_ = enabledMask;
    state_ = State::IDLE;
    havePending_ = false;
    haveReport_ = false;
    haveTxEvent_ = false;
    rel_ = Rel::NONE;
    haveRelEvent_ = false;
}

void SettingsChanger::noteTx(bool acked, uint32_t nowMs) {
    txEvent_.canId = button_ ? (uint16_t)ID_BUTTONS : (uint16_t)psa::ID_TX_SETTINGS;
    memcpy(txEvent_.frame, frame_, 8);
    txEvent_.acked = acked;
    txEvent_.handedOver = gate_ && gate_->handedOver();
    const uint32_t d = (uint32_t)(nowMs - tReq_);
    txEvent_.ms = d > 0xFFFF ? 0xFFFF : (uint16_t)d;
    haveTxEvent_ = true;
}

bool SettingsChanger::takeTxEvent(TxEvent& out) {
    if (!haveTxEvent_) return false;
    out = txEvent_;
    haveTxEvent_ = false;
    return true;
}

bool SettingsChanger::takeReleaseEvent(ReleaseEvent& out) {
    if (!haveRelEvent_) return false;
    out = relEvent_;
    haveRelEvent_ = false;
    return true;
}

void SettingsChanger::onRadioSetting(uint8_t id, uint8_t value, const CarState& car, const TxConditions& c,
                                     uint32_t nowMs) {
    // havePending_: an older command is waiting, the new one replaces it; rel_: a release frame is still due.
    if (state_ != State::IDLE || havePending_ || rel_ != Rel::NONE) {
        // A previous change is in progress: one waiting slot, the last command wins. An older waiting command
        // for another item is dropped with a report (REFUSED, busy); for the same item silently replaced.
        if (havePending_ && pendId_ != id) dropPending(nowMs);
        havePending_ = true;
        pendId_ = id;
        pendValue_ = value;
        return;
    }
    start(id, value, car, c, nowMs);
}

// Report for the waiting command that a newer command for another item pushed out of the slot. Does not touch
// the command in progress (state_, id_, frame_).
void SettingsChanger::dropPending(uint32_t nowMs) {
    report_ = ChangeReport();
    report_.id = pendId_;
    report_.value = pendValue_;
    report_.result = ChangeResult::REFUSED;
    report_.refuse = Refuse::OK;
    report_.txRefuse = TxRefuse::BUSY;
    report_.canId = isButton(pendId_) ? (uint16_t)ID_BUTTONS : (uint16_t)psa::ID_TX_SETTINGS;
    report_.haveFrame = false;
    report_.tookMs = 0;
    report_.ms = nowMs;
    haveReport_ = true;
    havePending_ = false;
}

void SettingsChanger::start(uint8_t id, uint8_t value, const CarState& car, const TxConditions& c, uint32_t nowMs) {
    id_ = id;
    value_ = value;
    tCmd_ = nowMs;
    tRetry_ = nowMs;
    attempt(car, c, nowMs);
}

// Steps 1-4: frame from the current 260 and request to the gate. Also called when retrying (WAIT_GATE),
// always on the current frame 260.
void SettingsChanger::attempt(const CarState& car, const TxConditions& c, uint32_t nowMs) {
    const bool notBuilt = !gate_ || gate_->state() == TxState::NOT_BUILT;
    // Button items (1A9) have their own path; in a build without transmit the existing refusal stays.
    button_ = !notBuilt && isButton(id_);
    if (button_) {
        attemptButton(car, c, nowMs);
        return;
    }
    Refuse r = build15B(car.f260, car.f361, nowMs, id_, value_, mask_, frame_);
    if (r != Refuse::OK) {
        finish(ChangeResult::REFUSED, r, notBuilt ? TxRefuse::NOT_BUILT : TxRefuse::OK, false, nowMs);
        return;
    }
    if (notBuilt) {                              // build without transmit: always refuse
        finish(ChangeResult::REFUSED, Refuse::OK, TxRefuse::NOT_BUILT, false, nowMs);
        return;
    }
    if (accepted(car.f260, id_, value_)) {
        finish(ChangeResult::ALREADY, Refuse::OK, TxRefuse::OK, false, nowMs);
        return;
    }
    CanFrame f;
    f.id = psa::ID_TX_SETTINGS;
    f.len = 8;
    memcpy(f.data, frame_, 8);
    TxRefuse t = gate_->request(f, c, nowMs);
    if (t == TxRefuse::OK) {
        memcpy(ref260_, &car.f260.d[1], 5);
        ref260Ms_ = car.f260.ms;
        haveNew260_ = false;
        tReq_ = nowMs;
        state_ = State::WAIT_TX;
        return;
    }
    // Temporary refusals: retry on every loop pass up to the limit, then report.
    const uint32_t waited = (uint32_t)(nowMs - tRetry_);
    if ((t == TxRefuse::CONTROLLER && waited < RETRY_CONTROLLER_MS) || (t == TxRefuse::BUSY && waited < RETRY_BUSY_MS)) {
        state_ = State::WAIT_GATE;
        return;
    }
    finish(ChangeResult::REFUSED, Refuse::OK, t, false, nowMs);
}

// Button item: one 1A9 frame toggles the state. We send only when the state from the bus is known,
// fresh and different from the requested one. No repetitions: a second frame would toggle it back.
void SettingsChanger::attemptButton(const CarState& car, const TxConditions& c, uint32_t nowMs) {
    Refuse r = buildButton(car, nowMs, id_, value_, mask_, frame_);
    if (r != Refuse::OK) {
        finish(ChangeResult::REFUSED, r, TxRefuse::OK, false, nowMs);
        return;
    }
    uint8_t st = 0;
    if (buttonState(car, nowMs, id_, st) && st == value_) {
        finish(ChangeResult::ALREADY, Refuse::OK, TxRefuse::OK, false, nowMs);
        return;
    }
#if CAN_TX_1A9_RELEASE
    // The press needs a release after it: refuse now if the per-minute limit has no room for both frames,
    // rather than leave the button "pressed" on the bus.
    if (gate_->rateFree(nowMs) < 2) {
        finish(ChangeResult::REFUSED, Refuse::OK, TxRefuse::RATE, false, nowMs);
        return;
    }
#endif
    CanFrame f;
    f.id = ID_BUTTONS;
    f.len = 8;
    memcpy(f.data, frame_, 8);
    TxRefuse t = gate_->request(f, c, nowMs);
    if (t == TxRefuse::OK) {
        haveNew260_ = false;
        tReq_ = nowMs;
        state_ = State::WAIT_TX;
        return;
    }
    const uint32_t waited = (uint32_t)(nowMs - tRetry_);
    if ((t == TxRefuse::CONTROLLER && waited < RETRY_CONTROLLER_MS) || (t == TxRefuse::BUSY && waited < RETRY_BUSY_MS)) {
        state_ = State::WAIT_GATE;
        return;
    }
    finish(ChangeResult::REFUSED, Refuse::OK, t, false, nowMs);
}

// Bits of frame 260 (bytes 2-5 and 6.7-6.4) changed relative to the frame before sending, outside the command field.
bool SettingsChanger::otherBitsChanged(uint8_t out[5]) const {
    uint8_t own[5];
    fieldMask(id_, own);
    bool any = false;
    for (int i = 0; i < 5; i++) {
        uint8_t diff = (uint8_t)((last260_[i] ^ ref260_[i]) & ~own[i]);
        if (i == 4) diff &= 0xF0;                // state bits 6.3-6.0 may change on their own
        out[i] = diff;
        if (diff) any = true;
    }
    return any;
}

// Release frame after a button press. Runs alongside WAIT_260 of the press; the gate is used by one of them
// at a time: the release is scheduled only after the press outcome was taken, and no new command starts
// while the release is due (onRadioSetting, IDLE below).
void SettingsChanger::tickRelease(const TxConditions& c, uint32_t nowMs) {
    if (rel_ == Rel::DUE) {
        const uint32_t since = (uint32_t)(nowMs - tPressAck_);
        if (since < RELEASE_DELAY_MS) return;
        CanFrame f;
        f.id = ID_BUTTONS;
        f.len = 8;
        releaseFrame(f.data);
        TxRefuse t = gate_->request(f, c, nowMs);
        if (t == TxRefuse::OK) {
            tRelReq_ = nowMs;
            rel_ = Rel::WAIT_TX;
            return;
        }
        // The gate pause after the press (BUSY) and a temporary controller refusal: retry on every pass,
        // at most RELEASE_GIVEUP_MS. Any other refusal (disarmed, no ignition, quiet bus ...): give up now.
        if ((t == TxRefuse::BUSY || t == TxRefuse::CONTROLLER) && since < RELEASE_DELAY_MS + RELEASE_GIVEUP_MS) return;
        endRelease(false, false, t, nowMs);
    } else if (rel_ == Rel::WAIT_TX) {
        TxOutcome o;
        if (gate_->takeOutcome(o)) endRelease(true, o == TxOutcome::ACKED, TxRefuse::OK, nowMs);
        else if ((uint32_t)(nowMs - tRelReq_) > TX_WAIT_MS) endRelease(true, false, TxRefuse::OK, nowMs);
    }
}

void SettingsChanger::endRelease(bool sent, bool acked, TxRefuse refuse, uint32_t nowMs) {
    relEvent_ = ReleaseEvent();
    releaseFrame(relEvent_.frame);
    relEvent_.sent = sent;
    relEvent_.acked = acked;
    relEvent_.handedOver = sent && gate_ && gate_->handedOver();
    relEvent_.refuse = refuse;
    const uint32_t d = sent ? (uint32_t)(nowMs - tRelReq_) : 0;
    relEvent_.ms = d > 0xFFFF ? 0xFFFF : (uint16_t)d;
    const uint32_t p = (uint32_t)(nowMs - tPressAck_);
    relEvent_.afterPressMs = p > 0xFFFF ? 0xFFFF : (uint16_t)p;
    haveRelEvent_ = true;
    rel_ = Rel::NONE;
}

void SettingsChanger::tick(const CarState& car, const TxConditions& c, uint32_t nowMs) {
    tickRelease(c, nowMs);
    switch (state_) {
        case State::IDLE:
            // The command that was waiting: from step 1, on the new 260; not while a release frame is due.
            if (havePending_ && rel_ == Rel::NONE) {
                havePending_ = false;
                start(pendId_, pendValue_, car, c, nowMs);
            }
            break;
        case State::WAIT_GATE:
            attempt(car, c, nowMs);
            break;
        case State::WAIT_TX: {
            TxOutcome o;
            if (gate_->takeOutcome(o)) {
                noteTx(o == TxOutcome::ACKED, nowMs);
                if (o == TxOutcome::ACKED) {
                    tAck_ = nowMs;
                    state_ = State::WAIT_260;
#if CAN_TX_1A9_RELEASE
                    if (button_) {               // only an acknowledged press gets a release
                        tPressAck_ = nowMs;
                        rel_ = Rel::DUE;
                    }
#endif
                } else {
                    finish(ChangeResult::TX_FAILED, Refuse::OK, TxRefuse::OK, true, nowMs);
                }
            } else if ((uint32_t)(nowMs - tReq_) > TX_WAIT_MS) {
                finish(ChangeResult::TX_FAILED, Refuse::OK, TxRefuse::OK, true, nowMs);
            }
            break;
        }
        case State::WAIT_260: {
            if (button_) {                       // button: wait until the state frame shows the requested value
                uint8_t st = 0;
                if (buttonState(car, nowMs, id_, st) && st == value_)
                    finish(ChangeResult::ACCEPTED, Refuse::OK, TxRefuse::OK, true, nowMs);
                else if ((uint32_t)(nowMs - tAck_) >= BUTTON_TIMEOUT_MS)
                    finish(ChangeResult::NOT_ACCEPTED, Refuse::OK, TxRefuse::OK, true, nowMs);
                break;
            }
            const RawFrame& f = car.f260;
            if (f.seen && !f.pasted && f.len == 7 && f.ms != ref260Ms_) {   // frame newer than the one 15B was built from
                haveNew260_ = true;
                memcpy(last260_, &f.d[1], 5);
                if (accepted(f, id_, value_)) {
                    uint8_t other[5];
                    const bool side = otherBitsChanged(other);
                    finish(side ? ChangeResult::SIDE_EFFECT : ChangeResult::ACCEPTED, Refuse::OK, TxRefuse::OK, true, nowMs);
                    break;
                }
            }
            if ((uint32_t)(nowMs - tAck_) >= ACCEPT_TIMEOUT_MS) {
                uint8_t other[5];
                const bool side = haveNew260_ && otherBitsChanged(other);
                finish(side ? ChangeResult::SIDE_EFFECT : ChangeResult::NOT_ACCEPTED, Refuse::OK, TxRefuse::OK, true, nowMs);
            }
            break;
        }
    }
}

void SettingsChanger::finish(ChangeResult result, Refuse refuse, TxRefuse txRefuse, bool haveFrame, uint32_t nowMs) {
    report_ = ChangeReport();
    report_.id = id_;
    report_.value = value_;
    report_.result = result;
    report_.refuse = refuse;
    report_.txRefuse = txRefuse;
    report_.canId = button_ ? (uint16_t)ID_BUTTONS : (uint16_t)psa::ID_TX_SETTINGS;
    report_.haveFrame = haveFrame;
    if (haveFrame) memcpy(report_.frame, frame_, 8);
    if (result == ChangeResult::SIDE_EFFECT && !button_) {
        otherBitsChanged(report_.otherBits);
        if (gate_) gate_->latchFault(TxFault::SIDE_EFFECT);
    }
    const uint32_t took = (uint32_t)(nowMs - tCmd_);
    report_.tookMs = took > 0xFFFF ? 0xFFFF : (uint16_t)took;
    report_.ms = nowMs;
    haveReport_ = true;
    state_ = State::IDLE;
}

bool SettingsChanger::takeReport(ChangeReport& out) {
    if (!haveReport_) return false;
    out = report_;
    haveReport_ = false;
    return true;
}

const char* changeResultName(ChangeResult r) {
    switch (r) {
        case ChangeResult::NONE: return "none";
        case ChangeResult::ALREADY: return "unchanged (the car already has this state)";
        case ChangeResult::ACCEPTED: return "ACCEPTED";
        case ChangeResult::NOT_ACCEPTED: return "NOT ACCEPTED";
        case ChangeResult::TX_FAILED: return "frame not acked";
        case ChangeResult::REFUSED: return "REFUSED";
        case ChangeResult::SIDE_EFFECT: return "other bits of 260 changed: transmit locked";
    }
    return "?";
}

}  // namespace vset
