// Serial port to the radio (protocol from -DRADIO_PROTOCOL, src/config.h; default XP, 38400 8N1).
// Reception always works. Transmission only when it is enabled at startup
// (MODE_DECODER); otherwise the TX pin is not assigned to the port at all.
#pragma once
#include <stdint.h>

#include "decoder.h"
#include "xp_protocol.h"

struct RadioUartStats {
    uint32_t framesSent = 0;        // frames sent to the radio
    uint32_t framesSuppressed = 0;  // frames prepared but not sent (transmission disabled)
    uint32_t acksSent = 0;          // acknowledgements sent to the radio
};

bool radioUartBegin(bool enableTx);                 // false = the port did not start
// Call on every loop, after decoder.tick(). Frames from the radio go to decoder.onRadioFrame,
// frames to the radio are taken from decoder.nextRadioTx (the 10 ms gap and queue draining are in the decoder).
void radioUartPoll(uint32_t nowMs, Decoder& decoder);
bool radioUartTxEnabled();
void radioUartSetLog(bool on);                      // printing of frames from the radio to the console
bool radioUartLog();
RadioUartStats radioUartStats();
const RadioRxStats& radioUartParserStats();     // counters of the protocol parser (zeros without one)
