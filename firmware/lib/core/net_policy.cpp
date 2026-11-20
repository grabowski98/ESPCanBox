#include "net_policy.h"

// Rules described in net_policy.h. Order in update():
//   1. whether the car is "awake" (frames, ACC or a computer on USB); the transition to quiet (also the first
//      call without traffic and unplugging USB with a quiet bus) starts the standby for the client;
//   2. the standby lasts while the client keeps asking (60 s), 10 min since going quiet have not passed and the battery
//      is not too weak; after a start with a quiet bus the first 60 s count as a client request.
//      During an update (otaActive / uploadBusy) the standby is not evaluated. Once ended it does NOT come back until the car wakes up again: otherwise a late
//      request (second core) would turn Wi-Fi on right after the decision to turn it off;
//   3. a firmware update overrides the result (holdAwake always, Wi-Fi per enabled / uploadBusy);
//   4. pulse for a home-network connection attempt.
NetDecision NetPolicy::update(const NetInputs& in, uint32_t nowMs) {
    NetDecision d;
    const bool carAwake = in.busAlive || in.accOn || in.usbPlugged;

    if (!started_) {
        started_ = true;
        carWasAwake_ = true;    // a start without traffic counts as going quiet at this moment
        startGrace_ = !carAwake;
    }
    if (carAwake) {
        lingering_ = false;
        startGrace_ = false;
    } else if (carWasAwake_) {
        quietSinceMs_ = nowMs;
        lingering_ = true;
    }
    carWasAwake_ = carAwake;

    if (!carAwake && (in.otaActive || in.uploadBusy)) {
        // The update keeps Wi-Fi on anyway; the standby is evaluated only after it ends (the phone has time
        // to rejoin the network after the restart and query the panel before the program confirms itself).
        lingering_ = true;
    } else if (!carAwake && lingering_) {
        // Age of the last request: the timestamp from the second core may be newer than nowMs -> age 0.
        int32_t age = (int32_t)(nowMs - in.lastClientMs);
        if (age < 0) age = 0;
        // Start with a quiet bus: the client has not had a chance to ask yet (Wi-Fi is only coming up),
        // so for clientIdleMs from startup it counts as active.
        const bool startWindow = startGrace_ && (uint32_t)(nowMs - quietSinceMs_) < cfg_.clientIdleMs;
        const bool clientActive = (in.clientSeen && (uint32_t)age < cfg_.clientIdleMs) || startWindow;
        const bool withinCap = (uint32_t)(nowMs - quietSinceMs_) < cfg_.maxLingerMs;
        // Voltage: NaN (bad measurement) also ends the standby, because a comparison with NaN gives false.
        const bool batteryOk = !in.batteryMeasured || in.batteryV >= cfg_.minBatteryV;
        if (!in.enabled || !clientActive || !withinCap || !batteryOk) lingering_ = false;
    }

    d.wifiOn = in.enabled && (carAwake || lingering_);
    d.holdAwake = !carAwake && lingering_;

    if (in.otaActive || in.uploadBusy) {
        d.holdAwake = true;
        if (in.enabled) d.wifiOn = true;
    }
    // The upload itself: Wi-Fi stays on even after the "wifi off" command (the switch takes effect after the write ends).
    if (in.uploadBusy) d.wifiOn = true;

    // Home network: the first retry after a full interval from the moment the conditions started to be
    // met (the first connection is made by Wi-Fi start-up itself). Never with the engine running
    // and never during an upload (scanning for networks would break the file transfer).
    const bool staWanted = d.wifiOn && in.staConfigured && !in.staConnected && !in.engineRunning &&
                           !in.uploadBusy;
    if (!staWanted) {
        staTried_ = false;
    } else if (!staTried_) {
        staTried_ = true;
        lastStaTryMs_ = nowMs;
    } else {
        const uint32_t every = in.apStations > 0 ? cfg_.staRetryBusyMs : cfg_.staRetryMs;
        if ((uint32_t)(nowMs - lastStaTryMs_) >= every) {
            d.staAttempt = true;
            lastStaTryMs_ = nowMs;
        }
    }
    return d;
}
