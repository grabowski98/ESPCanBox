// Board: home-made prototype v1 (hardware/prototype-v1, KiCad sheet "procesor").
// Module: Waveshare ESP32-S3-DEV-KIT-N16R8. Selected with -DBOARD=PROTO_V1 (platformio.ini).
//
// This file holds ONLY pin numbers and board components. What the device may send (operating mode,
// CAN transmit, network) stays in src/config.h and is off by default.
// Rules checked at compile time (boards/board_common.h): no GPIO 35-37 (octal PSRAM of the N16R8)
// and no 19/20 (USB); strapping pins 0/3/45/46 only with a BOARD_<PIN>_STRAP_OK macro and a comment.
// No Arduino.h here: the values are expanded in src/config.h, after Arduino.h.
#pragma once

#define BOARD_NAME "proto_v1"

// ---------- CAN (Adafruit CAN Pal module, TJA1051T/3) ----------
#define BOARD_PIN_CAN_RX GPIO_NUM_4         // from the module: RX
// To the module: TX, through the transmitter jumper (JP1 on the prototype, JP_TX on the target board).
// On the prototype JP1 has been CLOSED since 17.11.2026: the TX line reaches the module.
#define BOARD_PIN_CAN_TX GPIO_NUM_5
// Module SLNT (pin S of the TJA1051T/3): HIGH = silent mode (chip transmitter disabled,
// receiver working). can_bus.cpp sets the high level before switching the pin to output.
// Data from the NXP TJA1051 datasheet, Rev. 9 (28.11.2017), https://www.nxp.com/docs/en/data-sheet/TJA1051.pdf:
//   - Table 7: pin S high-level threshold >= 0.7 x VIO (2.31 V at 3.3 V), low-level <= 0.3 x VIO (0.99 V);
//   - sect. 7.2.2 and Table 7 (IIH 1-10 uA): pin S has an internal PULL-DOWN TO GROUND, so left unconnected
//     it means normal mode (transmitter active). That is why R3 and the continuity of the SLNT wire are critical;
//   - sect. 7.2.2: TXD has an internal pull-up to VIO (undriven = recessive state);
//   - Table 8: TXD -> bus delay 65-90 ns, TXD dominant time-out 0.3-5 ms;
//   - the transition time from silent to normal mode is NOT given in the datasheet: to be measured.
// Prototype (kicad/proto): on the board SLNT has only R3 (10 kΩ to 3.3 V). Since 17.11.2026 a wire
// SLNT -> GPIO6 is soldered on and jumper JP1 is closed, so transmission is now decided only by the program.
// In the five environments without transmission, and in the transmitting program as long as it is not armed:
// this pin is permanently high and TWAI is in listen-only mode. The LOW level is set only by
// src/can_tx/can_tx_port.cpp (environment decoder_tx_wifi), for the duration of one frame.
#define BOARD_PIN_CAN_SILENT GPIO_NUM_6

// ---------- Sound (MAX98357A module, I2S) ----------
#define BOARD_PIN_I2S_BCLK 15
#define BOARD_PIN_I2S_LRC 16
#define BOARD_PIN_I2S_DIN 17
// Module SD: low = amplifier off. On the board it is pulled down by a 10k resistor,
// so there is silence before the program starts.
#define BOARD_PIN_AMP_SD 18

// ---------- 12 V outputs to the radio (P-MOSFET switches, high level switches on) ----------
#define BOARD_PIN_OUT_ACC 12                // radio ACC (box connector: bottom 3)
#define BOARD_PIN_OUT_ILL 13                // illumination (top 3): lights on according to frame 036
#define BOARD_PIN_OUT_REV 14                // reverse gear (bottom 4)
#define BOARD_PIN_OUT_4 21                  // spare (top 4, wire "BRAKE")

// ---------- UART to the radio: 38400 8N1, 3.3 V ----------
// Directions measured 11.11.2026 with the logger:
//   the box TRANSMITS on wire "bottom 8" of the box connector (white),
//   the radio TRANSMITS on wire "top 7" (grey-white).
// JP_UART jumpers on the board: PIN_RADIO_TX -> bottom 8, PIN_RADIO_RX <- top 7.
// Series resistors on the prototype board: 1 kΩ and 4.7 kΩ (measured 15.11.2026;
// which one sits on which line: to be checked with a meter).
#define BOARD_PIN_RADIO_TX 10               // pad J4 "radio RX"; unassigned in MODE_BESIDE_BOX
#define BOARD_PIN_RADIO_RX 11               // pad J4 "radio TX"

// ---------- Other ----------
#define BOARD_PIN_I2C_SDA 8
#define BOARD_PIN_I2C_SCL 9
#define BOARD_PIN_VBAT_SENSE 7              // 470k / 100k divider from the 12 V rail
// Divider resistors in kΩ: VBAT_DIVIDER = (top + bottom) / bottom.
#define BOARD_VBAT_R_TOP_K 470.0f
#define BOARD_VBAT_R_BOTTOM_K 100.0f
// Divider R1/R2 is on the prototype board (M1 pin 7 = GPIO7, checked 13.11.2026);
// on a wired kit without a divider set this to 0.
// The measurement shows the voltage BEHIND diode D1 (approx. 0.7-0.9 V less than at the battery). Without 12 V
// (powered from USB) it shows approx. 0-1.5 V of noise (17.11.2026: 0.23-1.36 V); below NET_VBAT_VALID_MIN_V the console
// and the panel assume there is no 12 V.
#define BOARD_HAS_VBAT_DIVIDER 1
#define BOARD_PIN_STATUS_LED 38             // RGB LED on the Waveshare module
// Emergency button on the target board: shorts to ground.
#define BOARD_PIN_BUTTON 3                  // with internal pull-up (INPUT_PULLUP)
// GPIO3 is an ESP32-S3 strapping pin (JTAG signal source). Per the ESP32-S3 datasheet (section on strapping
// pins) its level is sampled at reset only when eFuse STRAP_JTAG_SEL is burned; with default eFuses
// it is ignored. The button is open at reset and the pin is otherwise floating/pulled up, so booting is not affected.
// Not verified on this unit: read the eFuses (espefuse.py summary) before relying on it.
#define BOARD_PIN_BUTTON_STRAP_OK 1
