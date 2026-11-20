#include "xp_signals.h"

#include "xp_protocol.h"

namespace xp {

using vehicle::SettingId;

bool encodeSteering(const vehicle::SteeringAngle& s, RadioFrame& out) {
    out.cmd = BOX_STEERING_ANGLE;
    out.len = 2;
    out.data[0] = (uint8_t)(s.value & 0xFF);
    out.data[1] = (uint8_t)(s.value >> 8);
    return true;
}

static void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

bool encodeTrip(const vehicle::TripData& t, uint8_t page, RadioFrame& out) {
    if (page == vehicle::TRIP_NOW) {
        out.cmd = BOX_TRIP_NOW;
        out.len = 9;
        for (uint8_t i = 0; i < 3; i++) put16(out.data + 2 * i, t.now[i]);
        out.data[6] = out.data[7] = out.data[8] = 0;
        return true;
    }
    if (page != vehicle::TRIP_1 && page != vehicle::TRIP_2) return false;
    const vehicle::TripPage& p = page == vehicle::TRIP_1 ? t.trip1 : t.trip2;
    out.cmd = page == vehicle::TRIP_1 ? BOX_TRIP_1 : BOX_TRIP_2;
    out.len = 6;
    put16(out.data, p.c);
    out.data[2] = 0x00;
    out.data[3] = p.a;
    put16(out.data + 4, p.b);
    return true;
}

bool encodeOutsideTemp(const vehicle::OutsideTemp& t, RadioFrame& out) {
    const int halves = t.halfDegC;                    // temperature x 2
    int deg = halves >= 0 ? (halves + 1) / 2 : -((-halves) / 2);   // halves rounded up
    if (deg > 127) deg = 127;
    if (deg < -127) deg = -127;
    out.cmd = BOX_OUTSIDE_TEMP;
    out.len = 1;
    out.data[0] = deg >= 0 ? (uint8_t)deg : (uint8_t)(0x80 | (uint8_t)(-deg));
    return true;
}

bool encodeEcoMode(const vehicle::EcoMode& e, RadioFrame& out) {
    out.cmd = BOX_ECO_MODE;
    out.len = 1;
    out.data[0] = e.on ? 0x01 : 0x00;
    return true;
}

// AIR_* flags -> bits (windscreen, face, feet) at positions 7, 6, 5. Automatic: no bit.
static uint8_t airBits(uint8_t flags) {
    uint8_t b = 0;
    if (flags & vehicle::AIR_WINDSCREEN) b |= 0x80;
    if (flags & vehicle::AIR_FACE) b |= 0x40;
    if (flags & vehicle::AIR_FEET) b |= 0x20;
    return b;
}

bool encodeClimate(const vehicle::ClimateState& c, RadioFrame& out) {
    out.cmd = BOX_CLIMATE;
    out.len = 9;
    uint8_t b0 = 0x84;                                           // bits 7 and 2: constant throughout the recording
    if (c.acOn) b0 |= 0x40;
    if (c.recirculation) b0 |= 0x20;
    if ((c.airLeft & vehicle::AIR_AUTO) && c.fan == 0) b0 |= 0x08;   // AUTO
    if (c.rearDefrost) b0 |= 0x01;
    out.data[0] = b0;
    out.data[1] = (uint8_t)(airBits(c.airLeft) | (c.fan & 0x0F));
    out.data[2] = c.tempLeft;
    out.data[3] = c.tempRight;
    out.data[4] = c.frontDemist ? 0x80 : 0x00;
    out.data[5] = 0x81;
    out.data[6] = 0x40;
    out.data[7] = 0x00;
    out.data[8] = airBits(c.airRight);
    return true;
}

// One field of a settings byte: the bits under 'mask' get 'bits', the others stay.
static void field(uint8_t& b, uint8_t mask, uint8_t bits) { b = (uint8_t)((b & ~mask) | (bits & mask)); }

// One known item -> its bits in DATA[1..5] (d[0] = DATA[1]); unknown items leave the bits as they are.
static void applySettings(const vehicle::VehicleSettings& s, uint8_t* d) {
    for (uint8_t i = 0; i < (uint8_t)SettingId::COUNT; i++) {
        const vehicle::SettingItem& it = s.items[i];
        if (!it.known) continue;
        const uint8_t v = it.value;
        switch ((SettingId)i) {
            case SettingId::REAR_WIPER_REVERSE: field(d[0], 0x80, v ? 0x80 : 0); break;
            case SettingId::FATIGUE_DETECTION: field(d[0], 0x04, v ? 0x04 : 0); break;
            case SettingId::INSTRUMENT_COLOUR: field(d[0], 0x02, v ? 0x02 : 0); break;
            case SettingId::AMBIENT_LIGHT: field(d[1], 0xE0, (uint8_t)((v & 7) << 5)); break;
            case SettingId::PARKING_ASSIST: field(d[1], 0x10, v ? 0x10 : 0); break;
            case SettingId::PARKING_SENSORS: field(d[1], 0x08, v ? 0x08 : 0); break;     // "radar"
            case SettingId::FOLLOW_ME_HOME: field(d[2], 0xC0, (uint8_t)((v & 3) << 6)); break;
            case SettingId::WELCOME_LIGHTING: field(d[2], 0x30, (uint8_t)((v & 3) << 4)); break;
            case SettingId::DAYTIME_LIGHTS: field(d[2], 0x08, v ? 0x08 : 0); break;
            case SettingId::SOUND: field(d[2], 0x06, (uint8_t)((v & 3) << 1)); break;
            case SettingId::BLIND_SPOT: field(d[3], 0x80, v ? 0x80 : 0); break;
            case SettingId::STOP_START: field(d[3], 0x40, v ? 0x40 : 0); break;
            case SettingId::WELCOME_FUNCTION: field(d[3], 0x20, v ? 0x20 : 0); break;
            case SettingId::DOOR_UNLOCK_ALL: field(d[3], 0x10, v ? 0x10 : 0); break;
            case SettingId::LANGUAGE: field(d[3], 0x0F, v); break;
            case SettingId::FUEL_UNITS:
                // DATA[5] bits 7-6: 00 km/L, 01 L/100 km, 10 MPG. DATA[3] bit 0 (the radio does not read it):
                // 1 with L/100 km, 0 with km/L, unchanged with MPG (as the box).
                if (v == 0) {
                    field(d[2], 0x01, 0);
                    field(d[4], 0xC0, 0x00);
                } else if (v == 1) {
                    field(d[2], 0x01, 0x01);
                    field(d[4], 0xC0, 0x40);
                } else if (v == 2) {
                    field(d[4], 0xC0, 0x80);
                }
                break;
            default: break;     // UNASSIGNED_*: presence only
        }
    }
    // DATA[1] bits 4, 3 (boot, traction control), DATA[2] bits 2-0 and DATA[5] bits 5-0: never written
    // (always 0, as in the box; settings-map.md).
}

bool encodeCarState(const vehicle::DoorState& d, const vehicle::VehicleSettings& s, bool sideSwap, RadioFrame& io) {
    const uint8_t o = d.open;
    uint8_t b = (o & vehicle::DOOR_BOOT) ? 0x08 : 0;
    if (sideSwap) {
        if (o & vehicle::DOOR_DRIVER) b |= 0x80;
        if (o & vehicle::DOOR_PASSENGER) b |= 0x40;
        if (o & vehicle::DOOR_REAR_LEFT) b |= 0x20;
        if (o & vehicle::DOOR_REAR_RIGHT) b |= 0x10;
    } else {
        if (o & vehicle::DOOR_PASSENGER) b |= 0x80;
        if (o & vehicle::DOOR_DRIVER) b |= 0x40;
        if (o & vehicle::DOOR_REAR_RIGHT) b |= 0x20;
        if (o & vehicle::DOOR_REAR_LEFT) b |= 0x10;
    }
    io.cmd = BOX_CAR_STATE;
    io.len = 6;
    io.data[0] = (uint8_t)(b | 0x04);                 // bit 2: set with ignition on in the recording
    if (s.valid) applySettings(s, io.data + 1);
    return true;
}

bool encodeSettingsPresent(const vehicle::VehicleSettings& s, RadioFrame& out) {
    struct Row {
        SettingId id;
        uint8_t byte, bit;
    };
    static const Row ROWS[] = {
        {SettingId::UNASSIGNED_1, 0, 0x80},       {SettingId::DAYTIME_LIGHTS, 0, 0x40},
        {SettingId::AMBIENT_LIGHT, 0, 0x20},      {SettingId::WELCOME_LIGHTING, 0, 0x10},
        {SettingId::FOLLOW_ME_HOME, 0, 0x08},     {SettingId::REAR_WIPER_REVERSE, 0, 0x04},
        {SettingId::UNASSIGNED_2, 0, 0x02},       {SettingId::WELCOME_FUNCTION, 0, 0x01},
        {SettingId::DOOR_UNLOCK_ALL, 1, 0x80},    {SettingId::UNASSIGNED_3, 1, 0x40},
    };
    out.cmd = BOX_FEATURES;
    out.len = 2;
    out.data[0] = out.data[1] = 0;
    for (const Row& r : ROWS) {
        const vehicle::SettingItem& it = s.at(r.id);
        if (it.presentKnown && it.present) out.data[r.byte] |= r.bit;
    }
    return true;
}

}  // namespace xp
