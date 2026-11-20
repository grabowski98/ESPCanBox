// Radio protocol: what the firmware needs to know about the link to the
// head unit, as one const structure. No allocation, no hardware, no Arduino framework header.
//
// Selected at build time (-DRADIO_PROTOCOL=..., src/config.h) and handed to the Decoder through
// DecoderConfig::radio (lib/app/decoder.h; default: XP). Protocols:
//   lib/radio_xp/       XP (Simple Soft box <-> FYT radio, UART 38400 8N1), reference, default
//   lib/radio_none/     no radio: no UART, nothing is ever sent
//   lib/radio_example/  template with an invented framing, compiled only by the host tests
//   your own            -DRADIO_PROTOCOL=CUSTOM (src/config.h)
//
// The structure carries everything that is specific to one radio:
//   - the line: UART speed, framing of one frame into bytes (encodeFrame), the byte-stream parser of the
//     frames from the radio (rxParser), the acknowledgement byte and the gap between frames;
//   - what goes to the radio: key codes, power, parking sensors, obstacle, version, and the car data for the
//     radio screen (car-neutral types from vehicle_signals.h -> one radio frame);
//   - what comes from the radio: radio state (decodeRadioFrame) and commands (decodeCommand);
//   - protocol properties as const tables: keys with a latch, state frames with a rate limit.
// The common layer (RadioSession in lib/core/radio_session.h: queue, state slots, repeat and gap timing)
// and the Decoder use only these fields; neither knows a frame number or a byte of any protocol.
// Any pointer may be nullptr: "this radio has no such frame" (nothing is built or sent).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "vehicle_signals.h"

// One frame for the radio, before framing (header, length, checksum ...). The XP protocol uses this type as
// xp::Frame. cmd = frame number in the protocol, len = number of bytes in data. The framing into bytes on the
// line is the protocol's own (RadioProtocol::encodeFrame).
const uint8_t RADIO_FRAME_MAX_DATA = 40;
struct RadioFrame {
    uint8_t cmd = 0;
    uint8_t len = 0;
    uint8_t data[RADIO_FRAME_MAX_DATA] = {0};
};

// Room for one framed frame on the line: data plus up to 8 bytes of framing (header, length, checksum ...).
// src/radio_uart.cpp sizes its transmit buffer with it; encodeFrame returns 0 when a frame does not fit.
const size_t RADIO_TX_MAX_BYTES = RADIO_FRAME_MAX_DATA + 8;

// One key event for the radio: key code of the protocol, state (1 pressed, 2 held, 0 released).
// The XP protocol uses this type as xp::KeyEvent.
struct RadioKeyEvent {
    uint8_t code = 0;
    uint8_t state = 0;
};

// Byte-stream parser of the frames from the radio: a state machine of the protocol (header, length check,
// checksum, frame timeout). One instance per program, owned by the protocol library (RadioProtocol::rxParser).
struct RadioRxStats {
    uint32_t frames = 0;         // frames with a valid checksum
    uint32_t acks = 0;           // acknowledgement bytes between frames
    uint32_t badChecksum = 0;
    uint32_t badLength = 0;      // length byte greater than the frame buffer
    uint32_t timeouts = 0;       // frame cut off halfway
    uint32_t strayBytes = 0;     // bytes outside frames that are not acknowledgements
};
class RadioRxParser {
public:
    enum Result : uint8_t {
        NONE,    // byte accepted, nothing ready
        FRAME,   // a complete valid frame in 'out'
        ACK,     // acknowledgement byte outside a frame
    };
    virtual Result feed(uint8_t byte, uint32_t nowMs, RadioFrame& out) = 0;
    virtual void reset() = 0;
    virtual const RadioRxStats& stats() const = 0;

protected:
    ~RadioRxParser() = default;     // never deleted through this type (static instances only)
};

// Time to give RadioRxParser::feed when the bytes are read from a receive buffer once per loop pass
// (src/radio_uart.cpp). Found in a code review: with the loop time a stalled pass (e.g. a flash write over 30 ms)
// made a frame that had fully arrived in the buffer look cut off. The arrival time of a buffered byte is unknown,
// so this clock advances only over intervals in which the buffer was seen EMPTY, i.e. over time in which no byte
// can have arrived. A gap the parser measures is then never longer than the real gap on the line, and in a normal
// 1 ms loop it is shorter by at most one pass.
class RadioRxClock {
public:
    // Once per poll, before reading: bufferEmpty = no byte is waiting now. Returns the time for feed().
    uint32_t observe(uint32_t nowMs, bool bufferEmpty) {
        // Empty now: every byte that was waiting at the previous poll has been read, and none has come since.
        if (have_ && bufferEmpty) clock_ += nowMs - lastMs_;
        lastMs_ = nowMs;
        have_ = true;
        return clock_;
    }

private:
    bool have_ = false;
    uint32_t lastMs_ = 0;
    uint32_t clock_ = 0;
};

// What the radio told us about itself (filled by RadioProtocol::decodeRadioFrame; shown in the console and
// the web panel). A protocol fills only the fields it has.
struct RadioInfo {
    bool everHeard = false;        // at least one valid frame arrived from the radio
    uint32_t lastFrameMs = 0;
    bool radioOn = false;
    uint8_t volume = 0;
    bool muted = false;
    uint8_t source[8] = {0};       // raw content of the "source" frame
    bool haveTime = false;
    uint8_t year = 0, month = 0, day = 0, hour = 0, minute = 0;   // year = 2000 + year
    // latest commands we do not execute yet (they would require transmitting on CAN)
    uint8_t lastSettingId = 0, lastSettingValue = 0;
    uint8_t lastAirId = 0, lastAirValue = 0;
    uint32_t settingCount = 0, airCount = 0, unknownCount = 0;
};
// Flags returned by RadioProtocol::decodeRadioFrame.
const uint8_t RADIO_RX_SEND_ALL = 0x01;   // the radio asks for everything: version, parking, all state frames

// A command from the radio, already decoded by the protocol (RadioProtocol::decodeCommand).
// The setting numbers (SETTING) are the numbers of the vehicle settings table of the reference car profile.
enum class RadioCmdKind : uint8_t {
    NONE,        // a different command or frame too short
    SETTING,     // vehicle setting: id = item number, value
    AIR,         // climate control from the screen: id = button number, value
    SOUND,       // factory amplifier sound: id = item number, value
    REQUEST,     // frame request (id = number of the requested frame)
    RADIO_ON,    // the radio reports "on"
    RADIO_OFF,   // the radio reports "shutting down"
};
struct RadioCmd {
    RadioCmdKind kind = RadioCmdKind::NONE;
    uint8_t id = 0;
    uint8_t value = 0;
};

// A state frame whose sending is limited in rate (e.g. the steering angle, which changes on the bus every
// dozen or so ms): at most one frame with this number per minMs. A property of the protocol.
struct RadioRateLimit {
    uint8_t cmd;
    uint16_t minMs;
};

struct RadioProtocol {
    const char* id;                         // short ASCII name, e.g. "xp", "none"
    // UART to the radio, 8N1. 0 = the protocol has no UART: src/radio_uart.cpp does not start the port
    // (no pin is assigned) and the Decoder sends nothing.
    uint32_t uartBaud;
    // Logical key -> key code sent to the radio. 0 = this key is not sent. nullptr = no keys at all.
    uint8_t (*keyCode)(vehicle::KeyId key);

    // ---- car data for the radio screen ----
    // Each encoder writes one radio frame and returns true; false = nothing to send. nullptr = the radio
    // does not show this data (the Decoder then does not ask). The frame is queued as a "state" frame:
    // sent when its content changes (lib/core/radio_session.h). An encoder never sees a CAN frame.
    bool (*encodeSteering)(const vehicle::SteeringAngle& s, RadioFrame& out);
    // 'page' = one of vehicle::TRIP_NOW, TRIP_1, TRIP_2 (the page the car just updated).
    bool (*encodeTrip)(const vehicle::TripData& t, uint8_t page, RadioFrame& out);
    bool (*encodeOutsideTemp)(const vehicle::OutsideTemp& t, RadioFrame& out);
    bool (*encodeEcoMode)(const vehicle::EcoMode& e, RadioFrame& out);
    bool (*encodeClimate)(const vehicle::ClimateState& c, RadioFrame& out);
    // Doors and setting values in one frame (the XP frame 38 carries both). 'io' is in/out: on entry the
    // frame this encoder built last time for this Decoder (all zeros at first); a field the neutral data
    // do not state (SettingItem::known = false, or settings not valid yet) keeps its previous content.
    // sideSwap: swap left and right for the radio (console "doorswap"; XP: on by default, see the encoder).
    bool (*encodeCarState)(const vehicle::DoorState& d, const vehicle::VehicleSettings& s, bool sideSwap,
                           RadioFrame& io);
    // Which setting items the car has (the XP frame 3C: rows the radio shows).
    bool (*encodeSettingsPresent)(const vehicle::VehicleSettings& s, RadioFrame& out);

    // ---- line framing ----
    // The fields from here on may be left out of an initializer (they are then zero / nullptr: no framing,
    // nothing sent, nothing received), so a protocol that only gives the encoders above still compiles.
    // Writes one frame as bytes on the line (header, length, checksum ... of this protocol). Returns the number
    // of bytes, 0 = does not fit in 'cap' (the frame is dropped). nullptr = the protocol sends nothing.
    size_t (*encodeFrame)(const RadioFrame& f, uint8_t* out, size_t cap);
    // Parser of the byte stream from the radio (one static instance of the protocol). nullptr = nothing is
    // received (the bytes from the radio are read and ignored).
    RadioRxParser* rxParser;
    // Acknowledgement: when sendAck is true, the byte ackByte is sent back to the radio after every valid frame
    // from it (before the frame is handled). false = the protocol has no acknowledgement.
    bool sendAck;
    uint8_t ackByte;
    // Shortest gap between two frames to the radio, in ms (the radio needs time for its acknowledgement).
    uint16_t txGapMs;

    // ---- frames the common layer sends (RadioSession) ----
    // Radio power (ACC on / off). Sent also while ACC is off: the only frame that is.
    bool (*encodePower)(bool accOn, RadioFrame& out);
    // One key event (code from keyCode, after the key remapping).
    bool (*encodeKey)(const RadioKeyEvent& e, RadioFrame& out);
    // Parking sensors. 'valid' = false when the car stopped sending them ("no data").
    bool (*encodeParking)(const vehicle::ParkingState& p, bool valid, RadioFrame& out);
    // While the sensors are active the parking frame is repeated every parkingRepeatMs (0 = only on change).
    uint16_t parkingRepeatMs;
    // "Sensors see an obstacle" flag (a state frame).
    bool (*encodeObstacle)(bool obstacle, RadioFrame& out);
    // Version frame, sent when the radio asks for everything (RADIO_RX_SEND_ALL).
    bool (*encodeVersion)(RadioFrame& out);

    // ---- frames from the radio ----
    // Updates 'info' from one frame of the radio and returns RADIO_RX_* flags (0 = nothing to do).
    uint8_t (*decodeRadioFrame)(const RadioFrame& f, RadioInfo& info);
    // Command from the radio (vehicle setting, climate, sound, radio on / off). false = not a command.
    // nullptr = this radio sends no commands: nothing from the radio ever reaches the CAN side.
    bool (*decodeCommand)(const RadioFrame& f, RadioCmd& out);

    // ---- protocol properties (const tables, nullptr / 0 = none) ----
    // Keys with a latch: a press toggles, the first press sends "pressed", the next one "released"; at ignition
    // off a latched key is released (lib/radio_xp/xp_keys_custom.h). Codes of this protocol.
    const uint8_t* latchKeys;
    uint8_t latchKeyCount;
    // State frames with a rate limit.
    const RadioRateLimit* rateLimits;
    uint8_t rateLimitCount;
    // State frames: how many leading data bytes, besides the frame number, tell two state frames apart
    // (0..2). 0 = one state per frame number (XP: every state frame has its own number). A protocol with one
    // "car data" frame number and a page / sub-type in DATA[0] sets 1, so its pages do not overwrite each other.
    uint8_t stateKeyBytes;
    // Numbering of the SETTING / AIR / SOUND commands from decodeCommand. true = the numbers of the reference
    // command table (lib/app/xp_settings.h, vset: the XP radio's own numbers); the Decoder then hands them to the
    // vehicle settings change and prints them as such. false (default) = the Decoder does not interpret them:
    // there is no car-neutral command path yet (ROADMAP), so it only reports "not supported" on the console.
    bool commandsUseVsetNumbers;
};
