// Changing a vehicle setting at the radio's request: command 80 nn vv -> frame 15B -> check in frame 260.
//
// LOGIC ONLY, NO HARDWARE. Frame 15B is computed by vset::build15B (vehicle_settings.h); whether it is sent
// is decided by the CanTxGate gate (can_tx_gate.h). In a build without transmit support the gate is in the
// NOT_BUILT state (no transmit hardware), so every command ends immediately with a REFUSED report and
// nothing is sent. The full sequence is checked by a host test with a mock port: test_host/settings_change.cpp.
//
// Sequence of one command:
//   1. build15B from the CURRENT frame 260 (refusal -> REFUSED with a reason);
//   2. build without transmit -> REFUSED (txRefuse = NOT_BUILT);
//   3. frame 260 already shows the requested state -> ALREADY, we transmit nothing;
//   4. gate: a temporary refusal (controller, pause after the previous frame) is retried on every loop pass,
//      any other refusal -> REFUSED;
//   5. gate result: no acknowledgement -> TX_FAILED;
//   6. waiting for frame 260 (1300 ms from the acknowledgement, more than 2 periods of 260): ACCEPTED or NOT_ACCEPTED;
//      if other bits of 260 changed together with the requested field -> SIDE_EFFECT and the gate latches.
// No repetitions of frame 15B. A command that arrived during the previous one waits in ONE slot
// (the last one wins) and is computed afresh on frame 260 after the previous one finishes. When the slot
// is taken by a command for ANOTHER item, that older command is dropped with a REFUSED report (txRefuse =
// BUSY), so the console shows it; a newer command for the same item replaces it without a report.
//
// Button items (frame 1A9) with -DCAN_TX_1A9_RELEASE=1: RELEASE_DELAY_MS after the press was acknowledged on
// the bus, one release frame (1A9 20 7F FF 00 FF 00 50 00) goes through the same gate (all its conditions apply).
// While the gate has its pause after the press (500 ms) the release is retried on every pass, at most
// RELEASE_GIVEUP_MS from its due time, then given up (takeReleaseEvent: "not sent"). A press without an
// acknowledgement gets no release. The press report (ACCEPTED / NOT ACCEPTED) does not wait for the release;
// a next command waits until the release is finished.
#pragma once
#include <stdint.h>

#include "can_tx_gate.h"
#include "car_state.h"
#include "vehicle_settings.h"

namespace vset {

enum class ChangeResult : uint8_t {
    NONE,
    ALREADY,        // 260 already shows the requested state: nothing sent
    ACCEPTED,       // 15B acknowledged and 260 shows the requested state
    NOT_ACCEPTED,   // 15B acknowledged, but 260 did not change in time
    TX_FAILED,      // 15B was not acknowledged on the bus
    REFUSED,        // nothing sent: reason in 'refuse' (15B logic) or 'txRefuse' (gate)
    SIDE_EFFECT,    // other bits of 260 changed together with the requested field: gate latched
};

struct ChangeReport {
    uint8_t id = 0, value = 0;
    ChangeResult result = ChangeResult::NONE;
    Refuse refuse = Refuse::OK;        // reason from build15B (REFUSED)
    TxRefuse txRefuse = TxRefuse::OK;  // reason from the gate (REFUSED); NOT_BUILT = build without transmit
    uint16_t canId = 0x15B;            // frame ID: 15B or 1A9 (button items)
    bool haveFrame = false;            // 'frame' holds the frame handed to the gate
    uint8_t frame[8] = {0};
    uint8_t otherBits[5] = {0};        // bits of 260 (bytes 2..6) changed outside the command field
    uint16_t tookMs = 0;               // from the radio command to the result
    uint32_t ms = 0;                   // report time
};

// Result of sending one 15B frame (for the console line "[vset] 15B ... sent"), before the report arrives.
struct TxEvent {
    uint16_t canId = 0x15B;
    uint8_t frame[8] = {0};
    bool acked = false;      // acknowledged by another node
    bool handedOver = false; // the frame reached the CAN controller (CanTxGate::handedOver); false = never on the bus
    uint16_t ms = 0;         // from acceptance by the gate to the result
};

// Result of the release frame after a button press (CAN_TX_1A9_RELEASE=1 only), for the console line
// "[vset] 1A9 ... release sent / release not sent (...)".
struct ReleaseEvent {
    uint8_t frame[8] = {0};
    bool sent = false;              // accepted by the gate (then 'acked' tells the result)
    bool acked = false;
    bool handedOver = false;        // the frame reached the CAN controller (CanTxGate::handedOver)
    TxRefuse refuse = TxRefuse::OK; // not sent: reason from the gate (BUSY = still busy after RELEASE_GIVEUP_MS)
    uint16_t ms = 0;                // sent: from acceptance by the gate to the result
    uint16_t afterPressMs = 0;      // from the acknowledgement of the press to the result
};

class SettingsChanger {
public:
    static const uint32_t ACCEPT_TIMEOUT_MS = 1300;   // waiting for 260 after 15B is acknowledged
    static const uint32_t BUTTON_TIMEOUT_MS = 2500;   // waiting for the state frame (227 / 3E1) after 1A9 is acknowledged
    static const uint32_t RETRY_CONTROLLER_MS = 300;  // retrying on a temporary controller refusal
    static const uint32_t RETRY_BUSY_MS = 600;        // retrying while the gate has its pause after a frame (500 ms)
    static const uint32_t TX_WAIT_MS = 200;           // safeguard: the gate gave no result
    // Release after a button press (CAN_TX_1A9_RELEASE=1). Measured (can_20261110_190421.log): the box sent no
    // separate release; it sent 1A9 periodically (every ~204 ms, 20 7F FF 00 FF 00 50 00 with ignition on), a press
    // replaced one periodic frame (e.g. 1207.859) and the next periodic frame followed ~204 ms later (1208.066).
    // Our release imitates that next frame. Whether a press without it causes a later toggle is a hypothesis.
    // Our gate pause is 500 ms, so in practice the release goes out when the pause ends, about 500 ms after the press.
    static const uint32_t RELEASE_DELAY_MS = 200;
    static const uint32_t RELEASE_GIVEUP_MS = 1000;   // retrying a busy gate, counted from the due time

    void begin(CanTxGate* gate, uint32_t enabledMask);
    void setEnabledMask(uint32_t mask) { mask_ = mask; }
    uint32_t enabledMask() const { return mask_; }
    // Radio command 80 id value. Collect the report (if one was produced immediately) with takeReport().
    void onRadioSetting(uint8_t id, uint8_t value, const CarState& car, const TxConditions& c, uint32_t nowMs);
    // Once per loop pass, BEFORE CanTxGate::tick. At most one report per call.
    void tick(const CarState& car, const TxConditions& c, uint32_t nowMs);
    // One report slot: collect after every onRadioSetting and tick.
    bool takeReport(ChangeReport& out);
    // Frame send result (one-shot). Collect after every tick, before takeReport.
    bool takeTxEvent(TxEvent& out);
    // Release frame result (one-shot, CAN_TX_1A9_RELEASE=1 only). Collect after every tick.
    bool takeReleaseEvent(ReleaseEvent& out);
    bool busy() const { return state_ != State::IDLE || havePending_ || rel_ != Rel::NONE; }

private:
    enum class State : uint8_t { IDLE, WAIT_GATE, WAIT_TX, WAIT_260 };
    enum class Rel : uint8_t { NONE, DUE, WAIT_TX };   // release frame: waiting for its time / the gate, sent

    CanTxGate* gate_ = nullptr;
    uint32_t mask_ = ENABLED_DEFAULT;
    State state_ = State::IDLE;
    uint8_t id_ = 0, value_ = 0;
    bool havePending_ = false;         // command waiting for the previous one to finish
    uint8_t pendId_ = 0, pendValue_ = 0;
    uint32_t tCmd_ = 0;                // when the command arrived
    uint32_t tRetry_ = 0;              // start of retrying the request to the gate
    uint32_t tReq_ = 0;                // when the gate accepted the frame
    uint32_t tAck_ = 0;                // when the frame was acknowledged
    uint8_t frame_[8] = {0};           // frame handed to the gate
    bool button_ = false;              // current command is a button item (frame 1A9)
    uint8_t ref260_[5] = {0};          // bytes 2..6 of the frame 260 that 15B was built from
    uint32_t ref260Ms_ = 0;            // time of that frame 260 (to recognise a new one)
    bool haveNew260_ = false;          // a new full-length 260 arrived after the request
    uint8_t last260_[5] = {0};         // its bytes 2..6
    bool haveReport_ = false;
    bool haveTxEvent_ = false;
    TxEvent txEvent_;
    ChangeReport report_;
    Rel rel_ = Rel::NONE;
    uint32_t tPressAck_ = 0;           // when the press was acknowledged (release due RELEASE_DELAY_MS later)
    uint32_t tRelReq_ = 0;             // when the gate accepted the release
    bool haveRelEvent_ = false;
    ReleaseEvent relEvent_;

    void tickRelease(const TxConditions& c, uint32_t nowMs);
    void endRelease(bool sent, bool acked, TxRefuse refuse, uint32_t nowMs);
    void dropPending(uint32_t nowMs);
    void start(uint8_t id, uint8_t value, const CarState& car, const TxConditions& c, uint32_t nowMs);
    void attempt(const CarState& car, const TxConditions& c, uint32_t nowMs);
    void attemptButton(const CarState& car, const TxConditions& c, uint32_t nowMs);
    void finish(ChangeResult result, Refuse refuse, TxRefuse txRefuse, bool haveFrame, uint32_t nowMs);
    bool otherBitsChanged(uint8_t out[5]) const;
    void noteTx(bool acked, uint32_t nowMs);
};

const char* changeResultName(ChangeResult r);   // console text, no Polish diacritics

}  // namespace vset
