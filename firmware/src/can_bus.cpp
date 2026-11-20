#include "can_bus.h"

#include <driver/twai.h>

#include "config.h"

static uint32_t s_received = 0;
static bool s_started = false;                         // canBegin() succeeded
static bool s_listenOnly = true;                       // controller mode selected in canBegin()
static CanHealthInfo s_health;                         // last reading of the controller state
static uint32_t s_healthMs = 0;                        // when (millis)
static const uint32_t CAN_HEALTH_PERIOD_MS = 1000;     // state reading once per second

// TWAI bit timing for a bit rate (the speed comes from the car profile). Only the standard
// speeds of the ESP-IDF timing macros (hal/twai_types_deprecated.h of platform espressif32@8.0.0, versions
// for the ESP32-S3 clock source); each case copies the macro unchanged. 125 kbit/s = the same macro as in earlier versions.
// false = speed not in the list: the caller must not start the controller.
static bool canTimingFor(uint32_t bitrate, twai_timing_config_t& out) {
    switch (bitrate) {
        case 50000: { const twai_timing_config_t t = TWAI_TIMING_CONFIG_50KBITS(); out = t; return true; }
        case 100000: { const twai_timing_config_t t = TWAI_TIMING_CONFIG_100KBITS(); out = t; return true; }
        case 125000: { const twai_timing_config_t t = TWAI_TIMING_CONFIG_125KBITS(); out = t; return true; }
        case 250000: { const twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS(); out = t; return true; }
        case 500000: { const twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS(); out = t; return true; }
        case 1000000: { const twai_timing_config_t t = TWAI_TIMING_CONFIG_1MBITS(); out = t; return true; }
        default: return false;
    }
}

bool canBegin(bool txArmed) {
    // First the hardware transmitter lock: SLNT of the CAN Pal module high.
    // We do this before starting the controller, so that there is no moment with the transmitter enabled.
    // Order: FIRST the high level in the output register, THEN the pin as an output. The other way round
    // the pin would briefly drive a low level (register after reset = 0), i.e. transmitter enabled.
    // Until the switch to output, SLNT is held high by resistor R3 (10 kΩ to 3.3 V).
    // gpio_set_level, not digitalWrite: in this Arduino version (platform espressif32@8.0.0,
    // esp32-hal-gpio.c) digitalWrite before pinMode writes nothing (the pin is not yet
    // registered as GPIO). gpio_set_level sets the register itself, and pinMode does not change it.
    gpio_set_level(PIN_CAN_SILENT, 1);
    pinMode(PIN_CAN_SILENT, OUTPUT);
    digitalWrite(PIN_CAN_SILENT, HIGH);   // once more after pinMode, as before

    // TWAI_MODE_LISTEN_ONLY: the controller does not transmit frames, does not acknowledge (no ACK)
    // and does not send error frames. We are invisible to the car.
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(PIN_CAN_TX, PIN_CAN_RX, TWAI_MODE_LISTEN_ONLY);
    g.rx_queue_len = 128;     // approx. 218 frames/s on the bus; a margin of over half a second
    s_listenOnly = true;
#if CAN_TX_MODE == CAN_TX_SETTINGS
    if (txArmed) {
        // Armed state (design decision, variant a): controller in NORMAL mode, but the transceiver
        // still in silent mode (SLNT high), so neither ACK acknowledgements nor error frames reach
        // the bus. The transmitter is opened by can_tx_port.cpp only for the duration of one frame (allow list of the car profile).
        g.mode = TWAI_MODE_NORMAL;
        g.tx_queue_len = 0;   // no queue: the frame goes immediately or not at all (the send function returns an error)
        // Events needed by the gate and by bus-off handling. Without TWAI_ALERT_AND_LOG (writes nothing to USB).
        g.alerts_enabled = TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_ARB_LOST | TWAI_ALERT_BUS_ERROR |
                           TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED | TWAI_ALERT_TX_RETRIED |
                           TWAI_ALERT_PERIPH_RESET;
        s_listenOnly = false;
    }
#else
    (void)txArmed;            // program without transmission: always listen-only
#endif
    // Bit timing from the car profile (CarProfile::canBitrate, src/config.h). Unknown speed = safe state:
    // the driver is not installed, nothing starts (canBegin() = false, outputs stay off, red LED in main.cpp).
    // SLNT is already high above, so the transceiver transmitter stays disabled in that case too.
    twai_timing_config_t t;
    if (!canTimingFor(CAR_PROFILE_SELECTED.canBitrate, t)) return false;
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
    s_started = twai_start() == ESP_OK;
    return s_started;
}

bool canListenOnly() { return s_listenOnly; }

// Controller supervision. The only driver call is twai_get_status_info: it copies
// the counters and the state, waits for nothing (no time parameter). Once per CAN_HEALTH_PERIOD_MS.
// In listen-only mode the controller does not transmit, so a state other than "running" should not happen;
// the function exists so that such a case is visible (line "[can]", red LED in main.cpp).
// It repairs and changes nothing: the 12 V outputs still follow the ignition from the last fresh
// frames (PowerPolicy), and those go stale by themselves when the controller stops.
bool canHealthPoll(uint32_t nowMs) {
    if (!s_started) return false;
    if ((uint32_t)(nowMs - s_healthMs) < CAN_HEALTH_PERIOD_MS) return false;
    s_healthMs = nowMs;
    uint8_t state = 255;          // reading failed
    twai_status_info_t info;
    if (twai_get_status_info(&info) == ESP_OK) {
        state = (uint8_t)info.state;
        s_health.tec = (uint16_t)info.tx_error_counter;
        s_health.rec = (uint16_t)info.rx_error_counter;
    }
    bool changed = state != s_health.state;
    s_health.state = state;
    s_health.running = state == (uint8_t)TWAI_STATE_RUNNING;
    return changed;
}

const CanHealthInfo& canHealth() { return s_health; }

const char* canHealthStateName() {
    switch (s_health.state) {
        case (uint8_t)TWAI_STATE_STOPPED: return "stopped";
        case (uint8_t)TWAI_STATE_RUNNING: return "running";
        case (uint8_t)TWAI_STATE_BUS_OFF: return "bus-off";
        case (uint8_t)TWAI_STATE_RECOVERING: return "recovering from bus-off";
        default: return "no reading";
    }
}

bool canReceive(CanFrame& out) {
    twai_message_t m;
    while (twai_receive(&m, 0) == ESP_OK) {
        if (m.extd || m.rtr) continue;   // the car uses only standard 11-bit frames
        out.id = m.identifier;
        out.len = m.data_length_code > 8 ? 8 : m.data_length_code;
        for (uint8_t i = 0; i < out.len; i++) out.data[i] = m.data[i];
        s_received++;
        return true;
    }
    return false;
}

CanStats canStats() {
    CanStats s;
    s.received = s_received;
    twai_status_info_t info;
    if (twai_get_status_info(&info) == ESP_OK) {
        s.missed = info.rx_missed_count;
        // Overflow of the controller's hardware queue (FIFO in the chip): frames are lost before
        // the interrupt manages to move them to the driver queue (e.g. during a flash memory write).
        s.overrun = info.rx_overrun_count;
        s.busErrors = info.bus_error_count;
    }
    return s;
}

#if CAN_TX_MODE == CAN_TX_SETTINGS
// ---------- controller events in NORMAL mode (only the program with transmission) ----------
// There is no transmission here: only reading events and restoring reception after bus-off (bus-off also stops
// reception, and without reception the radio loses ACC). Transmission after bus-off stays blocked by the gate latch.
static CanAlertInfo s_alert;
static uint32_t s_txAlerts = 0;        // transmitter events waiting for can_tx_port.cpp

void canAlertsPoll(uint32_t nowMs) {
    if (!s_started || s_listenOnly) return;      // in listen-only the events are not enabled
    uint32_t a = 0;
    if (twai_read_alerts(&a, 0) != ESP_OK) return;   // time 0: does not wait; no events = ESP_ERR_TIMEOUT
    if (a & TWAI_ALERT_BUS_ERROR) {
        s_alert.busErrorAlerts++;
        s_alert.msLastBusError = nowMs;
    }
    if (a & TWAI_ALERT_ARB_LOST) s_alert.arbLost++;
    if (a & TWAI_ALERT_TX_FAILED) s_alert.txFailed++;
    if (a & TWAI_ALERT_BUS_OFF) {
        // The controller disconnected from the bus. First the transceiver transmitter is closed (SLNT high),
        // then recovery: the controller waits for 128 x 11 recessive bits and reports BUS_RECOVERED.
        gpio_set_level(PIN_CAN_SILENT, 1);
        s_alert.busOff++;
        twai_initiate_recovery();
    }
    if (a & TWAI_ALERT_BUS_RECOVERED) {
        // After recovery the driver is stopped: we start it again so that reception comes back.
        if (twai_start() == ESP_OK) s_alert.recovered++;
    }
    s_txAlerts |= a & (TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_ARB_LOST | TWAI_ALERT_BUS_OFF |
                       TWAI_ALERT_TX_RETRIED | TWAI_ALERT_PERIPH_RESET);
}

uint32_t canTakeTxAlerts() {
    uint32_t a = s_txAlerts;
    s_txAlerts = 0;
    return a;
}

const CanAlertInfo& canAlertInfo() { return s_alert; }
#endif  // CAN_TX_MODE == CAN_TX_SETTINGS
