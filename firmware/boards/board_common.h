// Checks common to every board header (boards/board_*.h). Included by src/config.h right after
// the selected board header and after Arduino.h (the GPIO_NUM_x values must already be known).
//
// Pin rules for the ESP32-S3 N16R8 module (CLAUDE.md, "Firmware code standard"):
//   - GPIO 35, 36, 37: used by the octal PSRAM/flash of the N16R8 module: never;
//   - GPIO 19, 20: native USB (D-, D+), the USB console runs over it: never;
//   - GPIO 0, 3, 45, 46: strapping pins (boot mode, JTAG source, flash voltage, ROM log): only with
//     a macro BOARD_<PIN>_STRAP_OK 1 in the board header, next to a comment that explains why it is safe.
// A board header that forgets a pin stops the build with #error, not with a silent default.
#pragma once

#define BOARD_REQUIRED_MACROS_ "BOARD_PIN_CAN_RX/TX/SILENT, I2S_BCLK/LRC/DIN, AMP_SD, OUT_ACC/ILL/REV/4, RADIO_TX/RX, I2C_SDA/SCL, VBAT_SENSE, STATUS_LED, BUTTON, BOARD_VBAT_R_TOP_K, BOARD_VBAT_R_BOTTOM_K, BOARD_HAS_VBAT_DIVIDER"

#if !defined(BOARD_NAME) || !defined(BOARD_PIN_CAN_RX) || !defined(BOARD_PIN_CAN_TX) || !defined(BOARD_PIN_CAN_SILENT) || \
    !defined(BOARD_PIN_I2S_BCLK) || !defined(BOARD_PIN_I2S_LRC) || !defined(BOARD_PIN_I2S_DIN) ||                       \
    !defined(BOARD_PIN_AMP_SD) || !defined(BOARD_PIN_OUT_ACC) || !defined(BOARD_PIN_OUT_ILL) ||                         \
    !defined(BOARD_PIN_OUT_REV) || !defined(BOARD_PIN_OUT_4) || !defined(BOARD_PIN_RADIO_TX) ||                         \
    !defined(BOARD_PIN_RADIO_RX) || !defined(BOARD_PIN_I2C_SDA) || !defined(BOARD_PIN_I2C_SCL) ||                       \
    !defined(BOARD_PIN_VBAT_SENSE) || !defined(BOARD_PIN_STATUS_LED) || !defined(BOARD_PIN_BUTTON) ||                   \
    !defined(BOARD_VBAT_R_TOP_K) || !defined(BOARD_VBAT_R_BOTTOM_K) || !defined(BOARD_HAS_VBAT_DIVIDER)
#error "Board header incomplete: it must define BOARD_NAME and all of " BOARD_REQUIRED_MACROS_ " (see boards/board_template.h)"
#define BOARD_INCOMPLETE_ 1
// Stand-in values only so that the #error above is the only message (the build fails anyway).
#ifndef BOARD_NAME
#define BOARD_NAME "incomplete"
#endif
#ifndef BOARD_PIN_CAN_RX
#define BOARD_PIN_CAN_RX GPIO_NUM_NC
#endif
#ifndef BOARD_PIN_CAN_TX
#define BOARD_PIN_CAN_TX GPIO_NUM_NC
#endif
#ifndef BOARD_PIN_CAN_SILENT
#define BOARD_PIN_CAN_SILENT GPIO_NUM_NC
#endif
#ifndef BOARD_PIN_I2S_BCLK
#define BOARD_PIN_I2S_BCLK -1
#endif
#ifndef BOARD_PIN_I2S_LRC
#define BOARD_PIN_I2S_LRC -1
#endif
#ifndef BOARD_PIN_I2S_DIN
#define BOARD_PIN_I2S_DIN -1
#endif
#ifndef BOARD_PIN_AMP_SD
#define BOARD_PIN_AMP_SD -1
#endif
#ifndef BOARD_PIN_OUT_ACC
#define BOARD_PIN_OUT_ACC -1
#endif
#ifndef BOARD_PIN_OUT_ILL
#define BOARD_PIN_OUT_ILL -1
#endif
#ifndef BOARD_PIN_OUT_REV
#define BOARD_PIN_OUT_REV -1
#endif
#ifndef BOARD_PIN_OUT_4
#define BOARD_PIN_OUT_4 -1
#endif
#ifndef BOARD_PIN_RADIO_TX
#define BOARD_PIN_RADIO_TX -1
#endif
#ifndef BOARD_PIN_RADIO_RX
#define BOARD_PIN_RADIO_RX -1
#endif
#ifndef BOARD_PIN_I2C_SDA
#define BOARD_PIN_I2C_SDA -1
#endif
#ifndef BOARD_PIN_I2C_SCL
#define BOARD_PIN_I2C_SCL -1
#endif
#ifndef BOARD_PIN_VBAT_SENSE
#define BOARD_PIN_VBAT_SENSE -1
#endif
#ifndef BOARD_PIN_STATUS_LED
#define BOARD_PIN_STATUS_LED -1
#endif
#ifndef BOARD_PIN_BUTTON
#define BOARD_PIN_BUTTON -1
#endif
#ifndef BOARD_VBAT_R_TOP_K
#define BOARD_VBAT_R_TOP_K 0.0f
#endif
#ifndef BOARD_VBAT_R_BOTTOM_K
#define BOARD_VBAT_R_BOTTOM_K 1.0f
#endif
#ifndef BOARD_HAS_VBAT_DIVIDER
#define BOARD_HAS_VBAT_DIVIDER 0
#endif
#endif

#ifndef BOARD_INCOMPLETE_

// Strapping pins are refused unless the board header says why they are safe (default: not justified).
#ifndef BOARD_PIN_BUTTON_STRAP_OK
#define BOARD_PIN_BUTTON_STRAP_OK 0
#endif
#ifndef BOARD_PIN_STATUS_LED_STRAP_OK
#define BOARD_PIN_STATUS_LED_STRAP_OK 0
#endif
#ifndef BOARD_PIN_OUT_4_STRAP_OK
#define BOARD_PIN_OUT_4_STRAP_OK 0
#endif

// C++11 constexpr (one return statement each), so it works with any toolchain standard.
constexpr bool boardPinForbidden(int p) { return (p >= 35 && p <= 37) || p == 19 || p == 20; }
constexpr bool boardPinStrapping(int p) { return p == 0 || p == 3 || p == 45 || p == 46; }
// ESP32-S3 has GPIO 0-21 and 26-48 (22-25 do not exist).
constexpr bool boardPinExists(int p) { return (p >= 0 && p <= 21) || (p >= 26 && p <= 48); }

#define BOARD_CHECK_PIN_(pin, strapOk)                                                                    \
    static_assert(boardPinExists((int)(pin)), #pin ": no such GPIO on the ESP32-S3");                      \
    static_assert(!boardPinForbidden((int)(pin)), #pin ": GPIO 35-37 (N16R8 PSRAM) and 19/20 (USB) are not allowed"); \
    static_assert(!boardPinStrapping((int)(pin)) || (strapOk),                                             \
                  #pin ": strapping pin 0/3/45/46 needs a " #pin "_STRAP_OK macro with a justification")

BOARD_CHECK_PIN_(BOARD_PIN_CAN_RX, 0);
BOARD_CHECK_PIN_(BOARD_PIN_CAN_TX, 0);
BOARD_CHECK_PIN_(BOARD_PIN_CAN_SILENT, 0);
BOARD_CHECK_PIN_(BOARD_PIN_I2S_BCLK, 0);
BOARD_CHECK_PIN_(BOARD_PIN_I2S_LRC, 0);
BOARD_CHECK_PIN_(BOARD_PIN_I2S_DIN, 0);
BOARD_CHECK_PIN_(BOARD_PIN_AMP_SD, 0);
BOARD_CHECK_PIN_(BOARD_PIN_OUT_ACC, 0);
BOARD_CHECK_PIN_(BOARD_PIN_OUT_ILL, 0);
BOARD_CHECK_PIN_(BOARD_PIN_OUT_REV, 0);
BOARD_CHECK_PIN_(BOARD_PIN_OUT_4, BOARD_PIN_OUT_4_STRAP_OK);
BOARD_CHECK_PIN_(BOARD_PIN_RADIO_TX, 0);
BOARD_CHECK_PIN_(BOARD_PIN_RADIO_RX, 0);
BOARD_CHECK_PIN_(BOARD_PIN_I2C_SDA, 0);
BOARD_CHECK_PIN_(BOARD_PIN_I2C_SCL, 0);
BOARD_CHECK_PIN_(BOARD_PIN_VBAT_SENSE, 0);
BOARD_CHECK_PIN_(BOARD_PIN_STATUS_LED, BOARD_PIN_STATUS_LED_STRAP_OK);
BOARD_CHECK_PIN_(BOARD_PIN_BUTTON, BOARD_PIN_BUTTON_STRAP_OK);

// No two functions on one pin.
constexpr int kBoardPins_[] = {
    (int)(BOARD_PIN_CAN_RX),   (int)(BOARD_PIN_CAN_TX),   (int)(BOARD_PIN_CAN_SILENT), (int)(BOARD_PIN_I2S_BCLK),
    (int)(BOARD_PIN_I2S_LRC),  (int)(BOARD_PIN_I2S_DIN),  (int)(BOARD_PIN_AMP_SD),     (int)(BOARD_PIN_OUT_ACC),
    (int)(BOARD_PIN_OUT_ILL),  (int)(BOARD_PIN_OUT_REV),  (int)(BOARD_PIN_OUT_4),      (int)(BOARD_PIN_RADIO_TX),
    (int)(BOARD_PIN_RADIO_RX), (int)(BOARD_PIN_I2C_SDA),  (int)(BOARD_PIN_I2C_SCL),    (int)(BOARD_PIN_VBAT_SENSE),
    (int)(BOARD_PIN_STATUS_LED), (int)(BOARD_PIN_BUTTON)};
constexpr int kBoardPinCount_ = sizeof(kBoardPins_) / sizeof(kBoardPins_[0]);
constexpr bool boardPinUsedFrom_(int pin, int i) {
    return i >= kBoardPinCount_ ? false : (kBoardPins_[i] == pin || boardPinUsedFrom_(pin, i + 1));
}
constexpr bool boardPinsDistinct_(int i) {
    return i >= kBoardPinCount_ ? true : (!boardPinUsedFrom_(kBoardPins_[i], i + 1) && boardPinsDistinct_(i + 1));
}
static_assert(boardPinsDistinct_(0), "Board header: the same GPIO is assigned to two functions");

#endif  // BOARD_INCOMPLETE_
