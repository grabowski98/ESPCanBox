// Device configuration: operating modes (everything that can send something is OFF by default)
// and hardware constants. Pin numbers come from the board header boards/board_<name>.h, selected
// with -DBOARD=<NAME> in platformio.ini (-I boards). The board header must match its schematic.
#pragma once
#include <Arduino.h>

#include "project_identity.h"   // project name, network prefix, mDNS (include/, from project.json)

// ---------- Board selection ----------
// -DBOARD=PROTO_V1 -> boards/board_proto_v1.h (prototype v1, hardware/prototype-v1).
// -DBOARD=TEMPLATE -> boards/board_template.h (stops with #error: "fill in pins").
// -DBOARD=CUSTOM -DBOARD_CUSTOM_HEADER='"board_x.h"' -> your own header without editing this file.
#define BOARD_ID_PROTO_V1 1
#define BOARD_ID_TEMPLATE 2
#define BOARD_ID_CUSTOM 3
#define BOARD_CAT_(a, b) a##b
#define BOARD_CAT(a, b) BOARD_CAT_(a, b)
#ifndef BOARD
#error "BOARD is not set: build with -DBOARD=PROTO_V1 (platformio.ini) or another boards/board_*.h"
#elif BOARD_CAT(BOARD_ID_, BOARD) == BOARD_ID_PROTO_V1
#include "board_proto_v1.h"
#elif BOARD_CAT(BOARD_ID_, BOARD) == BOARD_ID_TEMPLATE
#include "board_template.h"
#elif BOARD_CAT(BOARD_ID_, BOARD) == BOARD_ID_CUSTOM
#ifndef BOARD_CUSTOM_HEADER
#error "BOARD=CUSTOM needs -DBOARD_CUSTOM_HEADER='\"board_<name>.h\"'"
#endif
#include BOARD_CUSTOM_HEADER
#else
#error "Unknown BOARD value: use PROTO_V1, TEMPLATE or CUSTOM (see src/config.h)"
#endif
#include "board_common.h"         // pin rules (no 35-37, 19/20; strapping pins only with a reason)

// ---------- CAN (transceiver pins: board header) ----------
const gpio_num_t PIN_CAN_RX = BOARD_PIN_CAN_RX;
const gpio_num_t PIN_CAN_TX = BOARD_PIN_CAN_TX;
// Transceiver silent pin: HIGH = transmitter off. In the five environments without transmission, and in the
// transmitting program as long as it is not armed, it is permanently high and TWAI is in listen-only mode.
// The LOW level is set only by src/can_tx/can_tx_port.cpp (environment decoder_tx_wifi), for one frame.
// Hardware details (internal pull-down of the TJA1051 S pin, external pull-up): boards/board_proto_v1.h.
const gpio_num_t PIN_CAN_SILENT = BOARD_PIN_CAN_SILENT;
// Bus speed: from the car profile (CarProfile::canBitrate, below); PSA AEE2010 INFO-DIV = 125 kbit/s
// (measured 10.11.2026). src/can_bus.cpp does not start the controller on a speed it does not know.

// ---------- Car profile and radio protocol ----------
// -DCAR_PROFILE=PSA_AEE2010 (default) -> lib/car_psa_aee2010 (Peugeot 508 SW 2015, reference)
// -DCAR_PROFILE=GENERIC              -> lib/car_generic (unknown car: listen only, no keys, no 12 V outputs)
// -DCAR_PROFILE=CUSTOM -DCAR_PROFILE_CUSTOM_HEADER='"profile_<name>.h"' -DCAR_PROFILE_CUSTOM_OBJECT=kCar<Name>
//                                    -> your own profile in lib/car_<name> without editing this file
//                                       (also add lib_deps = car_<name>: the library finder does not follow an
//                                       include through a macro; platformio_override.sample.ini). LISTEN ONLY:
//                                       a program with transmission does not build with CUSTOM (below).
// (lib/car_example is a template compiled only by the host tests; it cannot be selected here.)
// -DRADIO_PROTOCOL=XP (default)       -> lib/radio_xp (Simple Soft box protocol, UART 38400 8N1)
// -DRADIO_PROTOCOL=NONE               -> lib/radio_none (no UART, nothing sent to a radio)
// -DRADIO_PROTOCOL=CUSTOM -DRADIO_PROTOCOL_CUSTOM_HEADER='"protocol_<name>.h"' -DRADIO_PROTOCOL_CUSTOM_OBJECT=kRadio<Name>
//                                    -> your own protocol in lib/radio_<name> without editing this file, with its
//                                       own framing, parser and frames (template: lib/radio_example, host tests
//                                       only). Also add lib_deps = radio_<name> (as for CAR_PROFILE=CUSTOM).
//                                       LISTEN ONLY on CAN: a program with transmission does not build with
//                                       CUSTOM (below), because the radio commands it decodes would reach the
//                                       CAN transmit path.
// A named profile (the rows below) gives CAR_PROFILE_TX_ALLOW_COUNT as the SAME compile-time constant its
// profile object uses for txAllowCount (PSA AEE2010: psa::TX_ALLOW_COUNT in both), so the two cannot differ.
#define CAR_PROFILE_ID_PSA_AEE2010 1
#define CAR_PROFILE_ID_GENERIC 2
#define CAR_PROFILE_ID_EXAMPLE 3
#define CAR_PROFILE_ID_CUSTOM 4
#ifndef CAR_PROFILE
#define CAR_PROFILE PSA_AEE2010
#endif
#if BOARD_CAT(CAR_PROFILE_ID_, CAR_PROFILE) == CAR_PROFILE_ID_PSA_AEE2010
#include "profile_psa_aee2010.h"
#define CAR_PROFILE_SELECTED kCarPsaAee2010
#define CAR_PROFILE_TX_ALLOW_COUNT psa::TX_ALLOW_COUNT
#define CAR_PROFILE_IS_GENERIC 0
#elif BOARD_CAT(CAR_PROFILE_ID_, CAR_PROFILE) == CAR_PROFILE_ID_GENERIC
#include "profile_generic.h"
#define CAR_PROFILE_SELECTED kCarGeneric
#define CAR_PROFILE_TX_ALLOW_COUNT generic::TX_ALLOW_COUNT
#define CAR_PROFILE_IS_GENERIC 1
#elif BOARD_CAT(CAR_PROFILE_ID_, CAR_PROFILE) == CAR_PROFILE_ID_EXAMPLE
#error "CAR_PROFILE=EXAMPLE is a template for the host tests only (lib/car_example); copy it into your own profile"
#elif BOARD_CAT(CAR_PROFILE_ID_, CAR_PROFILE) == CAR_PROFILE_ID_CUSTOM
#if !defined(CAR_PROFILE_CUSTOM_HEADER) || !defined(CAR_PROFILE_CUSTOM_OBJECT)
#error "CAR_PROFILE=CUSTOM needs -DCAR_PROFILE_CUSTOM_HEADER='\"profile_<name>.h\"' and -DCAR_PROFILE_CUSTOM_OBJECT=kCar<Name>"
#endif
#include CAR_PROFILE_CUSTOM_HEADER
#define CAR_PROFILE_SELECTED CAR_PROFILE_CUSTOM_OBJECT
#define CAR_PROFILE_TX_ALLOW_COUNT 0      // not known at compile time; transmission refused below
#define CAR_PROFILE_IS_GENERIC 0
#define CAR_PROFILE_IS_CUSTOM 1
#else
#error "Unknown CAR_PROFILE value: use PSA_AEE2010, GENERIC or CUSTOM (see src/config.h)"
#endif
#ifndef CAR_PROFILE_IS_CUSTOM
#define CAR_PROFILE_IS_CUSTOM 0
#endif
#define RADIO_PROTOCOL_ID_XP 1
#define RADIO_PROTOCOL_ID_NONE 2
#define RADIO_PROTOCOL_ID_EXAMPLE 3
#define RADIO_PROTOCOL_ID_CUSTOM 4
#ifndef RADIO_PROTOCOL
#define RADIO_PROTOCOL XP
#endif
#if BOARD_CAT(RADIO_PROTOCOL_ID_, RADIO_PROTOCOL) == RADIO_PROTOCOL_ID_XP
#include "protocol_xp.h"
#define RADIO_PROTOCOL_SELECTED kRadioXp
#define RADIO_PROTOCOL_IS_XP 1
#elif BOARD_CAT(RADIO_PROTOCOL_ID_, RADIO_PROTOCOL) == RADIO_PROTOCOL_ID_NONE
#include "protocol_none.h"
#define RADIO_PROTOCOL_SELECTED kRadioNone
#elif BOARD_CAT(RADIO_PROTOCOL_ID_, RADIO_PROTOCOL) == RADIO_PROTOCOL_ID_EXAMPLE
#error "RADIO_PROTOCOL=EXAMPLE is a template for the host tests only (lib/radio_example); copy it into your own protocol"
#elif BOARD_CAT(RADIO_PROTOCOL_ID_, RADIO_PROTOCOL) == RADIO_PROTOCOL_ID_CUSTOM
#if !defined(RADIO_PROTOCOL_CUSTOM_HEADER) || !defined(RADIO_PROTOCOL_CUSTOM_OBJECT)
#error "RADIO_PROTOCOL=CUSTOM needs -DRADIO_PROTOCOL_CUSTOM_HEADER='\"protocol_<name>.h\"' and -DRADIO_PROTOCOL_CUSTOM_OBJECT=kRadio<Name>"
#endif
#include RADIO_PROTOCOL_CUSTOM_HEADER
#define RADIO_PROTOCOL_SELECTED RADIO_PROTOCOL_CUSTOM_OBJECT
#define RADIO_PROTOCOL_IS_CUSTOM 1
#else
#error "Unknown RADIO_PROTOCOL value: use XP, NONE or CUSTOM (see src/config.h)"
#endif
#ifndef RADIO_PROTOCOL_IS_XP
#define RADIO_PROTOCOL_IS_XP 0
#endif
#ifndef RADIO_PROTOCOL_IS_CUSTOM
#define RADIO_PROTOCOL_IS_CUSTOM 0
#endif

// ---------- Sound (MAX98357A module, I2S; pins: board header) ----------
const int PIN_I2S_BCLK = BOARD_PIN_I2S_BCLK;
const int PIN_I2S_LRC = BOARD_PIN_I2S_LRC;
const int PIN_I2S_DIN = BOARD_PIN_I2S_DIN;
// Amplifier SD: low = amplifier off (pull-down on the board: silence before the program starts).
const int PIN_AMP_SD = BOARD_PIN_AMP_SD;
const int AUDIO_SAMPLE_RATE = 22050;        // Hz
const int AUDIO_VOLUME_DEFAULT = 40;        // percent of full scale
// Upper volume limit. Speaker from the Simple Soft kit: 8 Ω / 0.5 W, i.e. 2.0 V RMS.
// Measurement 16.11.2026 (AC meter on the speaker terminals, powered from USB): continuous tone at 50 %
// gives 0.753 V = 0.07 W. Hence the amplifier full scale is approx. 3.0 V (1.2 W).
// At 80 %: continuous tone approx. 1.2 V = 0.18 W; beep peak approx. 2.2 V (0.6 W for a few ms).
const int AUDIO_VOLUME_MAX = 80;
// Lowest volume that is stored in the settings (command "vol N", slider in the panel).
// A lower one works only until restart, and at startup the stored value is raised to this limit:
// in decoder mode the device speaker is the only parking sensor beeping (factory beeping
// from under the steering wheel is disabled in the BSI since 12.11.2026), so a single "vol 0" must not mute it permanently.
const int AUDIO_VOLUME_MIN_SAVED = 15;

// ---------- Operating mode ----------
// MODE_BESIDE_BOX: the device works BESIDE the factory CAN box. It only listens: to the car bus
//   and to the radio -> box line. Sends nothing to the radio, the 12 V outputs are off. Beeps from the speaker.
// MODE_DECODER: the device INSTEAD OF the box. Sends frames to the radio and drives the ACC / reverse outputs.
//   Enable only when the box is unplugged: two transmitters on one UART line and two ACC
//   sources means shorted outputs.
// In both modes the car CAN bus is ONLY listened to (exception: CAN_TX_MODE below).
#define MODE_BESIDE_BOX 0
#define MODE_DECODER 1
// Safe mode by default. Decoder mode is enabled by a separate environment in platformio.ini
// (decoder), never by a change here.
#ifndef WORK_MODE
#define WORK_MODE MODE_BESIDE_BOX
#endif

// ---------- Transmitting on the car CAN ----------
// CAN_TX_OFF: controller in listen-only mode, the program contains no transmit function (default).
// CAN_TX_SETTINGS: allowed to transmit only the frames of the car profile's transmit allow list
//   (CarProfile::txAllow, const; PSA AEE2010: frame 15B, personalisation change in the BSI; the trial frame
//   1A9 only with -DCAN_TX_EXPERIMENTAL=1, default 0), and only after arming with the "cantx on"
//   command (USB or panel on the device's own network) and a restart.
//   Arming is bound to the build string: every newly flashed program starts disarmed.
//   This is enabled only by environment decoder_tx_wifi in platformio.ini, never by a change here.
// Gate rules (frame list, limits, latch): lib/core/can_tx_gate.h. Hardware: src/can_tx/.
#define CAN_TX_OFF 0
#define CAN_TX_SETTINGS 1
#ifndef CAN_TX_MODE
#define CAN_TX_MODE CAN_TX_OFF
#endif
#if CAN_TX_MODE != CAN_TX_OFF && CAN_TX_MODE != CAN_TX_SETTINGS
#error "Unknown value of CAN_TX_MODE"
#endif
#if CAN_TX_MODE != CAN_TX_OFF && WORK_MODE != MODE_DECODER
#error "CAN transmit only in decoder mode"
#endif
// The generic profile is listen only: a program with transmission cannot be built with it.
// The example profile cannot be selected at all (#error above). A program with transmission does
// not build with a profile whose transmit allow list is empty either (count known at compile time).
#if CAN_TX_MODE != CAN_TX_OFF && CAR_PROFILE_IS_GENERIC
#error "CAN transmit is not allowed with CAR_PROFILE=GENERIC (listen-only profile)"
#endif
// A custom profile (CAR_PROFILE=CUSTOM) is listen only: its allow list is not visible to the compile-time
// checks here. Transmission with a new car needs a named profile row above, with measured frames.
// A custom radio protocol (RADIO_PROTOCOL=CUSTOM) is listen only as well: its decodeCommand would feed radio
// commands into the vehicle setting change, which is built for the XP setting numbers.
#if CAN_TX_MODE != CAN_TX_OFF && RADIO_PROTOCOL_IS_CUSTOM
#error "CAN transmit is not allowed with RADIO_PROTOCOL=CUSTOM (listen-only; the radio commands follow the XP numbering)"
#endif
#if CAN_TX_MODE != CAN_TX_OFF && CAR_PROFILE_IS_CUSTOM
#error "CAN transmit is not allowed with CAR_PROFILE=CUSTOM (listen-only; add a named profile in src/config.h)"
#endif
static_assert(CAN_TX_MODE == CAN_TX_OFF || CAR_PROFILE_TX_ALLOW_COUNT > 0,
              "CAN transmit needs a car profile with a non-empty transmit allow list");
static_assert(CAN_TX_MODE == CAN_TX_OFF || !CAR_PROFILE_IS_GENERIC, "CAN transmit is not allowed with the generic profile");
// Hardware timer of the transmitter window: this many microseconds after opening, SLNT returns high
// independently of the main loop (the gate closes the window after 20 ms; this is a margin for a hung loop).
const uint32_t CAN_TX_HW_WINDOW_US = 25000;

// ---------- Sleep ----------
// After this many milliseconds of silence on the bus (and no computer on USB) the device goes to sleep.
// After the key is removed the car keeps talking for approx. 100 s (measured 11.11.2026), then the bus goes quiet.
#define SLEEP_ENABLED 1
const uint32_t SLEEP_AFTER_MS = 30000;

// ---------- 12 V outputs to the radio (P-MOSFET switches, high level switches on) ----------
const int PIN_OUT_ACC = BOARD_PIN_OUT_ACC;  // radio ACC
const int PIN_OUT_ILL = BOARD_PIN_OUT_ILL;  // illumination: lights on according to frame 036
const int PIN_OUT_REV = BOARD_PIN_OUT_REV;  // reverse gear
const int PIN_OUT_4 = BOARD_PIN_OUT_4;      // spare

// ---------- UART to the radio: 38400 8N1, 3.3 V (pins and wiring notes: board header) ----------
const int PIN_RADIO_TX = BOARD_PIN_RADIO_TX;  // unassigned in MODE_BESIDE_BOX
const int PIN_RADIO_RX = BOARD_PIN_RADIO_RX;

// ---------- Other ----------
const int PIN_I2C_SDA = BOARD_PIN_I2C_SDA;
const int PIN_I2C_SCL = BOARD_PIN_I2C_SCL;
const int PIN_VBAT_SENSE = BOARD_PIN_VBAT_SENSE;  // divider from the 12 V rail
const float VBAT_DIVIDER = (BOARD_VBAT_R_TOP_K + BOARD_VBAT_R_BOTTOM_K) / BOARD_VBAT_R_BOTTOM_K;
// 0 = no divider on the board. The measurement shows the voltage behind the input diode; without 12 V
// (powered from USB) it shows noise; below NET_VBAT_VALID_MIN_V the console and the panel assume there is no 12 V.
#define HAS_VBAT_DIVIDER BOARD_HAS_VBAT_DIVIDER
const int PIN_STATUS_LED = BOARD_PIN_STATUS_LED;  // RGB LED (WS2812)
// Emergency button: shorts to ground.
const int PIN_BUTTON = BOARD_PIN_BUTTON;          // with internal pull-up (INPUT_PULLUP)

// ---------- Network: web service panel and firmware update over Wi-Fi ----------
// DISABLED by default. Network code gets into the program only in the platformio.ini environments that
// set -DNET_ENABLED=1 (listen_wifi, decoder_wifi); never through a change here.
// It changes nothing on the CAN bus: the network has no transmit function (transmission is decided by CAN_TX_MODE).
// WARNING: with network a different program can be flashed over Wi-Fi (images are not signed). A hardware
// guarantee of no transmission was given only by an open CAN transmitter jumper (JP1 on the prototype, JP_TX on
// the target board). Since 17.11.2026 JP1 on the prototype is closed and SLNT is wired to GPIO6:
// transmission on the car bus is decided only by the flashed program. Protection against mistakes:
// the canTx marker in the image and a separate consent on update (lib/core/fw_ident.h).
#ifndef NET_ENABLED
#define NET_ENABLED 0
#endif
// The device's own network (access point), WPA2. Name = prefix + 4 hex digits
// from the last two bytes of the chip MAC address, e.g. prefix + "A1B2".
// Prefix and mDNS name: include/project_identity.h (from project.json, tools/set_project_name.py).
#define NET_AP_SSID_PREFIX PROJECT_AP_SSID_PREFIX
// Network name: panel at http://<NET_HOSTNAME>.local (mDNS) or at the IP address below.
#define NET_HOSTNAME PROJECT_MDNS_HOST
// Device address in its own network, /24 mask. A subnet rarely seen on home routers,
// so that it does not overlap the home network the device may be connected to at the same time.
const uint8_t NET_AP_IP[4] = {10, 50, 8, 1};
// Channel of the own network. When the device is also connected to a home network, the chip has one radio
// and the own network moves to the router's channel.
const uint8_t NET_AP_CHANNEL = 1;
// How many devices at once on the own network: car radio, phone, laptop and one spare.
// Was 2 until 2.0.0-beta.1: in the car the radio could not join while two other devices were connected.
const uint8_t NET_AP_MAX_STATIONS = 4;
// Wi-Fi transmitter power in units of 0.25 dBm: 44 = 11 dBm (value WIFI_POWER_11dBm from WiFiGeneric.h;
// full power is 80-84). Low power is enough in the cabin; range to the home router: to be checked in the car.
const int8_t NET_TX_POWER_QDBM = 44;
// Staying awake for a panel client after the bus goes quiet is allowed only above this voltage.
// The threshold applies to the rail BEHIND diode D1, because that is what batteryVolts() measures: 11.8 V at the
// battery minus approx. 0.8 V across the diode. The diode drop is not measured exactly (0.7-0.9 V): to be confirmed with a meter.
const float NET_MIN_RAIL_V = 11.0f;
// Below this value we assume 12 V is not supplied (bench work on USB): the measurement is skipped.
// Used in all environments (also "stat" in the program without network), hence outside #if.
const float NET_VBAT_VALID_MIN_V = 6.0f;
