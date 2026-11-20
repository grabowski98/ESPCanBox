#include "psa_signals.h"

#include "can_ids.h"
#include "vehicle_settings.h"   // vset::radioLanguage

namespace psa {

using vehicle::SettingId;

bool decodeSteering(const CanFrame& f, vehicle::SteeringAngle& out) {
    if (f.id != ID_STEERING || f.len < 2) return false;
    out.valid = true;
    out.value = (uint16_t)((f.data[0] << 8) | f.data[1]);
    return true;
}

uint8_t decodeTrip(const CanFrame& f, vehicle::TripData& io) {
    if (f.id == ID_TRIP_NOW && f.len >= 7) {
        io.nowValid = true;
        for (uint8_t i = 0; i < 3; i++) io.now[i] = (uint16_t)((f.data[1 + 2 * i] << 8) | f.data[2 + 2 * i]);
        return vehicle::TRIP_NOW;
    }
    if ((f.id == ID_TRIP_1 || f.id == ID_TRIP_2) && f.len >= 5) {
        vehicle::TripPage& p = f.id == ID_TRIP_1 ? io.trip1 : io.trip2;
        p.valid = true;
        p.a = f.data[0];
        p.b = (uint16_t)((f.data[1] << 8) | f.data[2]);
        p.c = (uint16_t)((f.data[3] << 8) | f.data[4]);
        return f.id == ID_TRIP_1 ? vehicle::TRIP_1 : vehicle::TRIP_2;
    }
    return 0;
}

bool decodeOutsideTemp(const CanFrame& f, vehicle::OutsideTemp& out) {
    if (f.id != ID_BODY_STATE || f.len < 7) return false;
    out.valid = true;
    out.halfDegC = (int16_t)((int)f.data[6] - 80);   // (byte / 2 - 40) degrees = byte - 80 half degrees
    return true;
}

bool decodeEcoMode(const CanFrame& f, vehicle::EcoMode& out) {
    if (f.id != ID_LIGHTING || f.len < 3) return false;
    out.valid = true;
    out.on = (f.data[2] & 0x80) != 0;
    return true;
}

// Air distribution code of frame 350 -> vehicle::AIR_* flags (codes from the recording; others: no flag).
static uint8_t airFlags(uint8_t code) {
    switch (code) {
        case 8: return vehicle::AIR_WINDSCREEN | vehicle::AIR_FACE | vehicle::AIR_FEET;
        case 5: return vehicle::AIR_FACE | vehicle::AIR_FEET;
        case 3: return vehicle::AIR_FACE;
        case 0: return vehicle::AIR_AUTO;
        default: return 0;
    }
}

bool decodeClimate(const CanFrame& f, vehicle::ClimateState& io) {
    if (f.id == ID_CLIMATE && f.len >= 7) {
        const uint8_t* d = f.data;
        io.valid = true;
        io.acOn = !(d[0] & 0x08);
        io.frontDemist = (d[0] & 0x10) != 0;
        io.tempLeft = d[3] ? (uint8_t)(d[3] + 0x23) : 0;
        io.tempRight = d[4] ? (uint8_t)(d[4] + 0x23) : 0;
        io.fan = d[5] & 0x0F;
        io.recirculation = (d[5] >> 4) == 3;
        io.airLeft = airFlags(d[6] >> 4);
        io.airRight = airFlags(d[6] & 0x0F);
    } else if (f.id == ID_REAR_DEFROST && f.len >= 1) {
        // In frame 350 this state disappears after a moment, while the box holds it until the heating is
        // switched off: frame 2AD is the source.
        io.rearDefrost = (f.data[0] & 0x20) != 0;
    } else {
        return false;
    }
    return io.valid;
}

bool decodeDoors(const CanFrame& f, vehicle::DoorState& io) {
    const uint8_t DOORS = vehicle::DOOR_PASSENGER | vehicle::DOOR_DRIVER | vehicle::DOOR_REAR_RIGHT |
                          vehicle::DOOR_REAR_LEFT | vehicle::DOOR_BOOT;   // = F8, byte 4 of the 1A1 message
    if (f.id == ID_CAR_MESSAGE && f.len >= 4) {
        if (f.data[1] != 0xDE) return false;      // a different message or no message
        if (!io.anyOpen) return false;            // message just after the lamp went out: ignored
        io.open = f.data[3] & DOORS;
    } else if (f.id == ID_INDICATORS && f.len >= 5) {
        io.anyOpen = (f.data[4] & 0x40) != 0;
        if (io.anyOpen) return false;             // lamp lit: the last indication stays
        io.open = 0;                              // lamp out: everything closed
    } else {
        return false;
    }
    io.valid = true;
    return true;
}

namespace {

void set(vehicle::VehicleSettings& s, SettingId id, uint8_t value) {
    vehicle::SettingItem& it = s.at(id);
    it.value = value;
    it.known = true;
}

void unknown(vehicle::VehicleSettings& s, SettingId id) { s.at(id).known = false; }

void present(vehicle::VehicleSettings& s, SettingId id, bool has) {
    vehicle::SettingItem& it = s.at(id);
    it.present = has;
    it.presentKnown = true;
}

// Items read from frame 260 (CAN bytes counted from 1, notation "5.7" = byte 5 bit 7, as in settings-map.md).
const SettingId FROM_260[] = {
    SettingId::REAR_WIPER_REVERSE, SettingId::FATIGUE_DETECTION, SettingId::AMBIENT_LIGHT,
    SettingId::PARKING_ASSIST,     SettingId::FOLLOW_ME_HOME,    SettingId::WELCOME_LIGHTING,
    SettingId::DAYTIME_LIGHTS,     SettingId::SOUND,             SettingId::WELCOME_FUNCTION,
    SettingId::DOOR_UNLOCK_ALL,    SettingId::LANGUAGE,          SettingId::FUEL_UNITS,
};

void from260(const uint8_t* p, vehicle::VehicleSettings& s) {
    const uint8_t B1 = p[0], B2 = p[1], B3 = p[2], B4 = p[3], B5 = p[4], B7 = p[6];
    set(s, SettingId::REAR_WIPER_REVERSE, (B5 & 0x20) ? 1 : 0);    // 5.5
    set(s, SettingId::FATIGUE_DETECTION, (B7 & 0x10) ? 1 : 0);     // 7.4
    // ambient: when 3.0 = 1, level from 2.5-2.3 plus 1
    set(s, SettingId::AMBIENT_LIGHT, (B3 & 0x01) ? (uint8_t)(((B2 >> 3) & 7) + 1) : 0);
    set(s, SettingId::PARKING_ASSIST, (B5 & 0x80) ? 1 : 0);        // 5.7 (measured)
    // "follow me home" 4.7 + duration 4.6-4.5; welcome lighting 4.3 + duration 4.2-4.1
    set(s, SettingId::FOLLOW_ME_HOME, (B4 & 0x80) ? (uint8_t)(((B4 >> 5) & 3) + 1) : 0);
    set(s, SettingId::WELCOME_LIGHTING, (B4 & 0x08) ? (uint8_t)(((B4 >> 1) & 3) + 1) : 0);
    set(s, SettingId::DAYTIME_LIGHTS, (B3 & 0x02) ? 1 : 0);        // 3.1
    // sound 2.1-2.0, only when 2.2 = 1
    if (B2 & 0x04) set(s, SettingId::SOUND, (uint8_t)(B2 & 3));
    else unknown(s, SettingId::SOUND);
    set(s, SettingId::WELCOME_FUNCTION, (B3 & 0x08) ? 1 : 0);      // 3.3
    set(s, SettingId::DOOR_UNLOCK_ALL, (B3 & 0x40) ? 0 : 1);       // 3.6 inverted (measured)
    const uint8_t lang = vset::radioLanguage((uint8_t)((B1 >> 2) & 0x1F));   // 1.6-1.2 through the table
    if (lang != 0xFF) set(s, SettingId::LANGUAGE, lang);
    else unknown(s, SettingId::LANGUAGE);
    // fuel consumption units from 1.1, 1.0, 2.7; other combinations: no information
    const uint8_t unitLow = (uint8_t)(B1 & 3), unitHigh = (uint8_t)(B2 >> 7);
    if (unitLow == 1 && unitHigh == 0) set(s, SettingId::FUEL_UNITS, 0);          // km/L
    else if (unitLow == 0 && unitHigh == 0) set(s, SettingId::FUEL_UNITS, 1);     // L/100 km
    else if (unitLow == 3 && unitHigh == 1) set(s, SettingId::FUEL_UNITS, 2);     // MPG
    else unknown(s, SettingId::FUEL_UNITS);
}

}  // namespace

uint8_t decodeSettings(const CanFrame& f, vehicle::VehicleSettings& io) {
    const uint8_t n = f.len > 8 ? 8 : f.len;      // the length from the frame is not trusted
    switch (f.id) {
        case ID_PERSONALIZATION:
            if (n >= 7) {
                io.valid = true;
                from260(f.data, io);
            } else {
                for (SettingId id : FROM_260) unknown(io, id);
            }
            return vehicle::SETTINGS_VALUES;
        case ID_BUTTON_LEDS:
            if (n >= 3) {
                set(io, SettingId::PARKING_SENSORS, (f.data[0] & 0x40) ? 1 : 0);   // "radar": 227 1.6
                set(io, SettingId::BLIND_SPOT, (f.data[2] & 0x01) ? 1 : 0);        // 227 3.0 (measured)
            } else {
                unknown(io, SettingId::PARKING_SENSORS);
                unknown(io, SettingId::BLIND_SPOT);
            }
            return vehicle::SETTINGS_VALUES;
        case ID_STOP_START: {
            // 3E1 byte 4 bits 7-6: 10 -> 1 (switched off with the button), 01 -> 0 (measured); other: no information
            const uint8_t v = n >= 4 ? (uint8_t)(f.data[3] & 0xC0) : 0;
            if (v == 0x80) set(io, SettingId::STOP_START, 1);
            else if (v == 0x40) set(io, SettingId::STOP_START, 0);
            else unknown(io, SettingId::STOP_START);
            return vehicle::SETTINGS_VALUES;
        }
        case ID_FRAME_217:
            // instrument colour from bit 8.1 (in the recordings byte 8 = FF in every frame)
            if (n >= 8) set(io, SettingId::INSTRUMENT_COLOUR, (f.data[7] & 0x02) ? 1 : 0);
            else unknown(io, SettingId::INSTRUMENT_COLOUR);
            return vehicle::SETTINGS_VALUES;
        case ID_OPTIONS: {
            if (n < 4) return 0;
            // 361: "the car has this item" bits (assignment: firmware analysis / needs owner decision).
            const uint8_t B1 = f.data[0], B2 = f.data[1], B3 = f.data[2], B4 = f.data[3];
            present(io, SettingId::UNASSIGNED_1, (B1 & 0x80) != 0);        // 1.7
            present(io, SettingId::DAYTIME_LIGHTS, (B1 & 0x01) != 0);      // 1.0
            present(io, SettingId::AMBIENT_LIGHT, (B1 & 0x04) != 0);       // 1.2
            present(io, SettingId::WELCOME_LIGHTING, (B1 & 0x20) != 0);    // 1.5
            present(io, SettingId::FOLLOW_ME_HOME, (B2 & 0x20) != 0);      // 2.5
            present(io, SettingId::REAR_WIPER_REVERSE, (B2 & 0x40) != 0);  // 2.6
            present(io, SettingId::UNASSIGNED_2, (B3 & 0x40) != 0);        // 3.6
            present(io, SettingId::WELCOME_FUNCTION, (B2 & 0x10) != 0);    // 2.4
            present(io, SettingId::DOOR_UNLOCK_ALL, (B2 & 0x02) != 0);     // 2.1
            present(io, SettingId::UNASSIGNED_3, (B4 & 0x10) != 0);        // 4.4
            return vehicle::SETTINGS_PRESENT;
        }
        default:
            return 0;
    }
}

}  // namespace psa
