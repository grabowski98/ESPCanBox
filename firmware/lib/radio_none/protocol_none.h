// Radio protocol "none": no radio. src/radio_uart.cpp does not start the UART (no pin is
// assigned), the Decoder never hands out a frame, no key has a code. Selected with -DRADIO_PROTOCOL=NONE.
#pragma once
#include "radio_protocol.h"

extern const RadioProtocol kRadioNone;
