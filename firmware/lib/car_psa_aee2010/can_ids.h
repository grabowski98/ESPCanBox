// Identifiers of the CAN frames on the Peugeot 508 INFO-DIV bus (125 kbit/s) used by the decoder unit,
// and the periods at which the car sends them.
//
// Periods and lengths: measured, median interval in the captures test_data/captures/can_20261110_190421.log
// and test_data/captures/can_20261111_191045.log (ignition on); both captures give the same values.
// Names of frames 260, 361, 227, 3E1, 217 follow the PSA-RE description (not verified by measurement except
// for the bits listed in vehicles/peugeot-508-sw-2015/settings-map.md).
//
// The old names (xp::CAN_* in xp_car.h) remain as aliases of these constants.
#pragma once
#include <stdint.h>

namespace psa {

// --- identifiers ---                       length in the capture, who reads it here
const uint32_t ID_LIGHTING = 0x036;       // 8 B: economy mode, lights, panel knob
const uint32_t ID_WHEEL_KNOB = 0x0A2;     // 6 B: SRC button on the steering wheel
const uint32_t ID_PARKING_AID = 0x0E1;    // 8 B: parking sensors
const uint32_t ID_BODY_STATE = 0x0F6;     // 8 B: ignition state, reverse gear, outside temperature
const uint32_t ID_STEERING = 0x10B;       // 7 B: steering angle
const uint32_t ID_PARKING_COMMAND = 0x126;// 3 B: parking aid inhibition (byte 1: 18 = sensors off, 00 = on), BSI, 500 ms
const uint32_t ID_PANEL_KEYS = 0x122;     // 8 B: buttons of the panel above the climate control
const uint32_t ID_INDICATORS = 0x128;     // 8 B: warning lamps (door-open lamp)
const uint32_t ID_CAR_MESSAGE = 0x1A1;    // 8 B: car messages (which door)
const uint32_t ID_FRAME_217 = 0x217;      // 8 B: byte 8 bit 1 -> frame 38 (instrument colour); frame name unknown
const uint32_t ID_WHEEL_KEYS = 0x21F;     // 3 B: steering wheel buttons
const uint32_t ID_TRIP_NOW = 0x221;       // 7 B: trip computer, current page
const uint32_t ID_BUTTON_LEDS = 0x227;    // 5 B: button LEDs -> frame 38
const uint32_t ID_PERSONALIZATION = 0x260;// 7 B: vehicle settings state (BSI personalization)
const uint32_t ID_TRIP_2 = 0x261;         // 5 B: trip computer, trip 2
const uint32_t ID_TRIP_1 = 0x2A1;         // 5 B: trip computer, trip 1
const uint32_t ID_REAR_DEFROST = 0x2AD;   // 5 B: rear window heating
const uint32_t ID_CLIMATE = 0x350;        // 7 B: climate control
const uint32_t ID_OPTIONS = 0x361;        // 6 B: which settings the car has -> frame 3C
const uint32_t ID_STOP_START = 0x3E1;     // 5 B: Stop/Start state -> frame 38, since E6

// --- periods in milliseconds (measured as above) ---
const uint16_t PERIOD_LIGHTING_MS = 100;
const uint16_t PERIOD_WHEEL_KNOB_MS = 100;
const uint16_t PERIOD_PARKING_AID_MS = 100;
const uint16_t PERIOD_BODY_STATE_MS = 500;
const uint16_t PERIOD_STEERING_MS = 100;
const uint16_t PERIOD_PANEL_KEYS_MS = 200;
const uint16_t PERIOD_INDICATORS_MS = 200;
const uint16_t PERIOD_CAR_MESSAGE_MS = 200;
const uint16_t PERIOD_FRAME_217_MS = 100;
const uint16_t PERIOD_WHEEL_KEYS_MS = 100;
const uint16_t PERIOD_TRIP_NOW_MS = 1000;
const uint16_t PERIOD_BUTTON_LEDS_MS = 500;
const uint16_t PERIOD_PERSONALIZATION_MS = 500;
const uint16_t PERIOD_TRIP_2_MS = 1000;
const uint16_t PERIOD_TRIP_1_MS = 1000;
const uint16_t PERIOD_REAR_DEFROST_MS = 500;
const uint16_t PERIOD_CLIMATE_MS = 500;
const uint16_t PERIOD_OPTIONS_MS = 500;
const uint16_t PERIOD_STOP_START_MS = 1000;

}  // namespace psa
