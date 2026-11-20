// 12 V outputs (ACC, illumination, reverse, spare) and the status LED.
// After startup all 12 V outputs are off; only MODE_DECODER switches them on.
#pragma once
#include <stdint.h>

void outputsBegin();                        // all 12 V outputs off
// Outputs to the radio. Call only in MODE_DECODER (box unplugged).
void outputsSetRadio(bool acc, bool illumination, bool reverse);
void outputsAllOff();                       // safe state
void statusLed(uint8_t r, uint8_t g, uint8_t b);
float batteryVolts();                       // 0 when the board has no divider
