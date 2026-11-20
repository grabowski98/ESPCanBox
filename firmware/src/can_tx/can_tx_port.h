// Hardware for transmitting on the car CAN: implementation of the CanTxPort interface (lib/core/can_tx_gate.h).
// The src/can_tx/ directory is compiled ONLY in environment decoder_tx_wifi
// (platformio.ini: in the others the filter -<can_tx/>). Description and rules: can_tx_port.cpp.
#pragma once
#include <stdint.h>

#include "can_tx_gate.h"

// Call once in setup(), after canBegin(). Creates the hardware timer of the transmitter window.
// false = the timer could not be created: the gate must NOT be armed then (the port will not open the transmitter anyway).
bool canTxPortBegin();
// Port for the gate (Decoder::attachCanTx). Always the same static object.
CanTxPort* canTxPort();
// How many times the transmitter was closed by the hardware timer instead of the gate (should be 0).
uint32_t canTxPortHwTimeouts();
