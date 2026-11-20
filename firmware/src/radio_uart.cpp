#include "radio_uart.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "console.h"

// UART1 of the ESP32-S3 chip. UART0 stays for the USB-UART bridge on the module, and the console goes over
// native USB, so UART1 is free.
static HardwareSerial& port = Serial1;

// Parser of the frames from the radio: the one of the selected protocol (RadioProtocol::rxParser, a static
// instance in the protocol library). nullptr = the protocol receives nothing: bytes are read and dropped.
static RadioRxParser* const parser = RADIO_PROTOCOL_SELECTED.rxParser;
static const RadioRxStats noRxStats;
static RadioUartStats stats;
static bool txEnabled = false;
static bool started = false;
static bool logRx = true;

// The gap between our frames (RadioProtocol::txGapMs; XP: 10 ms) is enforced by Decoder::nextRadioTx
// (lib/app/decoder.h). XP: at startup the box sends frames approx. every 20 ms; 10 ms gives the radio time
// for the acknowledgement (1 byte = 0.26 ms) with a large margin.
// How many bytes we handle in one loop pass: limits the time spent here
// when a long burst of garbage appears on the line.
static const int RX_BUDGET = 96;
// Time for the parser's frame timeout (XP: 30 ms between bytes of one frame). Not the loop time: a loop pass
// stalled by a flash write would make a frame already complete in the UART buffer look cut off
// (lib/core/radio_protocol.h, RadioRxClock).
static RadioRxClock rxClock;

bool radioUartBegin(bool enableTx) {
    txEnabled = enableTx;
    // Radio protocol without a UART (RADIO_PROTOCOL=NONE): the port is not started and no pin
    // is assigned, so nothing can be sent. Not an error: the device simply has no radio.
    if (RADIO_PROTOCOL_SELECTED.uartBaud == 0) {
        txEnabled = false;
        started = false;
        return true;
    }
    port.setRxBufferSize(512);      // before begin(); the longest frame has 37 bytes
    // Third and fourth parameter: RX and TX pin. TX = -1: the port gets no output,
    // so in "beside the box" mode it physically cannot send anything.
    port.begin(RADIO_PROTOCOL_SELECTED.uartBaud, SERIAL_8N1, PIN_RADIO_RX, enableTx ? PIN_RADIO_TX : -1);
    started = (bool)port;
    return started;
}

void radioUartPoll(uint32_t nowMs, Decoder& decoder) {
    if (!started) {
        // No UART (radio "none"): the decoder only empties its queue for the radio (returns 0, sends nothing).
        if (RADIO_PROTOCOL_SELECTED.uartBaud == 0) {
            uint8_t none[1];
            decoder.nextRadioTx(nowMs, false, none, 0);
            stats.framesSuppressed = decoder.radioTxSuppressed();
        }
        return;
    }

    const uint32_t rxMs = rxClock.observe(nowMs, port.available() <= 0);
    int budget = RX_BUDGET;
    while (budget-- > 0 && port.available() > 0) {
        int c = port.read();
        if (c < 0) break;
        RadioFrame f;
        if (parser && parser->feed((uint8_t)c, rxMs, f) == RadioRxParser::FRAME) {
            // Order per radio frame: acknowledgement F3 -> decoder -> preview.
            // Acknowledgement BEFORE the decoder and the preview: in the program with network the console line also goes
            // to the panel (a short wait for the lock, up to 2 ms) and must not delay the reply to the radio.
            if (txEnabled && RADIO_PROTOCOL_SELECTED.sendAck) {   // acknowledgement of the protocol (XP: F3, like the box)
                port.write(RADIO_PROTOCOL_SELECTED.ackByte);
                stats.acksSent++;
            }
            decoder.onRadioFrame(f, nowMs);
            // Preview: command and data, as they came from the radio. The time (A6) is repeated by the radio
            // approx. every 1 s, and the source (C0) every second with the track time counter: these two
            // are printed only on change (for C0 the first 5 bytes count).
            static uint8_t lastTime[5], lastSource[5];
            bool show = logRx;
            // XP frame numbers: only with the XP protocol (another protocol prints every frame).
            if (RADIO_PROTOCOL_IS_XP && show && (f.cmd == xp::RADIO_TIME || f.cmd == xp::RADIO_SOURCE) && f.len >= 5) {
                uint8_t* last = f.cmd == xp::RADIO_TIME ? lastTime : lastSource;
                show = memcmp(last, f.data, 5) != 0;
                memcpy(last, f.data, 5);
            }
            if (show) {
                conPrintf("[radio_rx] %02X", f.cmd);
                for (uint8_t i = 0; i < f.len; i++) conPrintf(" %02X", f.data[i]);
                conPrintln();
            }
        }
    }

    // At most one frame per loop pass. With txEnabled = false the decoder only drains the queue
    // (returns 0), so the port sends nothing; the TX pin is not assigned then anyway.
    uint8_t buf[RADIO_TX_MAX_BYTES];
    size_t n = decoder.nextRadioTx(nowMs, txEnabled, buf, sizeof(buf));
    if (n > 0) port.write(buf, n);
    stats.framesSent = decoder.radioTxSent();
    stats.framesSuppressed = decoder.radioTxSuppressed();
}

bool radioUartTxEnabled() { return txEnabled; }
void radioUartSetLog(bool on) { logRx = on; }
bool radioUartLog() { return logRx; }
RadioUartStats radioUartStats() { return stats; }
const RadioRxStats& radioUartParserStats() { return parser ? parser->stats() : noRxStats; }
