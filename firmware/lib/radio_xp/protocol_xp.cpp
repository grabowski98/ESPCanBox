#include "protocol_xp.h"

#include <string.h>

#include "radio_commands.h"
#include "xp_keys_custom.h"
#include "xp_parking.h"
#include "xp_protocol.h"
#include "xp_signals.h"

namespace xp {

namespace {

// Content of frame 71 exactly as sent by the Simple Soft box V2.29.101 (measurement).
// The radio shows the text "XP_PSA_2016_10_20 V2256" from it. We keep it unchanged
// so that the radio behaves exactly as it does with the box.
const uint8_t VERSION_DATA[9] = {0x0A, 0x05, 0x16, 0x10, 0x20, 0x56, 0x02, 0x19, 0x06};

// The parser of the frames from the radio (one per program: src/radio_uart.cpp uses it through kRadioXp).
Parser s_rxParser;

}  // namespace

bool encodePower(bool accOn, RadioFrame& out) {
    const uint8_t d[2] = {(uint8_t)(accOn ? 0x01 : 0x00), 0x0A};
    out = makeFrame(BOX_POWER, d, 2);
    return true;
}

bool encodeKey(const RadioKeyEvent& e, RadioFrame& out) {
    const uint8_t d[2] = {e.code, e.state};
    out = makeFrame(BOX_KEY, d, 2);
    return true;
}

bool encodeParkingFrame(const vehicle::ParkingState& p, bool valid, RadioFrame& out) {
    out = buildParkingFrame(p, valid);
    return true;
}

// The same bytes as xp::buildObstacleFlag (lib/app/xp_car.h, kept for the 0.6.0 reference glue); the golden
// stream test compares the two paths byte for byte.
bool encodeObstacle(bool obstacle, RadioFrame& out) {
    out = Frame();
    out.cmd = BOX_OBSTACLE;
    out.len = 1;
    out.data[0] = obstacle ? 0x80 : 0x00;
    return true;
}

bool encodeVersion(RadioFrame& out) {
    out = makeFrame(BOX_VERSION, VERSION_DATA, sizeof(VERSION_DATA));
    return true;
}

uint8_t decodeRadioFrame(const RadioFrame& f, RadioInfo& info) {
    switch (f.cmd) {
        case RADIO_TIME:
            if (f.len >= 5) {
                info.year = f.data[0];
                info.month = f.data[1];
                info.day = f.data[2];
                info.hour = f.data[3];
                info.minute = f.data[4];
                info.haveTime = true;
            }
            break;
        case RADIO_STATE:
            if (f.len >= 1) info.radioOn = f.data[0] != 0;
            break;
        case RADIO_VOLUME:
            if (f.len >= 1) {
                info.muted = (f.data[0] & 0x80) != 0;
                if (!info.muted) info.volume = f.data[0];
            }
            break;
        case RADIO_SOURCE:
            memcpy(info.source, f.data, f.len < 8 ? f.len : 8);
            break;
        case RADIO_REQUEST:
            // The radio asks for a frame: we only know the version (71). The session then also sends the sensor
            // state right away, because after startup the radio needs it for the chart, and every state frame.
            if (f.len >= 1 && f.data[0] == BOX_VERSION) return RADIO_RX_SEND_ALL;
            break;
        case RADIO_SETTING:
            if (f.len >= 2) {
                info.lastSettingId = f.data[0];
                info.lastSettingValue = f.data[1];
                info.settingCount++;
            }
            break;
        case RADIO_AIR:
            if (f.len >= 2) {
                info.lastAirId = f.data[0];
                info.lastAirValue = f.data[1];
                info.airCount++;
            }
            break;
        case RADIO_TUNER:
        case RADIO_VARIANT:
        case RADIO_TEXT:
            break;                         // known, unused for now
        default:
            info.unknownCount++;
            break;
    }
    return 0;
}

bool decodeCommand(const RadioFrame& f, ::RadioCmd& out) {
    out = parseRadioCmd(f);
    return out.kind != RadioCmdKind::NONE;
}

uint8_t keyCode(vehicle::KeyId key) {
    using vehicle::KeyId;
    switch (key) {
        case KeyId::VOLUME_UP: return 0x14;
        case KeyId::VOLUME_DOWN: return 0x15;
        case KeyId::MUTE: return 0x16;
        case KeyId::PHONE: return 0x30;
        case KeyId::LIST: return 0x50;
        case KeyId::WHEEL_KNOB_PRESS: return 0x07;
        case KeyId::SOURCE: return 0x21;
        case KeyId::TRIP: return 0x23;
        case KeyId::NAV: return 0x9E;
        case KeyId::MEDIA: return 0xA0;
        case KeyId::SCROLL_LEFT: return 0x97;
        case KeyId::ZOOM_IN: return 0x9C;
        case KeyId::SCROLL_RIGHT: return 0x98;
        case KeyId::TEL: return 0x9F;
        case KeyId::SETUP: return 0x99;
        case KeyId::DRIVE: return 0x9B;
        case KeyId::MOON: return 0xA4;
        case KeyId::ZOOM_OUT: return 0x9D;
        case KeyId::MIC_MUTE: return 0xA1;
        case KeyId::WEB: return 0xA3;
        case KeyId::PANEL_EXTRA: return 0x22;
        case KeyId::PANEL_KNOB_PRESS: return 0x16;   // the box sends the same code as MUTE
        case KeyId::NEXT: return 0x17;
        case KeyId::PREV: return 0x18;
        case KeyId::PANEL_KNOB_UP: return 0x81;
        case KeyId::PANEL_KNOB_DOWN: return 0x82;
        case KeyId::NONE:
        case KeyId::COUNT: break;
    }
    return 0;
}

// Protocol tables. Moon (A4, function 69): the radio blanks the screen on "pressed" and returns on "released"
// (observation in the car 2026-11-16), so the key becomes a toggle (xp_keys_custom.h).
const uint8_t LATCH_KEYS[1] = {KEY_PANEL_MOON};
// The steering angle changes on the bus every dozen or so ms; a few times per second is enough for the radio
// (the box sends on average every 0.6 s).
const RadioRateLimit RATE_LIMITS[1] = {{BOX_STEERING_ANGLE, 150}};

}  // namespace xp

const RadioProtocol kRadioXp = {
    "xp",
    xp::UART_BAUD,     // 38400 8N1
    xp::keyCode,
    // car data for the radio screen (xp_signals.h)
    xp::encodeSteering,          // 29
    xp::encodeTrip,              // 33, 34, 35
    xp::encodeOutsideTemp,       // 36
    xp::encodeEcoMode,           // 44
    xp::encodeClimate,           // 21
    xp::encodeCarState,          // 38
    xp::encodeSettingsPresent,   // 3C
    // line
    xp::encode,                  // 2E CMD LEN DATA CHK
    &xp::s_rxParser,
    true, xp::ACK_FROM_BOX,      // F3 after every valid radio frame, like the box
    10,                          // 10 ms between frames (the box: about 20 ms at startup)
    // session frames
    xp::encodePower,             // EA
    xp::encodeKey,               // 20
    xp::encodeParkingFrame,      // 32
    1000,                        // the box repeats frame 32 about every 1 s while the sensors are active
    xp::encodeObstacle,          // 3E
    xp::encodeVersion,           // 71
    // from the radio
    xp::decodeRadioFrame,
    xp::decodeCommand,
    // tables
    xp::LATCH_KEYS, 1,
    xp::RATE_LIMITS, 1,
    0,                           // every XP state frame has its own number
    true,                        // commands 80 / 8A / 84 use the numbers of the reference table (vset)
};
