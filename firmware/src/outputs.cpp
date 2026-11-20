#include "outputs.h"

#include <driver/gpio.h>

#include "config.h"

void outputsBegin() {
    // Low level = NPN transistor off = P-MOSFET switch off.
    // We set the level before switching the pin to output, so that there is no pulse.
    // gpio_set_level, not digitalWrite: in this Arduino version (platform espressif32@8.0.0)
    // digitalWrite before pinMode writes nothing (description in can_bus.cpp at SLNT).
    // gpio_set_level writes 0 to the output register, pinMode does not change it.
    const int pins[] = {PIN_OUT_ACC, PIN_OUT_ILL, PIN_OUT_REV, PIN_OUT_4};
    for (int p : pins) {
        gpio_set_level((gpio_num_t)p, 0);
        pinMode(p, OUTPUT);
        digitalWrite(p, LOW);   // once more after pinMode, as in can_bus.cpp so far
    }
}

void outputsSetRadio(bool acc, bool illumination, bool reverse) {
    // A high level turns on the NPN transistor, which switches on the P-MOSFET and the output carries 12 V.
    digitalWrite(PIN_OUT_ACC, acc ? HIGH : LOW);
    digitalWrite(PIN_OUT_REV, reverse ? HIGH : LOW);
    digitalWrite(PIN_OUT_ILL, illumination ? HIGH : LOW);
}

void outputsAllOff() {
    digitalWrite(PIN_OUT_ACC, LOW);
    digitalWrite(PIN_OUT_ILL, LOW);
    digitalWrite(PIN_OUT_REV, LOW);
    digitalWrite(PIN_OUT_4, LOW);
}

void statusLed(uint8_t r, uint8_t g, uint8_t b) {
    rgbLedWrite(PIN_STATUS_LED, r, g, b);    // RGB LED (WS2812) on the ESP32 module
}

float batteryVolts() {
#if HAS_VBAT_DIVIDER
    return analogReadMilliVolts(PIN_VBAT_SENSE) * 0.001f * VBAT_DIVIDER;
#else
    return 0.0f;
#endif
}
