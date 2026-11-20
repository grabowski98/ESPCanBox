// The ONLY place in the program that can transmit a frame on the car CAN bus (twai_transmit) and that
// sets the transceiver SLNT low (transmitter active). Design decisions below.
//
// WHETHER and WHAT may be transmitted is decided by the CanTxGate gate (lib/core/can_tx_gate.h): only frames
// from the transmit allow list of the car profile selected at build time (CarProfile::txAllow, const; PSA
// AEE2010: 15B, plus 1A9 only with -DCAN_TX_EXPERIMENTAL=1), one at a time, transmitter window at most
// 20 ms, latch after errors. This file executes its commands on the hardware and adds its own, independent safeguards:
//   - send() checks the frame once more against the same allow list (a second check, in case the gate had a bug);
//   - every frame goes with the "single shot" flag (one attempt, the controller does not retry by itself);
//   - a hardware timer (esp_timer, CAN_TX_HW_WINDOW_US = 25 ms) started BEFORE opening the transmitter
//     sets SLNT high independently of the main loop; without a working timer the transmitter will not open;
//   - the transmitter opens only when the controller works in NORMAL mode (armed state from canBegin).
//
// Transceiver TJA1051T/3 (NXP datasheet Rev. 9, 28.11.2017, https://www.nxp.com/docs/en/data-sheet/TJA1051.pdf):
//   pin S high = silent mode (sect. 7.1.2: transmitter disabled, receiver working), low = normal mode
//   (sect. 7.1.1). The transition time from silent to normal mode is not given in the datasheet (to be measured with an oscilloscope);
//   the gate waits 2 ticks of the ms counter (at least 1 ms) between opening and the frame. One 8-byte frame
//   at 125 kbit/s takes approx. 1 ms (up to 135 bits of 8 us each).
//
// Known limitations (not measured; the transmit path itself was tried in the prototype car on 18.11.2026):
//   - the esp_timer timer runs in a system task, not in an interrupt: during a flash memory write
//     it may be late. That is why the gate refuses during the whole update session and while a settings save is pending;
//   - while the transmitter is open, the controller in NORMAL mode acknowledges (ACK) other nodes' frames from those few ms.
#include "can_tx_port.h"

#include <Arduino.h>
#include <driver/twai.h>
#include <esp_timer.h>
#include <string.h>

#include "../can_bus.h"
#include "../config.h"

#if CAN_TX_MODE != CAN_TX_SETTINGS
#error "src/can_tx/ may be compiled only in an environment with CAN_TX_MODE = CAN_TX_SETTINGS (decoder_tx_wifi)"
#endif

static esp_timer_handle_t s_timer = nullptr;      // hardware timer of the transmitter window
static volatile bool s_hwTimeout = false;         // the timer fired (set by the esp_timer task)
static volatile uint32_t s_hwTimeouts = 0;
static bool s_open = false;                       // transmitter opened by this module
static bool s_arbLost = false;                    // arbitration was lost in the current attempt

// esp_timer task (not the main loop): transmitter closed after CAN_TX_HW_WINDOW_US, whatever the loop is doing.
// Only a GPIO register write and two variables; no logging and no dynamic memory.
static void onWindowTimeout(void*) {
    gpio_set_level(PIN_CAN_SILENT, 1);            // SLNT high: transceiver in silent mode
    s_hwTimeout = true;
    s_hwTimeouts = s_hwTimeouts + 1;
}

namespace {

class HwPort : public CanTxPort {
public:
    void transmitter(bool on) override {
        if (!on) {
            gpio_set_level(PIN_CAN_SILENT, 1);    // SLNT high ALWAYS first
            if (s_timer) esp_timer_stop(s_timer); // the "was not started" error does not matter
            s_open = false;
            return;
        }
        // Opening: only in NORMAL mode (armed startup) and only with a working hardware timer.
        s_open = false;
        if (!s_timer || canListenOnly()) return;
        s_hwTimeout = false;
        s_arbLost = false;
        canTakeTxAlerts();                        // old events must not pose as the result of a new frame
        if (esp_timer_start_once(s_timer, CAN_TX_HW_WINDOW_US) != ESP_OK) return;   // without the timer we do not open
        gpio_set_level(PIN_CAN_SILENT, 0);        // SLNT low: transceiver transmitter active
        s_open = true;
    }

    // One send attempt (single shot). Does not wait: wait time 0, there is no transmit queue.
    bool send(const CanFrame& f) override {
        if (!s_open || s_hwTimeout) return false;
        // Second, independent check against the allow list of the profile chosen at build time (config.h).
        if (!CanTxGate::allowedFrame(&CAR_PROFILE_SELECTED, f)) return false;
        twai_message_t m;
        memset(&m, 0, sizeof(m));                 // standard frame: 11 bits, no RTR
        m.ss = 1;                                 // single shot UNCONDITIONALLY: one attempt, no retries
        m.identifier = f.id;                      // after the check above: only an identifier from the allow list
        m.data_length_code = f.len;               // checked above: at most 8, the length of the allow list row
        memcpy(m.data, f.data, f.len);
        s_arbLost = false;
        canTakeTxAlerts();
        return twai_transmit(&m, 0) == ESP_OK;
    }

    // 0 in progress, 1 acknowledged (ACK), 2 failed, 3 arbitration lost, 4 controller event (latch).
    uint8_t poll() override {
        canAlertsPoll(millis());
        const uint32_t a = canTakeTxAlerts();
        if (s_hwTimeout) return 4;                // the window was closed by the hardware timer
        if (a & (TWAI_ALERT_BUS_OFF | TWAI_ALERT_TX_RETRIED | TWAI_ALERT_PERIPH_RESET)) return 4;
        if (a & TWAI_ALERT_TX_SUCCESS) return 1;
        if (a & TWAI_ALERT_ARB_LOST) s_arbLost = true;
        // With "single shot" a lost arbitration ends the attempt with a TX_FAILED event (the frame did not go,
        // but it is not a bus error): the gate may retry within the same window.
        if (a & TWAI_ALERT_TX_FAILED) return s_arbLost ? 3 : 2;
        return 0;
    }

    CanHealth health() override {
        CanHealth h;
        twai_status_info_t info;
        if (twai_get_status_info(&info) == ESP_OK) {
            h.state = (uint8_t)info.state;
            h.tec = (uint16_t)info.tx_error_counter;
            h.rec = (uint16_t)info.rx_error_counter;
        } else {
            h.state = 0;                          // a failed reading is treated as "not running" (temporary refusal)
        }
        const CanAlertInfo& ai = canAlertInfo();
        h.busErrors = ai.busErrorAlerts;
        h.arbLost = ai.arbLost;
        h.txFailed = ai.txFailed;
        h.msLastBusError = ai.msLastBusError;
        return h;
    }
};

HwPort s_port;

}  // namespace

bool canTxPortBegin() {
    gpio_set_level(PIN_CAN_SILENT, 1);            // just in case: transmitter closed
    if (s_timer) return true;
    esp_timer_create_args_t args;
    memset(&args, 0, sizeof(args));
    args.callback = &onWindowTimeout;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "can_tx_window";
    if (esp_timer_create(&args, &s_timer) != ESP_OK) {
        s_timer = nullptr;
        return false;
    }
    return true;
}

CanTxPort* canTxPort() { return &s_port; }

uint32_t canTxPortHwTimeouts() { return s_hwTimeouts; }
