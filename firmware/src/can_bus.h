// Frame reception from the car bus. This module has NO frame transmit function: the only
// place able to transmit a frame is src/can_tx/can_tx_port.cpp, compiled only in environment
// decoder_tx_wifi. Here is the controller mode selection at startup and the handling of its events.
#pragma once
#include <stdint.h>

#include "can_frame.h"

struct CanStats {
    uint32_t received = 0;    // frames received since startup
    uint32_t missed = 0;      // frames lost (full driver queue: the main loop could not keep up)
    uint32_t overrun = 0;     // frames lost in the controller (overflowed hardware queue: the interrupt
                              // did not run, e.g. during a write to flash memory); rx_overrun_count
    uint32_t busErrors = 0;   // bus errors seen by the controller
};

// CAN controller state from the last canHealthPoll() reading.
struct CanHealthInfo {
    bool running = true;      // controller in RUNNING state (until the first reading we assume true)
    uint8_t state = 1;        // twai_state_t: 0 stopped, 1 running, 2 bus-off, 3 recovering from bus-off;
                              // 255 = state reading failed
    uint16_t tec = 0;         // transmit error counter (stays at 0 in listen-only mode)
    uint16_t rec = 0;         // receive error counter
};

// false = the driver did not start.
// txArmed = false (always in the five environments without transmission, and in the sixth without a valid arming):
//   TWAI controller in LISTEN_ONLY mode and SLNT high. Two barriers, we are invisible to the car.
// txArmed = true (only the program with CAN_TX_MODE = CAN_TX_SETTINGS; elsewhere the parameter is ignored):
//   controller in NORMAL mode, SLNT still high (transceiver in silent mode). The transmitter is opened
//   only by can_tx_port.cpp for the duration of one frame. The mode is selected once, here; a change of arming
//   takes effect from the next startup (without reinstalling the driver during operation).
bool canBegin(bool txArmed = false);
bool canListenOnly();             // mode selected in canBegin(): true = LISTEN_ONLY
bool canReceive(CanFrame& out);   // does not wait; false = no frame
CanStats canStats();
// Controller supervision: call on every loop pass, it reads the state itself once per second
// (twai_get_status_info, does not wait). true = the state has just changed (to print a "[can]" line).
// When canBegin() failed, it reads nothing and returns false (the error was reported at startup).
bool canHealthPoll(uint32_t nowMs);
const CanHealthInfo& canHealth();
const char* canHealthStateName();     // "running", "stopped", "bus-off", "recovering from bus-off", "no reading"

// ---------- controller events in NORMAL mode (only the program with transmission, armed state) ----------
// The definitions exist only with CAN_TX_MODE = CAN_TX_SETTINGS; in the other environments nobody calls them.
struct CanAlertInfo {
    uint32_t busErrorAlerts = 0;   // how many times the event reading contained a bus error
    uint32_t arbLost = 0;          // lost arbitrations of our frames
    uint32_t txFailed = 0;         // our frames without acknowledgement
    uint32_t busOff = 0;           // entries into bus-off
    uint32_t recovered = 0;        // successful recoveries from bus-off (controller restarted)
    uint32_t msLastBusError = 0;   // time of the last bus error (valid when busErrorAlerts != 0)
};
// The only place that reads driver events (twai_read_alerts, does not wait). Call on every
// loop pass. Handles bus-off: SLNT high, twai_initiate_recovery, after recovery twai_start.
void canAlertsPoll(uint32_t nowMs);
// Transmitter events collected since the previous call (TWAI_ALERT_* bits): send result for can_tx_port.cpp.
uint32_t canTakeTxAlerts();
const CanAlertInfo& canAlertInfo();
