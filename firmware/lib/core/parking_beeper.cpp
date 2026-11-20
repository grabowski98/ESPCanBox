#include "parking_beeper.h"

void ParkingBeeper::onFrame(const vehicle::ParkingState& pa, uint32_t nowMs) {
    last_ = pa;
    haveFrame_ = true;
    lastFrameMs_ = nowMs;
}

bool ParkingBeeper::frameFresh(uint32_t nowMs) const {
    // subtraction on uint32_t works correctly also after the time counter overflows
    return haveFrame_ && (uint32_t)(nowMs - lastFrameMs_) <= cfg_.frameTimeoutMs;
}

BeeperOutput ParkingBeeper::update(uint32_t nowMs) {
    BeeperOutput out;
    if (!frameFresh(nowMs)) {
        wasActive_ = false;
        return out;   // no fresh frames = silence
    }

    // Nearest zone separately for front and rear; only the side that is active counts.
    uint8_t frontZone = last_.frontStatus == vehicle::PARK_ACTIVE ? last_.nearestFront() : 7;
    uint8_t rearZone = last_.rearStatus == vehicle::PARK_ACTIVE ? last_.nearestRear() : 7;
    bool carAsks = last_.soundRequest && last_.intervalRaw != vehicle::INTERVAL_NONE;

    // Continuous tone: only on the car's request.
    if (carAsks && last_.intervalRaw == vehicle::INTERVAL_CONTINUOUS) {
        out.active = true;
        out.continuous = true;
        out.toneOn = true;
        out.zone = 0;
        out.freqHz = last_.soundFront ? cfg_.freqFrontHz : cfg_.freqRearHz;
        wasActive_ = true;
        cycleStartMs_ = nowMs;   // after leaving the continuous tone we start with a beep
        return out;
    }

    uint8_t zone = frontZone < rearZone ? frontZone : rearZone;
    bool want = zone < 7 && (carAsks || cfg_.beepWithoutCarRequest);
    if (!want) {
        wasActive_ = false;
        return out;   // silence
    }
    out.active = true;
    out.zone = zone;
    // Tone pitch: when the car asks for sound it also says from which side; otherwise the nearer side.
    bool front = carAsks ? last_.soundFront : (frontZone < rearZone);
    out.freqHz = front ? cfg_.freqFrontHz : cfg_.freqRearHz;

    if (!wasActive_) {           // first beep immediately
        wasActive_ = true;
        cycleStartMs_ = nowMs;
    }
    uint32_t period = cfg_.periodMs[zone];          // zone < 7 checked above
    if (period < (uint32_t)cfg_.beepMs + 20) period = (uint32_t)cfg_.beepMs + 20;   // a pause always remains
    uint32_t t = (uint32_t)(nowMs - cycleStartMs_);
    if (t >= period) {           // new cycle; the tempo is taken from the freshest frame
        cycleStartMs_ = nowMs;
        t = 0;
    }
    out.toneOn = t < cfg_.beepMs;
    return out;
}
