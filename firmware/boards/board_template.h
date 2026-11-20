// Board template: starting point for your own board. Selected with -DBOARD=TEMPLATE only to show
// that an unfilled board stops the build.
//
// How to add a board:
//   1. Copy this file to boards/board_<yourname>.h and fill in every pin (GPIO numbers of the ESP32-S3).
//   2. Delete the #error line below.
//   3. Either add an entry in src/config.h (BOARD_ID_<YOURNAME>) or, without touching src/, build with
//      -DBOARD=CUSTOM -DBOARD_CUSTOM_HEADER='"board_<yourname>.h"' (see platformio_override.sample.ini).
//   4. Build: boards/board_common.h refuses GPIO 35-37 and 19/20, unjustified strapping pins 0/3/45/46,
//      non-existent GPIOs and one GPIO used twice.
// Only pins and board components go here. Whether the device may send anything (12 V outputs, UART to the
// radio, CAN transmit) is decided in src/config.h and platformio.ini, all off by default.
// Reference: boards/board_proto_v1.h (comments there describe the prototype circuit).
#pragma once

#error "boards/board_template.h: fill in pins. Copy it to boards/board_<name>.h, set every BOARD_PIN_* and remove this #error."

// #define BOARD_NAME "my_board"

// ---------- CAN transceiver ----------
// #define BOARD_PIN_CAN_RX GPIO_NUM_?       // transceiver RXD
// #define BOARD_PIN_CAN_TX GPIO_NUM_?       // transceiver TXD
// Transceiver silent/standby pin: HIGH must mean "transmitter off". Check the datasheet of your
// transceiver: if the pin has an internal pull-down (TJA1051 S pin), add an external pull-up.
// #define BOARD_PIN_CAN_SILENT GPIO_NUM_?

// ---------- Sound (I2S amplifier, e.g. MAX98357A) ----------
// #define BOARD_PIN_I2S_BCLK ?
// #define BOARD_PIN_I2S_LRC ?
// #define BOARD_PIN_I2S_DIN ?
// #define BOARD_PIN_AMP_SD ?                // amplifier shutdown, low = off; pull-down on the board

// ---------- 12 V outputs (high level switches on) ----------
// #define BOARD_PIN_OUT_ACC ?
// #define BOARD_PIN_OUT_ILL ?
// #define BOARD_PIN_OUT_REV ?
// #define BOARD_PIN_OUT_4 ?

// ---------- UART to the radio ----------
// #define BOARD_PIN_RADIO_TX ?
// #define BOARD_PIN_RADIO_RX ?

// ---------- Other ----------
// #define BOARD_PIN_I2C_SDA ?
// #define BOARD_PIN_I2C_SCL ?
// #define BOARD_PIN_VBAT_SENSE ?            // ADC pin behind a divider from the 12 V rail
// #define BOARD_VBAT_R_TOP_K 470.0f         // divider resistors in kΩ
// #define BOARD_VBAT_R_BOTTOM_K 100.0f
// #define BOARD_HAS_VBAT_DIVIDER 1          // 0 = no divider on the board
// #define BOARD_PIN_STATUS_LED ?            // WS2812 RGB LED
// #define BOARD_PIN_BUTTON ?                // to ground, internal pull-up
// A strapping pin (0, 3, 45, 46) is accepted only with a justification, e.g.:
// #define BOARD_PIN_BUTTON_STRAP_OK 1       // why the level of this pin at reset is safe
