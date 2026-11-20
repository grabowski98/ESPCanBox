#include "xp_car.h"

#include "psa_frames.h"
#include "psa_signals.h"
#include "xp_signals.h"

namespace xp {

bool buildSteering(const CanFrame& f, Frame& out) {
    vehicle::SteeringAngle s;
    return psa::decodeSteering(f, s) && encodeSteering(s, out);
}

bool buildTrip(const CanFrame& f, Frame& out) {
    vehicle::TripData t;
    const uint8_t page = psa::decodeTrip(f, t);
    return page != 0 && encodeTrip(t, page, out);
}

bool buildOutsideTemp(const CanFrame& f, Frame& out) {
    vehicle::OutsideTemp t;
    return psa::decodeOutsideTemp(f, t) && encodeOutsideTemp(t, out);
}

bool buildEcoMode(const CanFrame& f, Frame& out) {
    vehicle::EcoMode e;
    return psa::decodeEcoMode(f, e) && encodeEcoMode(e, out);
}

bool ClimateEncoder::onCanFrame(const CanFrame& f, Frame& out) {
    return psa::decodeClimate(f, state_) && encodeClimate(state_, out);
}

bool DoorEncoder::onCanFrame(const CanFrame& f, Frame& out) {
    if (!psa::decodeDoors(f, state_)) return false;
    build(out);
    return true;
}

void DoorEncoder::build(Frame& out) const {
    // Settings bytes from setSettings(); the neutral settings stay "not valid", so the encoder keeps them.
    for (uint8_t i = 0; i < 5; i++) out.data[1 + i] = settings_[i];
    vehicle::VehicleSettings none;
    encodeCarState(state_, none, swap_, out);
}

void DoorEncoder::setSettings(const uint8_t d[5]) {
    for (uint8_t i = 0; i < 5; i++) settings_[i] = d[i];
}

Frame buildObstacleFlag(bool obstacle) {
    Frame f;
    f.cmd = BOX_OBSTACLE;
    f.len = 1;
    f.data[0] = obstacle ? 0x80 : 0x00;
    return f;
}

// The rule lives in the car profile (psa::decodeLightsOn, psa_frames.h); same code.
bool decodeLightsOn(const CanFrame& f, bool& lightsOn) { return psa::decodeLightsOn(f, lightsOn); }

}  // namespace xp
