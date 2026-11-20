// Decoder: all of the unit's logic glued into one object.
// No hardware and no Arduino framework header: the inputs are CAN frames, frames from the radio and the time in milliseconds,
// the outputs are output states (ACC, ILL, REV, sound), ready bytes for the radio and console lines.
//
// This is the code moved from src/main.cpp of version 0.6.0 (handleFrame from keys.onCanFrame and the middle
// of loop()) and the transmission from src/radio_uart.cpp, without changing the call order or the line texts.
// Compatibility is guarded by the test test_host/golden_stream.cpp --decoder (references from ref_glue_060.h).
// Frames 38 (setting states from CAN) and 3C were added: lib/car_psa_aee2010/vehicle_settings.h.
// Radio commands 80 / 8A / 84 produce a console line with a name (radio_commands.h); none
// is executed, because the program cannot transmit on CAN.
// Command 80 goes through SettingsChanger and the CanTxGate gate (settings_changer.h,
// can_tx_gate.h). The gate has no hardware (port = nullptr, state NOT_BUILT), so the result is the same
// as before: a refusal and the same console line.
//
// The program with transmission (environment decoder_tx_wifi) gives the gate the hardware via
// attachCanTx(); then command 80 can end with a 15B frame. In the other environments nobody
// calls attachCanTx and everything stays as above.
// The gate takes its frame list from the car profile
// (CarProfile::txAllow); the button frame 1A9 is on it only with -DCAN_TX_EXPERIMENTAL=1.
//
// The car and the radio come from two const structures,
// DecoderConfig::car (CarProfile, lib/core/car_profile.h) and DecoderConfig::radio (RadioProtocol,
// lib/core/radio_protocol.h). The defaults are PSA AEE2010 + XP: every existing caller gets the same behaviour.
// What follows the profile: buttons (key map + key codes), parking sensors, ignition / reverse, lights and
// the car data for the radio screen (steering angle, trip computer, outside temperature, economy mode,
// climate control, doors, vehicle settings): the profile decodes CAN into the car-neutral types of
// vehicle_signals.h (CarProfile::decode...), the radio protocol encodes them (RadioProtocol::encode...),
// and onCanFrameCarData() only connects the two, without any car or radio frame number.
// Still PSA AEE2010 code, run only with the PSA AEE2010 frame set (psaCar_): the car state for the
// settings change and the transmit conditions (CarModel, frames 260 / 361 / 0F6, SettingsChanger), i.e.
// everything on the transmit side.
// The radio side goes only through the RadioProtocol: the common session (RadioSession, lib/core/radio_session.h:
// queue, state slots, timing) gets power, keys, parking, obstacle and version frames from the protocol's
// encoders, nextRadioTx frames with the protocol's encodeFrame, and the commands from the radio come from
// its decodeCommand. What the commands mean (vehicle setting numbers, climate buttons) still follows the
// reference pair: the setting numbers are those of the XP radio (vset table). Climate and sound commands
// only produce a console line: nothing is sent to the car for them.
// With radio "none" (or a protocol without encodeFrame) nothing goes to the radio (nextRadioTx always 0,
// the queue is emptied).
//
// Call order in one pass of the main loop (src/main.cpp):
//   onCanFrame (every received frame) -> tick -> onRadioFrame (frames from the radio) -> nextRadioTx.
// Decoder itself does not touch the hardware: the only path to CAN is the CanTxPort interface given to the gate.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "can_frame.h"
#include "can_tx_gate.h"
#include "car_profile.h"
#include "car_state.h"
#include "park_diag.h"
#include "parking_beeper.h"
#include "power_policy.h"
#include "profile_generic.h"
#include "profile_psa_aee2010.h"
#include "protocol_xp.h"
#include "psa_frames.h"
#include "radio_commands.h"
#include "radio_link.h"
#include "radio_protocol.h"
#include "radio_session.h"
#include "settings_changer.h"
#include "vehicle_settings.h"
#include "xp_car.h"
#include "xp_keys.h"
#include "xp_keys_custom.h"
#include "xp_protocol.h"
#include "xp_settings.h"

struct DecoderConfig {
    bool decoderMode = false;   // true = MODE_DECODER mode (config.h); for now only stored
    // true (always in the device): frame 38 DATA[1..5] and frame 3C computed from CAN, pair 38 + 3C after
    // radio command 81 01. false only in the golden_stream --without 38,3C test: behaviour of
    // version 0.7.0 (constant 02 E1 A9 98 40, no 3C, no repeat), to show that apart from frames
    // 38 and 3C the stream to the radio did not change.
    bool settingsFromCan = true;
    // true (always in the device): radio commands 80 / 8A / 84 produce the console line "[vset]", "[climate]",
    // "[sound]". false only in the golden_stream --without 38,3C test (the line did not exist in 0.7.0).
    bool logRadioCommands = true;
    // Car profile and radio protocol. Defaults: the reference pair, PSA AEE2010 + XP.
    // src/main.cpp sets them from -DCAR_PROFILE / -DRADIO_PROTOCOL (src/config.h).
    // car = nullptr: the generic profile (safer than guessing a car); radio = nullptr: XP.
    const vehicle::CarProfile* car = &kCarPsaAee2010;
    const RadioProtocol* radio = &kRadioXp;
};

// What main.cpp has to do with the hardware after tick().
struct DecoderOutputs {
    PowerOutputs power;          // ACC, illumination, reverse (12 V outputs to the radio) and ignition
    BeeperOutput beep;           // parking sensor sound
    bool busAlive = false;       // a frame from the bus in the last second
    bool carBusy = false;        // reverse or active sensors: the car needs the unit
    bool engineRunning = false;  // fresh 0F6 frame with byte 1 = 8E
    bool accForNet = false;      // ACC with real bus traffic in the last 5 s
    bool sensorsOn = false;      // fresh 0E1 frame with active sensors (wakes the amplifier)
};

// Last radio command 80 (vehicle setting change) and result counters: data for the web panel
// (field car.settings in /api/status). Filled from the SettingsChanger report.
// The program cannot transmit on CAN, so every command ends in a refusal: only the counter
// 'refused' grows, and results from ALREADY down do not occur (they are for the program with transmission).
enum class SettingCmdResult : uint8_t {
    REFUSED,       // item with a field in frame 15B (groups A-D): "REFUSED (no CAN transmit in this build)"
    NO_FRAME,      // no known frame (group E and numbers outside the table)
    BLOCKED,       // frame known (1A9), transmission blocked until measured (80 05, 80 0C)
    ALREADY,       // the car already has the requested state, nothing sent
    ACCEPTED,      // frame 15B acknowledged, frame 260 shows the requested state
    NOT_ACCEPTED,  // frame 15B acknowledged, frame 260 unchanged
    TX_FAILED,     // frame 15B not acknowledged on the bus
    SIDE_EFFECT,   // other bits of 260 changed as well: transmission blocked
};
struct SettingCmdStats {
    bool haveLast = false;       // there was a command 80 since startup
    uint8_t lastId = 0;          // item number (second byte of the command)
    uint8_t lastValue = 0;       // requested value
    SettingCmdResult lastResult = SettingCmdResult::REFUSED;
    uint32_t lastMs = 0;         // when the result was produced (without transmission: the same moment as the command)
    uint16_t lastTookMs = 0;     // time from command to result (0 without transmission)
    bool lastHaveFrame = false;  // lastFrame contains the 15B frame handed to the gate (never without transmission)
    uint8_t lastFrame[8] = {0};
    uint32_t accepted = 0;       // changes accepted by the car (always 0: the program does not transmit)
    uint32_t notAccepted = 0;    // sent, but the car did not change state (always 0)
    uint32_t refused = 0;        // commands ended with a refusal (all three kinds of result)
};
// Result string for JSON (ASCII only): "refused", "no_frame", "blocked"; for the program
// with transmission also "unchanged", "accepted", "not_accepted", "tx_failed", "side_effect".
const char* settingCmdResultName(SettingCmdResult r);

class Decoder {
public:
    // Ready console line ("[park] ...", "[car] ...", "[key] ..."), terminated with '\n',
    // exactly as conPrintf printed it in main.cpp 0.6.0. Called from onCanFrame and tick.
    typedef void (*LogFn)(const char* line);

    void begin(const DecoderConfig& cfg, LogFn log);
    // CAN transmit hardware (only the program with transmission, after begin()). port = nullptr changes nothing.
    // armed = false: gate disarmed (every request = refusal, the transmitter is never enabled).
    void attachCanTx(CanTxPort* port, bool armed);
    void setCanTxArmed(bool on);        // "cantx off" works immediately; "on" only after a restart (main.cpp)
    void canTxForceClose();             // before sleep, restart: transmitter disabled
    void canTxClearFault();             // "cantx reset"
    // Additional gate blocks: a program update session is running, a settings write to flash memory is pending.
    void setTxBlock(bool otaBusy, bool nvsPending) {
        otaBusy_ = otaBusy;
        nvsPending_ = nvsPending;
    }
    // Overnight record of the parking sensors state (lib/car_psa_aee2010/park_diag.h) in RTC memory (src/main.cpp).
    // Without the call: an own record in RAM (host tests). wokeFromSleep = this start is a wake-up from deep sleep.
    void attachParkDiag(psa::ParkDiagRecord* rec, bool wokeFromSleep) { parkDiag_.begin(rec, wokeFromSleep); }
    // Console command "set NN VV": the same path as radio command 80 NN VV (the same refusals).
    void requestSetting(uint8_t id, uint8_t value, uint32_t nowMs);

    // One CAN frame. fromBus = from the bus or pasted over USB; false = pasted from the web panel
    // (does not count as bus traffic).
    void onCanFrame(const CanFrame& f, uint32_t nowMs, bool fromBus);
    // One valid frame from the radio (after the protocol's parser, RadioProtocol::rxParser).
    void onRadioFrame(const RadioFrame& f, uint32_t nowMs);
    // Once per loop pass: buttons, radio power, radio session (RadioSession), parking sensor sound.
    // flashBusy = a flash memory write is in progress (longer freshness limit for frame 0F6).
    const DecoderOutputs& tick(uint32_t nowMs, bool flashBusy);
    // Next frame for the radio as ready bytes (RadioProtocol::encodeFrame; XP: xp::encode). 0 = nothing to send.
    // Includes: the protocol's gap between frames (XP: 10 ms), emptying the queue when txEnabled = false
    // (counter radioTxSuppressed) and the rule "a failed encode does not advance the time".
    size_t nextRadioTx(uint32_t nowMs, bool txEnabled, uint8_t* buf, size_t cap);

    // --- reads for the console and the panel ---
    // Car state collected from CAN frames (car_state.h). The methods below (ignitionRaw, tempRaw ...) read from it.
    const CarState& car() const { return car_.state(); }
    const DecoderConfig& config() const { return cfg_; }
    const DecoderOutputs& outputs() const { return out_; }       // result of the last tick()
    bool carBusy(uint32_t nowMs) const;                          // reverse or active sensors
    bool busAlive(uint32_t nowMs) const;                         // a frame from the bus in the last second
    const PowerOutputs& power() const { return powerOut_; }
    uint8_t ignitionRaw() const;                                 // PSA: byte 1 of the last 0F6; other profiles: decodeBody
    bool ignitionFresh(uint32_t nowMs) const;                    // ignition frame of the profile younger than 1 s
    bool frame0F6Fresh(uint32_t nowMs) const;                    // 0F6 frame younger than 1 s (PSA AEE2010 only)
    bool haveTempByte() const { return car_.state().f0F6.len >= 7; }   // the last 0F6 had byte 7
    uint8_t tempRaw() const { return car_.state().f0F6.d[6]; }   // byte 7 of frame 0F6: value / 2 - 40 = deg C
    bool doorsKnown() const { return doorState_.valid; }         // there was a door frame (decoded by the profile)
    bool haveLastKey() const { return haveLastKey_; }
    const xp::KeyEvent& lastKey() const { return lastKey_; }
    uint32_t lastKeyMs() const { return lastKeyMs_; }
    uint32_t radioTxSent() const { return framesSent_; }             // frames handed over for sending to the radio
    uint32_t radioTxSuppressed() const { return framesSuppressed_; } // frames dropped (transmission disabled)
    // Frames the protocol's encodeFrame could not write into the transmit buffer (returned 0): dropped.
    uint32_t radioTxTooLong() const { return framesTooLong_; }
    const RadioSession& link() const { return radio_; }
    const xp::KeyEncoder& keys() const { return keys_; }
    const ParkingBeeper& beeper() const { return beeper_; }
    // Car data for the radio screen, as decoded by the car profile (car-neutral types, vehicle_signals.h).
    const vehicle::DoorState& doorState() const { return doorState_; }
    const vehicle::ClimateState& climate() const { return climate_; }
    const vehicle::VehicleSettings& vehicleSettings() const { return settings_; }
    bool doorSwap() const { return doorSwap_; }                  // side swap for the radio (console "doorswap")
    // Data bytes 1..5 of the last car state frame built for the radio (XP frame 38 DATA[1..5]: setting states).
    // All zeros until the radio protocol has built one (radio "none": always zeros).
    const uint8_t* settings38() const { return carStateFrame_.data + 1; }
    const SettingCmdStats& settingCmds() const { return setCmd_; }   // last command 80 and counters
    // CAN transmit gate (read only). Without attachCanTx always TxState::NOT_BUILT and all zeros.
    TxState canTxState() const { return gate_.state(); }
    // Line "[cantx] state=... sent=N ... tec=N rec=N fault=..." for the cantx command, with '\n'.
    void formatCanTxStatus(char* buf, size_t n) const;
    // Line "[vset] 260 at startup=... changed bits (bytes 2-6)=..." for the vset command, with '\n'.
    void formatSettingsDiff(char* buf, size_t n) const;
    const TxStats& canTxStats() const { return gate_.stats(); }
    // Line "[stat] can-tx=none|disarmed|ready|FAIL sent=N ..." for the stat command, with '\n'.
    void formatCanTx(char* buf, size_t n) const;
    // Line "[diag] wakes=N park-flips-asleep=N 1A9 press=N release=N" for the stat command, with '\n'.
    void formatParkDiag(char* buf, size_t n) const;
    // Mask of enabled settings items (bit = item number), default vset::ENABLED_DEFAULT.
    // Only the program with transmission changes it (command "vset on|off NN", stored in NVS).
    void setSettingsMask(uint32_t mask) { changer_.setEnabledMask(mask); }
    uint32_t settingsMask() const { return changer_.enabledMask(); }
    xp::KeyCustomizer& keyMap() { return keyCustom_; }
    // Line "[park] ..." with the state from the last 0E1 frame (for the stat command), with '\n'.
    void formatParking(char* buf, size_t n) const;
    // Command "vset": a header line (last frames 260 and 361, age of 260, mask) and one line
    // per settings table item (number, name, state from 260, base, mask, result of the pure 15B logic
    // for the current state). Lines with '\n'. Read only: transmits nothing.
    uint8_t settingsCount() const { return vset::count(); }
    void formatSettingsHeader(uint32_t nowMs, char* buf, size_t n) const;
    void formatSettingRow(uint8_t i, uint32_t nowMs, char* buf, size_t n) const;

    // --- console commands ---
    void injectKey(uint8_t code, uint8_t state);     // button event straight to the radio (key, kd, ku)
    void toggleDoorSwap(uint32_t nowMs);             // swap door sides + door frame to the radio
    void setKeyLog(bool on) { logKeys_ = on; }       // printing of "[key] ..." lines
    bool keyLog() const { return logKeys_; }

private:
    static const uint32_t FRAME_0F6_FRESH_MS = 1000;       // 0F6 arrives every 500 ms

    DecoderConfig cfg_;
    LogFn log_ = nullptr;
    // From begin(): cfg_.car uses the PSA AEE2010 frame set (decodeBody = psa::decodeBodyState; CarModel, the
    // settings change and the transmit conditions run), and the radio
    // has a UART (frames are handed to the port). The defaults match the default DecoderConfig.
    bool psaCar_ = true;
    bool radioOn_ = true;
    // Last ignition / reverse state decoded through the profile (CarProfile::decodeBody), for the
    // "[car] reverse" line. For PSA AEE2010 the same value as CarModel's body (both take every decoded 0F6).
    vehicle::BodyState body_;
    bool haveBody_ = false;          // decodeBody accepted a frame (any profile)
    uint32_t bodyMs_ = 0;            // time of that frame

    ParkingBeeper beeper_;
    PowerPolicy power_;
    xp::KeyEncoder keys_;
    RadioSession radio_;               // protocol set in begin() (cfg_.radio)
    xp::KeyCustomizer keyCustom_;      // own deviations from the box: moon, swap table (defaults from the car profile)

    // Car data for the radio screen: state kept for the profile's decode functions (CarProfile::decode...),
    // read by the radio protocol's encoders (RadioProtocol::encode...). All defaults ("not valid") at startup.
    vehicle::SteeringAngle steering_;
    vehicle::TripData trip_;
    vehicle::OutsideTemp outsideTemp_;
    vehicle::EcoMode eco_;
    vehicle::ClimateState climate_;
    vehicle::DoorState doorState_;
    vehicle::VehicleSettings settings_;
    // The car state frame for the radio (doors + setting values) is built incrementally: the encoder gets
    // its previous content (RadioProtocol::encodeCarState). The side swap is a radio option, on by default.
    RadioFrame carStateFrame_;
    bool haveCarStateFrame_ = false;   // the encoder built carStateFrame_ at least once (its slot exists)
    uint8_t presentCmd_ = 0;           // number of the "settings present" frame the protocol built last
    bool havePresent_ = false;
    bool doorSwap_ = true;

    // Car state: bus traffic, frame 0F6 (ignition, reverse, temperature), doors and the raw
    // frames 036, 260, 361, 227, 3E1, 217. Receives every frame ALONGSIDE the decode functions above.
    CarModel car_;
    SettingCmdStats setCmd_;           // last radio command 80 and counters (for the panel)
    // CAN transmission: logic only. gate_.begin(nullptr) = no transmit hardware (NOT_BUILT).
    CanTxGate gate_;
    vset::SettingsChanger changer_;    // command 80 -> frame 15B -> check in 260 (here: always a refusal)
    bool flashBusy_ = false;           // from the last tick(): gate condition
    bool otaBusy_ = false;             // setTxBlock: program update session
    bool nvsPending_ = false;          // setTxBlock: a settings write is pending
    TxFault lastFault_ = TxFault::NONE;   // for the "[cantx] FAULT" line on a new latch
    psa::ParkDiag parkDiag_;           // overnight parking sensors record (own RAM record until attachParkDiag)
    RawFrame start260_;                // first complete frame 260 since startup (reference state for "vset")
    PowerOutputs powerOut_;
    bool logKeys_ = true;
    bool haveLastKey_ = false;
    xp::KeyEvent lastKey_;
    uint32_t lastKeyMs_ = 0;

    uint32_t framesSent_ = 0, framesSuppressed_ = 0, framesTooLong_ = 0;
    uint32_t lastTxMs_ = 0;

    DecoderOutputs out_;

    void logf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void logRadioCommand(const RadioCmd& c);
    void onCanFrameCarData(const CanFrame& f, uint32_t now);   // profile decode -> radio encode
    TxConditions txConditions(uint32_t nowMs) const;
    void onSettingReport(const vset::ChangeReport& r);
    void pumpSettings();               // lines "[vset] 15B ... sent", report, "[cantx] FAULT"
    static void formatParkingOf(const psa::ParkingAid& pa, char* buf, size_t n);
};
