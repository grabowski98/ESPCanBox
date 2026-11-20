// Host test: a foreign radio protocol gets the car data for the radio screen as car-neutral data.
//   radio_stub <synthetic example capture> <PSA AEE2010 capture>
// The "stub" protocol below is not a real radio: its encoders (RadioProtocol::encode...) print the neutral
// values they receive (vehicle_signals.h) and write a dummy frame. It knows no CAN identifier and no XP code.
// Two runs through the Decoder (lib/app), unchanged:
//   1. template profile lib/car_example (invented frames): doors and one trip page;
//   2. reference profile PSA AEE2010 on a recording from the car: every kind of car data.
// Only changes of the neutral values are printed (the steering angle and the trip pages only as counts),
// so the reference stays short. The test fails when a kind of data the profile gives never reaches the stub.
#include <cstdio>
#include <cstring>
#include <vector>

#include "profile_example.h"
#include "replay_frames.h"

namespace {

struct Counts {
    unsigned long steering, trip, temp, eco, climate, carState, present;
};
Counts g_n;
uint32_t g_ms = 0;   // time of the current replay step (for the printed lines)

// Last printed values (print on change only).
vehicle::OutsideTemp g_temp;
vehicle::EcoMode g_eco;
vehicle::ClimateState g_climate;
uint8_t g_doors = 0xFF;
bool g_settingsValid = false;
vehicle::SettingItem g_items[(uint8_t)vehicle::SettingId::COUNT];
vehicle::SettingItem g_present[(uint8_t)vehicle::SettingId::COUNT];
bool g_presentPrinted = false;

const char* const SETTING_NAMES[(uint8_t)vehicle::SettingId::COUNT] = {
    "parking_assist", "rear_wiper_reverse", "ambient_light", "parking_sensors", "follow_me_home",
    "welcome_lighting", "daytime_lights", "sound", "welcome_function", "door_unlock_all", "language",
    "blind_spot", "stop_start", "fuel_units", "fatigue_detection", "instrument_colour", "unassigned_1",
    "unassigned_2", "unassigned_3",
};

void dummy(uint8_t cmd, RadioFrame& out) {
    out.cmd = cmd;
    out.len = 1;
    out.data[0] = 0;
}

bool stubSteering(const vehicle::SteeringAngle& s, RadioFrame& out) {
    g_n.steering++;
    (void)s;
    dummy(0x01, out);
    return true;
}

bool stubTrip(const vehicle::TripData& t, uint8_t page, RadioFrame& out) {
    g_n.trip++;
    if (g_n.trip == 1) {
        const vehicle::TripPage& p = page == vehicle::TRIP_2 ? t.trip2 : t.trip1;
        if (page == vehicle::TRIP_NOW)
            printf("%7lu trip now %04X %04X %04X (first call)\n", (unsigned long)g_ms, t.now[0], t.now[1], t.now[2]);
        else
            printf("%7lu trip %u a=%02X b=%04X c=%04X (first call)\n", (unsigned long)g_ms,
                   page == vehicle::TRIP_2 ? 2 : 1, p.a, p.b, p.c);
    }
    dummy(0x02, out);
    return true;
}

bool stubTemp(const vehicle::OutsideTemp& t, RadioFrame& out) {
    g_n.temp++;
    if (!g_temp.valid || t.halfDegC != g_temp.halfDegC)
        printf("%7lu outside temperature %d half degrees\n", (unsigned long)g_ms, t.halfDegC);
    g_temp = t;
    dummy(0x03, out);
    return true;
}

bool stubEco(const vehicle::EcoMode& e, RadioFrame& out) {
    g_n.eco++;
    if (!g_eco.valid || e.on != g_eco.on) printf("%7lu economy mode %u\n", (unsigned long)g_ms, e.on);
    g_eco = e;
    dummy(0x04, out);
    return true;
}

bool stubClimate(const vehicle::ClimateState& c, RadioFrame& out) {
    g_n.climate++;
    const vehicle::ClimateState& o = g_climate;
    if (!o.valid || c.acOn != o.acOn || c.recirculation != o.recirculation || c.frontDemist != o.frontDemist ||
        c.rearDefrost != o.rearDefrost || c.fan != o.fan || c.airLeft != o.airLeft || c.airRight != o.airRight ||
        c.tempLeft != o.tempLeft || c.tempRight != o.tempRight)
        printf("%7lu climate ac=%u recirc=%u demist=%u rear=%u fan=%u air=%X/%X temp=%u/%u half degrees\n",
               (unsigned long)g_ms, c.acOn, c.recirculation, c.frontDemist, c.rearDefrost, c.fan, c.airLeft, c.airRight,
               c.tempLeft, c.tempRight);
    g_climate = c;
    dummy(0x05, out);
    return true;
}

bool stubCarState(const vehicle::DoorState& d, const vehicle::VehicleSettings& s, bool sideSwap, RadioFrame& io) {
    g_n.carState++;
    if (d.open != g_doors) printf("%7lu doors open=0x%02X side swap=%u\n", (unsigned long)g_ms, d.open, sideSwap);
    g_doors = d.open;
    if (s.valid != g_settingsValid) printf("%7lu settings valid=%u\n", (unsigned long)g_ms, s.valid);
    g_settingsValid = s.valid;
    for (uint8_t i = 0; i < (uint8_t)vehicle::SettingId::COUNT; i++) {
        const vehicle::SettingItem& it = s.items[i];
        if (it.known != g_items[i].known || (it.known && it.value != g_items[i].value))
            printf("%7lu setting %s known=%u value=%u\n", (unsigned long)g_ms, SETTING_NAMES[i], it.known, it.value);
        g_items[i] = it;
    }
    dummy(0x06, io);
    return true;
}

bool stubPresent(const vehicle::VehicleSettings& s, RadioFrame& out) {
    g_n.present++;
    bool changed = !g_presentPrinted;
    for (uint8_t i = 0; i < (uint8_t)vehicle::SettingId::COUNT; i++)
        if (s.items[i].presentKnown != g_present[i].presentKnown || s.items[i].present != g_present[i].present)
            changed = true;
    if (changed) {
        printf("%7lu items the car has:", (unsigned long)g_ms);
        for (uint8_t i = 0; i < (uint8_t)vehicle::SettingId::COUNT; i++)
            if (s.items[i].presentKnown) printf(" %s=%u", SETTING_NAMES[i], s.items[i].present);
        printf("\n");
    }
    for (uint8_t i = 0; i < (uint8_t)vehicle::SettingId::COUNT; i++) g_present[i] = s.items[i];
    g_presentPrinted = true;
    dummy(0x07, out);
    return true;
}

const RadioProtocol kRadioStub = {
    "stub",
    9600,          // any non-zero speed: the Decoder hands frames to the port
    nullptr,       // no key codes
    stubSteering, stubTrip, stubTemp, stubEco, stubClimate, stubCarState, stubPresent,
};

void onStep(uint32_t ms, const DecoderOutputs& o, const uint8_t* buf, size_t n) {
    (void)o;
    (void)buf;
    (void)n;
    g_ms = ms + 1;   // the next step's frames are fed at ms + 1
}

void reset() {
    std::memset(&g_n, 0, sizeof(g_n));
    g_temp = vehicle::OutsideTemp();
    g_eco = vehicle::EcoMode();
    g_climate = vehicle::ClimateState();
    g_doors = 0xFF;
    g_settingsValid = false;
    for (uint8_t i = 0; i < (uint8_t)vehicle::SettingId::COUNT; i++) {
        g_items[i] = vehicle::SettingItem();
        g_present[i] = vehicle::SettingItem();
    }
    g_presentPrinted = false;
}

void printCounts(const char* name) {
    printf("%s: steering=%lu trip=%lu temp=%lu eco=%lu climate=%lu car_state=%lu present=%lu\n", name,
           g_n.steering, g_n.trip, g_n.temp, g_n.eco, g_n.climate, g_n.carState, g_n.present);
}

bool runOne(const char* path, const vehicle::CarProfile* car, const char* name) {
    std::vector<ReplayFrame> frames;
    if (!loadCapture(path, frames) || frames.empty()) {
        printf("cannot read %s\n", path);
        return false;
    }
    printf("--- %s + stub radio\n", name);
    reset();
    g_ms = frames.front().ms;
    static Decoder d;
    d = Decoder();
    DecoderConfig cfg;
    cfg.car = car;
    cfg.radio = &kRadioStub;
    d.begin(cfg, nullptr);
    replay(d, frames, 1000, onStep);
    printCounts(name);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: radio_stub <example capture> <psa capture>\n");
        return 2;
    }
    int bad = 0;
    if (!runOne(argv[1], &kCarExample, "example")) bad++;
    // The template gives doors and one trip page, nothing else.
    if (g_n.carState == 0 || g_n.trip == 0 || g_n.steering + g_n.temp + g_n.eco + g_n.climate + g_n.present != 0) {
        printf("FAIL example: wrong set of data reached the stub\n");
        bad++;
    }
    if (!runOne(argv[2], &kCarPsaAee2010, "psa_aee2010")) bad++;
    if (!g_n.steering || !g_n.trip || !g_n.temp || !g_n.eco || !g_n.climate || !g_n.carState || !g_n.present ||
        !g_settingsValid) {
        printf("FAIL psa_aee2010: a kind of car data never reached the stub\n");
        bad++;
    }
    printf("%s\n", bad ? "RULES BROKEN" : "the stub radio got the car data as neutral values from both profiles");
    return bad ? 1 : 0;
}
