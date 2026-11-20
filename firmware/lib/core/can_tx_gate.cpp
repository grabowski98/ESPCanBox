#include "can_tx_gate.h"

// All time differences are computed by unsigned subtraction: the result is correct also when the
// millisecond counter wraps through zero (after 49.7 days).

TxRefuse CanTxGate::checkFrame(const vehicle::CarProfile* car, const CanFrame& f) {
    if (!car || !car->txAllow) return TxRefuse::ID_NOT_ALLOWED;
    for (uint8_t i = 0; i < car->txAllowCount; i++) {
        const vehicle::TxAllowEntry& e = car->txAllow[i];
        if (e.canId != f.id) continue;                       // f.id is 32-bit: a value above 0xFFFF never matches a row
        if (e.len != 0 && f.len != e.len) return TxRefuse::BAD_LEN;
        if (f.len > 8) return TxRefuse::ID_NOT_ALLOWED;      // the validator never sees more than 8 data bytes
        if (!e.validator || !e.validator(f)) return TxRefuse::ID_NOT_ALLOWED;
        return TxRefuse::OK;
    }
    return TxRefuse::ID_NOT_ALLOWED;
}

void CanTxGate::begin(CanTxPort* port, const vehicle::CarProfile* car) {
    port_ = port;
    car_ = car;
    open_ = false;
    haveOutcome_ = false;
    failedInRow_ = 0;
    stats_ = TxStats();
    for (uint8_t i = 0; i < RATE_MAX; i++) rateUsed_[i] = false;
    if (!port_) {
        state_ = TxState::NOT_BUILT;
        return;
    }
    port_->transmitter(false);
    state_ = TxState::DISARMED;
}

void CanTxGate::setArmed(bool on) {
    if (state_ == TxState::NOT_BUILT) return;
    if (on) {
        if (state_ == TxState::DISARMED)
            state_ = stats_.fault != TxFault::NONE ? TxState::FAULT : TxState::IDLE;
        return;
    }
    // Disarming: transmitter disabled, the current send abandoned.
    if (open_) {
        port_->transmitter(false);
        open_ = false;
        outcome_ = TxOutcome::FAILED;
        haveOutcome_ = true;
    }
    state_ = TxState::DISARMED;
}

void CanTxGate::pruneRate(uint32_t nowMs) {
    for (uint8_t i = 0; i < RATE_MAX; i++)
        if (rateUsed_[i] && (uint32_t)(nowMs - rateMs_[i]) >= RATE_WINDOW_MS) rateUsed_[i] = false;
}

void CanTxGate::readHealth(CanHealth& h) {
    h = port_->health();
    stats_.tec = h.tec;
    stats_.rec = h.rec;
}

TxRefuse CanTxGate::request(const CanFrame& f, const TxConditions& c, uint32_t nowMs) {
    TxRefuse r = TxRefuse::OK;
    if (state_ == TxState::NOT_BUILT || !port_) r = TxRefuse::NOT_BUILT;
    else if (state_ == TxState::DISARMED) r = TxRefuse::DISARMED;
    else if (state_ == TxState::FAULT) r = TxRefuse::FAULT;
    else r = checkFrame(car_, f);                                   // allow list of the car profile
    if (r == TxRefuse::OK) {
        if (state_ != TxState::IDLE) r = TxRefuse::BUSY;
        else if (!c.ignitionOk) r = TxRefuse::IGNITION;
        else if (!c.busAlive) r = TxRefuse::BUS_QUIET;
        else if (c.flashBusy || c.nvsPending) r = TxRefuse::FLASH_BUSY;
    }
    if (r == TxRefuse::OK) {
        CanHealth h;
        readHealth(h);
        if (h.state == 2) {                 // bus-off: transmitter event, latch
            latchFault(TxFault::BUS_OFF);
            r = TxRefuse::FAULT;
        } else if (h.tec >= TEC_LATCH) {
            latchFault(TxFault::TEC);
            r = TxRefuse::FAULT;
        } else if (h.state != 1 || h.rec != 0 ||
                   (h.busErrors != 0 && (uint32_t)(nowMs - h.msLastBusError) < BUS_ERROR_QUIET_MS)) {
            r = TxRefuse::CONTROLLER;       // temporary refusal, no latch
        }
    }
    if (r == TxRefuse::OK) {
        pruneRate(nowMs);
        uint8_t freeSlot = RATE_MAX;
        for (uint8_t i = 0; i < RATE_MAX; i++)
            if (!rateUsed_[i]) freeSlot = i;
        if (freeSlot == RATE_MAX) {
            r = TxRefuse::RATE;
        } else {
            rateUsed_[freeSlot] = true;
            rateMs_[freeSlot] = nowMs;
        }
    }
    if (r != TxRefuse::OK) {
        stats_.refused++;
        return r;
    }
    frame_ = f;
    arbTries_ = 0;
    haveOutcome_ = false;
    handedOver_ = false;
    port_->transmitter(true);               // SLNT low: from now on the unit is visible on the bus
    open_ = true;
    tOpen_ = nowMs;
    state_ = TxState::OPENING;
    return TxRefuse::OK;
}

// Disables the transmitter. If it was enabled for longer than WINDOW_MS (the main loop stalled), records
// the latch reason, even when the frame itself was acknowledged.
void CanTxGate::close(uint32_t nowMs) {
    if (!open_) return;
    port_->transmitter(false);
    open_ = false;
    tClosed_ = nowMs;
    if ((uint32_t)(nowMs - tOpen_) > WINDOW_MS && stats_.fault == TxFault::NONE) stats_.fault = TxFault::WINDOW;
}

// End of a send: transmitter disabled, result ready to be collected, then a pause or the latch.
void CanTxGate::finish(TxOutcome o, bool countFail, uint32_t nowMs) {
    close(nowMs);
    outcome_ = o;
    haveOutcome_ = true;
    if (o == TxOutcome::ACKED) {
        stats_.acked++;
        failedInRow_ = 0;
    } else {
        stats_.failed++;
        if (countFail && ++failedInRow_ >= FAIL_LATCH && stats_.fault == TxFault::NONE)
            stats_.fault = TxFault::THREE_FAILED;
    }
    state_ = stats_.fault != TxFault::NONE ? TxState::FAULT : TxState::COOLDOWN;
}

void CanTxGate::tick(const TxConditions& c, uint32_t nowMs) {
    if (state_ == TxState::NOT_BUILT || !port_) return;
    pruneRate(nowMs);
    const uint32_t openFor = (uint32_t)(nowMs - tOpen_);
    switch (state_) {
        case TxState::OPENING:
            if (openFor >= WINDOW_MS) {                      // the loop stalled with the transmitter enabled
                if (stats_.fault == TxFault::NONE) stats_.fault = TxFault::WINDOW;
                finish(TxOutcome::TIMEOUT, false, nowMs);
            } else if (c.flashBusy) {                        // nothing has been sent yet
                finish(TxOutcome::FAILED, false, nowMs);
            } else if (openFor >= OPEN_SETTLE_MS) {
                // The transceiver gets time to leave silent mode. Two ticks of the millisecond counter are
                // at least 1 ms of real time (a single tick may mean a fraction of a ms).
                if (port_->send(frame_)) {
                    stats_.sent++;
                    handedOver_ = true;
                    state_ = TxState::SENDING;
                } else {
                    finish(TxOutcome::FAILED, true, nowMs);
                }
            }
            break;
        case TxState::SENDING: {
            const uint8_t p = port_->poll();
            if (p == 1) {
                finish(TxOutcome::ACKED, false, nowMs);
            } else if (p == 2) {
                finish(TxOutcome::FAILED, true, nowMs);
            } else if (p == 4) {
                if (stats_.fault == TxFault::NONE) stats_.fault = TxFault::CONTROLLER_EVENT;
                finish(TxOutcome::FAILED, false, nowMs);
            } else if (p == 3) {
                // Lost arbitration is not an error: retry within the same window, at most ARB_RETRIES times.
                if (openFor < WINDOW_MS && arbTries_ < ARB_RETRIES && !c.flashBusy) {
                    arbTries_++;
                    if (port_->send(frame_)) stats_.sent++;
                    else finish(TxOutcome::FAILED, true, nowMs);
                } else {
                    finish(TxOutcome::FAILED, false, nowMs);
                }
            } else if (openFor >= WINDOW_MS) {
                // The frame is still waiting in the controller and the API cannot withdraw it: after the
                // transmitter is disabled it will end with a bit error. Hence the immediate latch.
                if (stats_.fault == TxFault::NONE) stats_.fault = TxFault::WINDOW;
                finish(TxOutcome::TIMEOUT, false, nowMs);
            } else if (c.flashBusy) {
                finish(TxOutcome::FAILED, true, nowMs);
            }
            break;
        }
        case TxState::COOLDOWN:
            if ((uint32_t)(nowMs - tClosed_) >= COOLDOWN_MS) {
                CanHealth h;
                readHealth(h);
                if (h.state == 2) latchFault(TxFault::BUS_OFF);
                else if (h.tec >= TEC_LATCH) latchFault(TxFault::TEC);
                else state_ = TxState::IDLE;
            }
            break;
        default:
            break;                                           // DISARMED, IDLE, FAULT: nothing to do
    }
}

uint8_t CanTxGate::rateFree(uint32_t nowMs) const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < RATE_MAX; i++)
        if (!rateUsed_[i] || (uint32_t)(nowMs - rateMs_[i]) >= RATE_WINDOW_MS) n++;
    return n;
}

bool CanTxGate::takeOutcome(TxOutcome& out) {
    if (!haveOutcome_) return false;
    out = outcome_;
    haveOutcome_ = false;
    return true;
}

void CanTxGate::forceClose() {
    if (state_ == TxState::NOT_BUILT || !port_) return;
    port_->transmitter(false);               // always, even when the gate considers the transmitter disabled
    if (state_ == TxState::OPENING || state_ == TxState::SENDING) {
        open_ = false;
        outcome_ = TxOutcome::FAILED;
        haveOutcome_ = true;
        stats_.failed++;
        // The pause start is counted from the window opening (forceClose gets no time): the pause will
        // not be shorter than COOLDOWN_MS minus the window time.
        tClosed_ = tOpen_;
        state_ = TxState::COOLDOWN;
    }
    open_ = false;
}

void CanTxGate::latchFault(TxFault reason) {
    if (state_ == TxState::NOT_BUILT || !port_) return;
    if (stats_.fault == TxFault::NONE) stats_.fault = reason == TxFault::NONE ? TxFault::OUTSIDE : reason;
    if (open_) {
        port_->transmitter(false);
        open_ = false;
        outcome_ = TxOutcome::FAILED;
        haveOutcome_ = true;
        stats_.failed++;
    }
    if (state_ != TxState::DISARMED) state_ = TxState::FAULT;   // a disarmed gate stays disarmed; the reason waits
}

void CanTxGate::clearFault() {
    if (state_ == TxState::NOT_BUILT) return;
    stats_.fault = TxFault::NONE;
    failedInRow_ = 0;
    if (state_ == TxState::FAULT) state_ = TxState::IDLE;
}

const char* txStateName(TxState s) {
    switch (s) {
        case TxState::NOT_BUILT: return "none";
        case TxState::DISARMED: return "disarmed";
        case TxState::IDLE: return "ready";
        case TxState::OPENING: return "opening";
        case TxState::SENDING: return "sending";
        case TxState::COOLDOWN: return "cooldown";
        case TxState::FAULT: return "fault";
    }
    return "?";
}

const char* txRefuseName(TxRefuse r) {
    switch (r) {
        case TxRefuse::OK: return "OK";
        case TxRefuse::NOT_BUILT: return "no CAN transmit in this build";
        case TxRefuse::DISARMED: return "transmit disarmed";
        case TxRefuse::FAULT: return "transmit locked after a fault";
        case TxRefuse::ID_NOT_ALLOWED: return "frame not on the list";
        case TxRefuse::BAD_LEN: return "wrong frame length";
        case TxRefuse::BUSY: return "previous send in progress";
        case TxRefuse::RATE: return "frame count limit";
        case TxRefuse::IGNITION: return "no ignition";
        case TxRefuse::BUS_QUIET: return "bus is quiet";
        case TxRefuse::FLASH_BUSY: return "flash write in progress";
        case TxRefuse::CONTROLLER: return "CAN controller not ready";
    }
    return "?";
}

const char* txFaultName(TxFault f) {
    switch (f) {
        case TxFault::NONE: return "none";
        case TxFault::THREE_FAILED: return "3 failed sends";
        case TxFault::TEC: return "transmit error counter";
        case TxFault::BUS_OFF: return "bus-off";
        case TxFault::WINDOW: return "transmitter window exceeded";
        case TxFault::CONTROLLER_EVENT: return "controller event";
        case TxFault::SIDE_EFFECT: return "foreign bits of 260 changed";
        case TxFault::OUTSIDE: return "external lock";
    }
    return "?";
}
