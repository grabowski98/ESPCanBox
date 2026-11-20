// Hardware-independent CAN frame.
// The lib/core directory uses nothing from Arduino or ESP32, so the same logic
// can be compiled on a PC and checked against a capture from the car (test_host/).
#pragma once
#include <stdint.h>

struct CanFrame {
    uint32_t id = 0;        // 11-bit identifier
    uint8_t len = 0;        // number of data bytes, 0..8
    uint8_t data[8] = {0};
};
