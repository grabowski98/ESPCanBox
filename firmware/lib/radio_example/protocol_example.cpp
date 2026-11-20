// TEMPLATE radio protocol (invented radio, see protocol_example.h). Every function here works only on
// car-neutral data (vehicle_signals.h) and RadioFrame: no CAN identifier, no frame of another protocol.
#include "protocol_example.h"

#include <string.h>

namespace example_radio {

uint8_t checksum(uint8_t len, uint8_t cmd, const uint8_t* data) {
    uint8_t sum = (uint8_t)(len + cmd);
    for (uint8_t i = 0; i < len; i++) sum = (uint8_t)(sum + data[i]);
    return sum;
}

size_t encode(const RadioFrame& f, uint8_t* out, size_t cap) {
    if (f.len > RADIO_FRAME_MAX_DATA) return 0;      // never trust a length: the buffer has RADIO_FRAME_MAX_DATA
    const size_t total = (size_t)f.len + 5;          // 2 sync bytes, length, command, data, checksum
    if (cap < total) return 0;
    out[0] = SYNC1;
    out[1] = SYNC2;
    out[2] = f.len;
    out[3] = f.cmd;
    memcpy(out + 4, f.data, f.len);
    out[4 + f.len] = checksum(f.len, f.cmd, f.data);
    return total;
}

void Parser::reset() {
    state_ = WAIT_SYNC1;
    got_ = 0;
}

RadioRxParser::Result Parser::feed(uint8_t byte, uint32_t nowMs, RadioFrame& out) {
    // Frame cut off halfway: start over, so that one lost byte does not shift the following frames.
    if (state_ != WAIT_SYNC1 && (uint32_t)(nowMs - lastByteMs_) > timeoutMs_) {
        stats_.timeouts++;
        reset();
    }
    lastByteMs_ = nowMs;
    switch (state_) {
        case WAIT_SYNC1:
            if (byte == SYNC1) state_ = WAIT_SYNC2;
            else stats_.strayBytes++;
            return NONE;
        case WAIT_SYNC2:
            if (byte == SYNC2) {
                state_ = WAIT_LEN;
            } else {
                stats_.strayBytes++;
                state_ = byte == SYNC1 ? WAIT_SYNC2 : WAIT_SYNC1;
            }
            return NONE;
        case WAIT_LEN:
            if (byte > RADIO_FRAME_MAX_DATA) {            // the length from the line is checked before use
                stats_.badLength++;
                reset();
                return NONE;
            }
            cur_.len = byte;
            got_ = 0;
            state_ = WAIT_CMD;
            return NONE;
        case WAIT_CMD:
            cur_.cmd = byte;
            state_ = cur_.len ? WAIT_DATA : WAIT_SUM;
            return NONE;
        case WAIT_DATA:
            cur_.data[got_++] = byte;
            if (got_ >= cur_.len) state_ = WAIT_SUM;
            return NONE;
        case WAIT_SUM:
            reset();
            if (byte != checksum(cur_.len, cur_.cmd, cur_.data)) {
                stats_.badChecksum++;
                return NONE;
            }
            stats_.frames++;
            out = cur_;
            return FRAME;
    }
    reset();
    return NONE;
}

namespace {

Parser s_rxParser;     // one parser per program (src/radio_uart.cpp uses it through RadioProtocol::rxParser)

void start(RadioFrame& f, uint8_t cmd, uint8_t len) {
    f = RadioFrame();
    f.cmd = cmd;
    f.len = len;
}

uint8_t keyCode(vehicle::KeyId key) {
    using vehicle::KeyId;
    switch (key) {
        case KeyId::VOLUME_UP: return 0x01;
        case KeyId::VOLUME_DOWN: return 0x02;
        case KeyId::MUTE: return 0x03;
        case KeyId::NEXT: return 0x04;
        case KeyId::PREV: return 0x05;
        case KeyId::SOURCE: return 0x06;
        case KeyId::PHONE: return 0x07;
        case KeyId::MOON: return KEY_MOON;
        default: return 0;                              // every other key: not sent to this radio
    }
}

bool encodeSteering(const vehicle::SteeringAngle& s, RadioFrame& out) {
    if (!s.valid) return false;
    start(out, CMD_STEERING, 2);
    out.data[0] = (uint8_t)(s.value >> 8);
    out.data[1] = (uint8_t)s.value;
    return true;
}

bool encodeOutsideTemp(const vehicle::OutsideTemp& t, RadioFrame& out) {
    if (!t.valid) return false;
    start(out, CMD_OUTSIDE_TEMP, 2);
    const uint16_t v = (uint16_t)t.halfDegC;         // two's complement, high byte first
    out.data[0] = (uint8_t)(v >> 8);
    out.data[1] = (uint8_t)v;
    return true;
}

// Doors only: this invented radio has no vehicle settings screen, so the settings are ignored.
bool encodeCarState(const vehicle::DoorState& d, const vehicle::VehicleSettings& s, bool sideSwap, RadioFrame& io) {
    (void)s;
    (void)sideSwap;                                  // this radio shows the sides as the car reports them
    if (!d.valid) return false;
    start(io, CMD_DOORS, 1);
    io.data[0] = d.open;
    return true;
}

bool encodePower(bool accOn, RadioFrame& out) {
    start(out, CMD_POWER, 1);
    out.data[0] = accOn ? 1 : 0;
    return true;
}

bool encodeKey(const RadioKeyEvent& e, RadioFrame& out) {
    start(out, CMD_KEY, 2);
    out.data[0] = e.code;
    out.data[1] = e.state;
    return true;
}

// Byte 0: 0 = no data, 1 = active; bytes 1-6: zones 0..7 as the car gives them (rear L/C/R, front L/C/R).
bool encodeParking(const vehicle::ParkingState& p, bool valid, RadioFrame& out) {
    start(out, CMD_PARKING, 7);
    const bool active = valid && p.anyActive();
    out.data[0] = active ? 1 : 0;
    const uint8_t z[6] = {p.rearLeft, p.rearCenter, p.rearRight, p.frontLeft, p.frontCenter, p.frontRight};
    for (uint8_t i = 0; i < 6; i++) out.data[1 + i] = active ? z[i] : 7;
    return true;
}

bool encodeObstacle(bool obstacle, RadioFrame& out) {
    start(out, CMD_OBSTACLE, 1);
    out.data[0] = obstacle ? 1 : 0;
    return true;
}

bool encodeVersion(RadioFrame& out) {
    start(out, CMD_VERSION, 3);
    out.data[0] = 'E';
    out.data[1] = 'X';
    out.data[2] = '1';
    return true;
}

uint8_t decodeRadioFrame(const RadioFrame& f, RadioInfo& info) {
    switch (f.cmd) {
        case RX_SEND_ALL:
            return RADIO_RX_SEND_ALL;
        case RX_VOLUME:
            if (f.len >= 1) info.volume = f.data[0];
            return 0;
        default:
            info.unknownCount++;
            return 0;
    }
}

const uint8_t LATCH_KEYS[1] = {KEY_MOON};
const RadioRateLimit RATE_LIMITS[1] = {{CMD_STEERING, 200}};   // at most 5 angle frames per second

}  // namespace

}  // namespace example_radio

const RadioProtocol kRadioExample = {
    "example",
    example_radio::UART_BAUD,
    example_radio::keyCode,
    // car data for the radio screen (nullptr = this radio does not show it)
    example_radio::encodeSteering,
    nullptr,                              // trip computer
    example_radio::encodeOutsideTemp,
    nullptr,                              // economy mode
    nullptr,                              // climate control
    example_radio::encodeCarState,        // doors
    nullptr,                              // settings present
    // line
    example_radio::encode,
    &example_radio::s_rxParser,
    false, 0,                             // no acknowledgement byte
    20,                                   // 20 ms between frames
    // session frames
    example_radio::encodePower,
    example_radio::encodeKey,
    example_radio::encodeParking,
    500,                                  // parking frame repeated every 0.5 s while the sensors are active
    example_radio::encodeObstacle,
    example_radio::encodeVersion,
    // from the radio
    example_radio::decodeRadioFrame,
    nullptr,                              // no commands from this radio
    // tables
    example_radio::LATCH_KEYS, 1,
    example_radio::RATE_LIMITS, 1,
};
