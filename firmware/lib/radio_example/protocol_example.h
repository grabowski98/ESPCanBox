// TEMPLATE radio protocol: how a radio protocol of your own is written (lib/radio_<name>), with every field of
// RadioProtocol (lib/core/radio_protocol.h) filled in. The radio, its framing and every frame number below
// are INVENTED for this example: no real head unit speaks it. It is compiled only by the host tests
// (test_host/radio_example.cpp), never into the firmware (lib_ignore in platformio.ini; -DRADIO_PROTOCOL=EXAMPLE
// stops the build with an error).
//
// To make your own protocol:
//   1. copy this directory to lib/radio_<name>, rename the files (protocol_<name>.h / .cpp: a header name must
//      be unique across lib/) and the object (kRadio<Name>), set "name" in library.json;
//   2. replace the framing, the parser and the frame numbers with the ones of YOUR radio, each one from your own
//      recording of the radio line (never from other people's code with an incompatible licence);
//   3. build with -DRADIO_PROTOCOL=CUSTOM -DRADIO_PROTOCOL_CUSTOM_HEADER='"protocol_<name>.h"'
//      -DRADIO_PROTOCOL_CUSTOM_OBJECT=kRadio<Name> and lib_deps = radio_<name> (src/config.h,
//      platformio_override.sample.ini). A custom protocol is listen only on CAN (no transmitting environment).
//
// Invented framing of this example (UART 19200 8N1), both directions:
//     55 AA  LEN  CMD  DATA[LEN]  SUM        SUM = (LEN + CMD + sum of DATA) & 0xFF
// No acknowledgement byte. Frames to the radio: 10 power, 11 key, 12 parking, 13 obstacle, 20 outside temperature,
// 21 steering angle, 22 doors, 7F version. Frames from the radio: 80 "send me everything", 81 volume.
// The radio sends no commands (decodeCommand = nullptr): nothing from it can reach the CAN side.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "radio_protocol.h"

namespace example_radio {

const uint8_t SYNC1 = 0x55;
const uint8_t SYNC2 = 0xAA;
const uint32_t UART_BAUD = 19200;

// Frame numbers (invented).
const uint8_t CMD_POWER = 0x10;
const uint8_t CMD_KEY = 0x11;
const uint8_t CMD_PARKING = 0x12;
const uint8_t CMD_OBSTACLE = 0x13;
const uint8_t CMD_OUTSIDE_TEMP = 0x20;
const uint8_t CMD_STEERING = 0x21;
const uint8_t CMD_DOORS = 0x22;
const uint8_t CMD_VERSION = 0x7F;
const uint8_t RX_SEND_ALL = 0x80;
const uint8_t RX_VOLUME = 0x81;

const uint8_t KEY_MOON = 0x08;   // the example's "screen off" key: a latch key (protocol table)

uint8_t checksum(uint8_t len, uint8_t cmd, const uint8_t* data);
// Framing: returns the number of bytes, 0 = does not fit in 'cap' or len > RADIO_FRAME_MAX_DATA.
size_t encode(const RadioFrame& f, uint8_t* out, size_t cap);

// Byte-stream parser: state machine with length check, checksum and frame timeout.
class Parser : public RadioRxParser {
public:
    explicit Parser(uint16_t frameTimeoutMs = 30) : timeoutMs_(frameTimeoutMs) {}
    Result feed(uint8_t byte, uint32_t nowMs, RadioFrame& out) override;
    void reset() override;
    const RadioRxStats& stats() const override { return stats_; }

private:
    enum State : uint8_t { WAIT_SYNC1, WAIT_SYNC2, WAIT_LEN, WAIT_CMD, WAIT_DATA, WAIT_SUM };
    State state_ = WAIT_SYNC1;
    RadioFrame cur_;
    uint8_t got_ = 0;
    uint32_t lastByteMs_ = 0;
    uint16_t timeoutMs_;
    RadioRxStats stats_;
};

}  // namespace example_radio

extern const RadioProtocol kRadioExample;
