// When Wi-Fi should be on, when the device may go to sleep and when to try connecting to the home network.
// Logic only, no hardware: the inputs are states and time, the outputs are three decisions.
//
// Background: the unit is on permanent +12 V. Wi-Fi draws about 100 mA, so it must not run
// while the car is parked. Rules:
//   - Wi-Fi runs when the car is awake (frames on the bus or the radio's ACC) or when a computer
//     is connected to USB (bench work);
//   - after the bus goes quiet the unit stays awake only for a panel client that
//     asked for something within the last minute, at most 10 minutes in total and only with a battery
//     not weaker than the threshold (if the voltage is measured);
//   - program start with a silent bus (after a restart, after an update, after a wake-up without
//     frames) counts as the bus going quiet at the moment of the first update(): a client with the correct
//     password keeps the standby under the normal rules (60 s since the last request, 10 min in total from
//     that moment, voltage threshold). With such a start the first 60 s count as a client request
//     (Wi-Fi has to come up first so that the client can ask); without a client Wi-Fi goes off after 60 s.
//     Unplugging USB with a silent bus: normal rules, without those 60 s;
//   - during an update (otaActive / uploadBusy) the standby for the client is neither evaluated nor
//     ended; evaluation resumes after it ends (e.g. after the new program is confirmed);
//   - a firmware update always keeps the device awake: an upload in progress, the time when the new
//     program waits for confirmation, and a scheduled restart (input otaActive). Wi-Fi is then
//     on, provided enabled. The user switch (enabled = false) turns Wi-Fi off in these states
//     (the device does not sleep anyway), BUT NOT during the upload itself (input uploadBusy):
//     interrupting the radio during the write achieves nothing, and stopping the server would wait for the end
//     of the upload; the switch takes effect after it ends;
//   - we try the home network (station mode) every 30 s, and when someone is connected to our
//     network, every 5 minutes: scanning for networks briefly breaks the connection with the phone. Never with
//     the engine running (the car is driving off, and scanning broadcasts the home network name);
//   - time: lastClientMs is sometimes written by another task LATER than the moment nowMs (two cores);
//     the age is computed as (int32_t)(nowMs - lastClientMs), and a negative result means age 0.
#pragma once
#include <stdint.h>

struct NetPolicyConfig {
    uint32_t clientIdleMs = 60000;        // client "active" = a request within this time
    uint32_t maxLingerMs = 600000;        // longest standby after the bus goes quiet
    float minBatteryV = 11.8f;            // below this voltage we do not stay awake for a client
    uint32_t staRetryMs = 30000;
    uint32_t staRetryBusyMs = 300000;     // retry interval when someone is connected to our network
};

struct NetInputs {
    bool enabled = true;          // user switch (command "wifi off")
    bool busAlive = false;        // a CAN frame in the last second
    bool accOn = false;           // the radio has ACC applied
    bool usbPlugged = false;      // computer on USB
    // A firmware upload is in progress, a new image waits for confirmation, or a restart is scheduled.
    // Effect: holdAwake = true always; wifiOn = true, provided enabled.
    bool otaActive = false;
    // The upload itself is in progress (part of otaActive). Effect: wifiOn = true even with enabled = false.
    bool uploadBusy = false;
    bool engineRunning = false;   // engine running: staAttempt always false
    bool clientSeen = false;      // at least one HTTP request with the correct password arrived since startup
    uint32_t lastClientMs = 0;    // time of the last such request; newer than nowMs = age 0
    bool batteryMeasured = false; // the voltage measurement is reliable (the board has a divider and 12 V is present)
    // Voltage on the same scale as NetPolicyConfig::minBatteryV. The unit passes here the rail voltage
    // after the input diode (this is what the board measures) and the matching threshold (NET_MIN_RAIL_V from config.h).
    float batteryV = 0.0f;
    uint8_t apStations = 0;       // devices connected to our network
    bool staConfigured = false;   // a home network is stored
    bool staConnected = false;
};

struct NetDecision {
    bool wifiOn = false;          // the Wi-Fi radio should run
    bool holdAwake = false;       // do not go to sleep despite a quiet bus
    bool staAttempt = false;      // try to connect to the home network now (a pulse for one call)
};

class NetPolicy {
public:
    explicit NetPolicy(const NetPolicyConfig& cfg = NetPolicyConfig()) : cfg_(cfg) {}
    NetDecision update(const NetInputs& in, uint32_t nowMs);

private:
    NetPolicyConfig cfg_;
    bool started_ = false;            // the first update() has already happened (rule for start with a quiet bus)
    bool carWasAwake_ = false;
    bool lingering_ = false;
    bool startGrace_ = false;         // standby running since a start with a quiet bus (60 s without a client)
    uint32_t quietSinceMs_ = 0;       // when the car went quiet
    bool staTried_ = false;
    uint32_t lastStaTryMs_ = 0;
};
