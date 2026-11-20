// Sound output: tone generator on I2S for the MAX98357A module.
#pragma once
#include <stdint.h>

bool audioBegin();
void audioSetTone(bool on, uint16_t freqHz);   // timbre and envelope modelled on the car's buzzer
void audioSetVolume(uint8_t percent);          // 0..100
uint8_t audioVolume();
void audioSetAmp(bool on);                     // module SD pin: false = amplifier asleep
