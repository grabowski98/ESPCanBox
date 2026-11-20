// Pin assignment for the sniffer and the recorder. The only place where pins are set.
// Board: ESP32-S3 dev board, module ESP32-S3-WROOM-1-N16R8.
// Not used on purpose: GPIO 35-37 (octal PSRAM of the N16R8 module), GPIO 19/20 (native USB),
// strapping pins 0, 3, 45, 46.
#pragma once
#include "driver/gpio.h"

// --- CAN transceiver module (SN65HVD230 or any 3.3 V CAN transceiver) ---
// Module 3.3V -> board 3V3, GND -> GND, module "CAN RX" -> GPIO4.
// The module "CAN TX" input stays UNCONNECTED: the transceiver has an internal pull-up on TXD,
// so its driver stays recessive and cannot put anything on the bus.
static const gpio_num_t PIN_CAN_RX = GPIO_NUM_4;   // transceiver RXD -> ESP32
static const gpio_num_t PIN_CAN_TX = GPIO_NUM_5;   // required by the TWAI driver; leave the pin free, no wire

// --- recorder only: the two data lines between the CAN box and the radio ---
// Each line through a series resistor (value depends on the idle voltage of the line, see README).
// Only UART receivers are used: no TX pin is assigned, so the ESP32 cannot drive these lines.
static const int PIN_UART_A = 11;   // line A
static const int PIN_UART_B = 10;   // line B
