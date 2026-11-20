#include "power_policy.h"

void PowerPolicy::onBodyState(const vehicle::BodyState& b, uint32_t nowMs) {
    last_ = b;
    haveFrame_ = true;
    lastFrameMs_ = nowMs;
}

void PowerPolicy::onLights(bool lightsOn, uint32_t nowMs) {
    lightsOn_ = lightsOn;
    haveLights_ = true;
    lastLightsMs_ = nowMs;
}

PowerOutputs PowerPolicy::update(uint32_t nowMs) {
    PowerOutputs out;
    // Freshness limit for the body and lights frames (PSA AEE2010: 0F6 and 036). In the tolerant mode (update over Wi-Fi) longer, because during
    // a flash write the CAN controller loses some frames; switching the ignition off with a frame with byte 1 = 80
    // works the same in both modes (it is the frame content, not its absence).
    uint32_t timeoutMs = lossTolerant_ ? cfg_.lossTolerantTimeoutMs : cfg_.frameTimeoutMs;
    bool fresh = haveFrame_ && (uint32_t)(nowMs - lastFrameMs_) <= timeoutMs;
    out.ignition = fresh && vehicle::ignitionOn(last_);

    if (out.ignition) {
        acc_ = true;                 // switch-on without delay
        offPending_ = false;
    } else if (acc_) {
        if (!offPending_) {
            offPending_ = true;
            offAtMs_ = nowMs + cfg_.accOffDelayMs;
        } else if ((int32_t)(nowMs - offAtMs_) >= 0) {
            acc_ = false;
            offPending_ = false;
        }
    }
    out.acc = acc_;
    // Reverse only with the ignition on and a fresh frame: after frames disappear
    // the camera must not stay switched on permanently.
    out.reverse = out.ignition && last_.reverseGear;
    bool lightsFresh = haveLights_ && (uint32_t)(nowMs - lastLightsMs_) <= timeoutMs;
    out.illumination = out.ignition && lightsFresh && lightsOn_;
    return out;
}
