// Transmit gate for the car's CAN bus.
//
// LOGIC ONLY, NO HARDWARE: the gate does not know the CAN driver. The hardware (the transceiver's SLNT pin,
// the send function) sits behind the CanTxPort interface, implemented in src/can_tx/ (compiled only in the
// environment decoder_tx_wifi, which hands it over through Decoder::attachCanTx). In every other environment
// Decoder calls begin(nullptr): state NOT_BUILT forever, every request ends in a refusal, nothing is called.
// In the tests the port is a mock (test_host/can_tx_gate.cpp, test_host/settings_change.cpp, test_host/tx_path.cpp).
//
// Fixed rules (in code, not in settings):
//   - list of allowed frames: the transmit allow list of the car profile (CarProfile::txAllow, const data),
//     given to begin(); identifier, length and a content validator per frame.
//     PSA AEE2010 (lib/car_psa_aee2010/tx_allowlist.cpp): 15B by default; 1A9 only in a build with
//     -DCAN_TX_EXPERIMENTAL=1. A profile without a list (generic) refuses every frame;
//   - one frame at a time, the next one no earlier than 500 ms after the transmitter is closed, at most 10 per minute;
//   - the transmitter (SLNT low) is open only for the duration of one frame, at most 20 ms;
//   - FAULT latch after transmitter events: until clearFault() or a restart.
// No Arduino framework header, no dynamic memory, no delay(): one machine step = one pass of the main loop.
#pragma once
#include <stdint.h>

#include "can_frame.h"
#include "car_profile.h"

enum class TxState : uint8_t {
    NOT_BUILT,   // program without transmission (port = nullptr)
    DISARMED,    // program can transmit but is not armed
    IDLE,        // armed, waiting for a request
    OPENING,     // transmitter enabled, the frame goes out after OPEN_SETTLE_MS (at least 1 ms of real time)
    SENDING,     // frame handed to the controller, waiting for the result
    COOLDOWN,    // transmitter disabled, 500 ms pause
    FAULT,       // latch: transmission blocked
};

enum class TxRefuse : uint8_t {
    OK,
    NOT_BUILT,
    DISARMED,
    FAULT,
    ID_NOT_ALLOWED,   // frame not on the list
    BAD_LEN,          // length other than the one in the allow list
    BUSY,             // a send or the pause after it is in progress (temporary refusal)
    RATE,             // 10 frames in the last minute
    IGNITION,         // no fresh body frame with ignition ON or ENGINE (PSA AEE2010: 0F6)
    BUS_QUIET,        // no traffic on the bus
    FLASH_BUSY,       // program update, flash write or a pending settings write
    CONTROLLER,       // temporary refusal: controller not running, REC != 0 or a recent bus error
};

enum class TxOutcome : uint8_t { NONE, ACKED, FAILED, TIMEOUT };

// Reason for the FAULT latch (TxStats::fault).
enum class TxFault : uint8_t {
    NONE,
    THREE_FAILED,       // 3 failed sends in a row
    TEC,                // transmit error counter >= 32
    BUS_OFF,            // the controller disconnected from the bus
    WINDOW,             // the transmitter was enabled for longer than 20 ms
    CONTROLLER_EVENT,   // the port reported a controller event (poll() = 4)
    SIDE_EFFECT,        // from outside: after a setting change, foreign bits of frame 260 changed
    OUTSIDE,            // from outside: another reason
};

// Admission conditions computed outside the gate (Decoder).
struct TxConditions {
    bool ignitionOk = false;   // body frame younger than 1 s, ignition ON or ENGINE (PSA AEE2010: 0F6 = 88 / 8E)
    bool busAlive = false;     // traffic on the bus in the last second
    bool flashBusy = false;    // update session or flash write
    bool nvsPending = false;   // a settings write to flash memory is pending
};

// CAN controller state reported by the port.
struct CanHealth {
    uint8_t state = 1;             // like twai_state_t: 0 stopped, 1 running, 2 bus-off, 3 recovering from bus-off
    uint16_t tec = 0, rec = 0;     // transmit and receive error counters
    uint32_t busErrors = 0;        // number of bus errors since startup
    uint32_t arbLost = 0, txFailed = 0;
    uint32_t msLastBusError = 0;   // time of the last bus error (valid when busErrors != 0)
};

// Transmit hardware. Implementation in src/can_tx/ (environment decoder_tx_wifi only); a mock in the tests.
struct CanTxPort {
    virtual void transmitter(bool on) = 0;      // true = transceiver transmitter active (SLNT low)
    virtual bool send(const CanFrame& f) = 0;   // one attempt (single shot), does not wait; false = not accepted
    // 0 in progress, 1 acknowledged (ACK), 2 failed, 3 arbitration lost, 4 controller event (latch)
    virtual uint8_t poll() = 0;
    virtual CanHealth health() = 0;
};

struct TxStats {
    uint32_t sent = 0;       // frames handed to the controller (every attempt, including a retry after arbitration)
    uint32_t acked = 0;      // acknowledged by another node
    uint32_t failed = 0;     // finished without acknowledgement (including window overrun)
    uint32_t refused = 0;    // requests rejected by request()
    TxFault fault = TxFault::NONE;   // latch reason (kept until clearFault)
    uint16_t tec = 0, rec = 0;       // from the last port read (request, end of pause)
};

class CanTxGate {
public:
    // Check of one frame against a transmit allow list (the car profile's, const). Order, unchanged from
    // the earlier fixed list: identifier not on the list -> ID_NOT_ALLOWED; length different
    // from the row (row len 0 = the validator decides) -> BAD_LEN; validator missing or false -> ID_NOT_ALLOWED.
    // Used by request() and, a second time and independently, by the hardware port (src/can_tx/).
    static TxRefuse checkFrame(const vehicle::CarProfile* car, const CanFrame& f);
    static bool allowedFrame(const vehicle::CarProfile* car, const CanFrame& f) {
        return checkFrame(car, f) == TxRefuse::OK;
    }
    static const uint32_t WINDOW_MS = 20;           // longest time the transmitter may be enabled
    static const uint32_t COOLDOWN_MS = 500;        // pause after the transmitter is closed
    // Time from enabling the transmitter (SLNT low) to handing the frame to the controller, in ms counter ticks.
    // The TJA1051 datasheet (NXP, Rev. 9) does not give the transition time from silent to normal mode: to be measured.
    // 2 ticks = at least 1 ms of real time, thousands of times more than the delays in Table 8.
    static const uint32_t OPEN_SETTLE_MS = 2;
    static const uint32_t RATE_WINDOW_MS = 60000;
    static const uint8_t RATE_MAX = 10;             // frames per minute
    static const uint8_t ARB_RETRIES = 2;           // retries after lost arbitration, within the same window
    static const uint8_t FAIL_LATCH = 3;            // failed sends in a row before the latch
    static const uint16_t TEC_LATCH = 32;
    static const uint32_t BUS_ERROR_QUIET_MS = 100; // this much quiet required after a bus error

    // port = nullptr: NOT_BUILT forever; otherwise DISARMED. car = the profile whose transmit allow list the
    // gate uses (nullptr or an empty list = every frame refused). Only the pointer to the const list is kept.
    void begin(CanTxPort* port, const vehicle::CarProfile* car);
    void setArmed(bool on);                         // DISARMED <-> IDLE; the latch stays
    // Request to send one frame. OK = accepted (transmitter open, result via takeOutcome()).
    TxRefuse request(const CanFrame& f, const TxConditions& c, uint32_t nowMs);
    void tick(const TxConditions& c, uint32_t nowMs);   // once per loop pass
    bool takeOutcome(TxOutcome& out);               // result of the last accepted send (one-shot)
    // The frame of the last accepted request was handed to the controller (port send() returned true at least
    // once). false when the send ended FAILED before that (flash busy, send() refused, forceClose while opening).
    bool handedOver() const { return handedOver_; }
    // How many more requests the per-minute limit would accept now (0..RATE_MAX). Read only: changes no
    // limit. Used to refuse a button press when its release frame would not fit in the limit.
    uint8_t rateFree(uint32_t nowMs) const;
    void forceClose();                              // before sleep, restart, update: transmitter disabled
    void latchFault(TxFault reason);                // also from outside (SettingsChanger)
    void clearFault();                              // FAULT -> IDLE
    TxState state() const { return state_; }
    const TxStats& stats() const { return stats_; }
    bool transmitterOn() const { return open_; }    // for tests: whether the gate holds the transmitter enabled

private:
    CanTxPort* port_ = nullptr;
    const vehicle::CarProfile* car_ = nullptr;   // allow list source (const, from begin)
    TxState state_ = TxState::NOT_BUILT;
    TxStats stats_;
    CanFrame frame_;
    bool open_ = false;            // transmitter enabled by the gate
    uint32_t tOpen_ = 0;           // when the transmitter was enabled
    uint32_t tClosed_ = 0;         // when it was disabled (start of the pause)
    uint8_t arbTries_ = 0;         // retries after arbitration in this window
    uint8_t failedInRow_ = 0;
    bool haveOutcome_ = false;
    bool handedOver_ = false;      // see handedOver(); reset by request()
    TxOutcome outcome_ = TxOutcome::NONE;
    uint32_t rateMs_[RATE_MAX] = {0};   // times of accepted requests within the last minute
    bool rateUsed_[RATE_MAX] = {false};

    void pruneRate(uint32_t nowMs);
    void close(uint32_t nowMs);
    void finish(TxOutcome o, bool countFail, uint32_t nowMs);
    void readHealth(CanHealth& h);
};

const char* txStateName(TxState s);     // "none", "disarmed", "ready", "opening", "sending", "cooldown", "fault"
const char* txRefuseName(TxRefuse r);   // short string for the console, ASCII only (no Polish diacritics)
const char* txFaultName(TxFault f);     // string for the console and JSON; NONE = "none"
