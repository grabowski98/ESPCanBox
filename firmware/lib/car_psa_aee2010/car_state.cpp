#include "car_state.h"

void CarModel::store(RawFrame& r, const CanFrame& f, uint32_t nowMs, bool fromBus) {
    uint8_t n = f.len > 8 ? 8 : f.len;       // the length from the frame is not trusted
    for (uint8_t i = 0; i < 8; i++) r.d[i] = i < n ? f.data[i] : 0;
    r.len = n;
    r.seen = true;
    r.ms = nowMs;
    r.pasted = !fromBus;
}

void CarModel::onBusTraffic(uint32_t nowMs, bool fromBus) {
    if (fromBus) {
        s_.haveAnyFrame = true;
        s_.msAnyFrame = nowMs;
    }
}

void CarModel::onCanFrame(const CanFrame& f, uint32_t nowMs, bool fromBus) {
    onBusTraffic(nowMs, fromBus);
    switch (f.id) {
        case psa::ID_BODY_STATE: {
            psa::BodyState b;
            if (!psa::decodeBodyState(f, b)) break;   // too short: state unchanged
            {
                // A new "ignition on" needs ignition OFF (or a sleeping bus) before it. Cranking does not count:
                // on engine start byte 1 goes 88 -> 90 -> 91 -> 92 -> 8A -> 8E (measured, centralka_can_20261119_150048.log
                // 749.7..751.4 s), and those in-between values must not restart the 3.5 s settle time.
                const bool onNow = b.ignition == vehicle::Ignition::ON || b.ignition == vehicle::Ignition::ENGINE;
                const bool gap = !s_.f0F6.seen || (uint32_t)(nowMs - s_.f0F6.ms) > IGNITION_GAP_MS;
                if (gap || b.ignition == vehicle::Ignition::OFF) ignitionWasOff_ = true;
                if (onNow && ignitionWasOff_) {
                    ignitionWasOff_ = false;
                    s_.ignitionOnSeen = true;
                    s_.msIgnitionOn = nowMs;
                }
            }
            // Reverse released (measured, can_20261111_151209.log: 227 byte 3 bit 0 returns ~40 ms after it).
            if (s_.body.reverseGear && !b.reverseGear) {
                s_.reverseOffSeen = true;
                s_.msReverseOff = nowMs;
            }
            s_.body = b;
            store(s_.f0F6, f, nowMs, fromBus);
            break;
        }
        case psa::ID_LIGHTING: store(s_.f036, f, nowMs, fromBus); break;
        case psa::ID_PERSONALIZATION: store(s_.f260, f, nowMs, fromBus); break;
        case psa::ID_OPTIONS: store(s_.f361, f, nowMs, fromBus); break;
        case psa::ID_BUTTON_LEDS: store(s_.f227, f, nowMs, fromBus); break;
        case psa::ID_PARKING_COMMAND: store(s_.f126, f, nowMs, fromBus); break;
        case psa::ID_STOP_START: store(s_.f3E1, f, nowMs, fromBus); break;
        case psa::ID_FRAME_217: store(s_.f217, f, nowMs, fromBus); break;
        // Doors: the same conditions under which psa::decodeDoors returns true (psa_signals.cpp);
        // agreement on a capture from the car is checked by test_host/car_state.cpp.
        case psa::ID_CAR_MESSAGE:
            if (f.len >= 4 && f.data[1] == 0xDE && doorLampOn_) s_.haveDoors = true;
            break;
        case psa::ID_INDICATORS:
            if (f.len >= 5) {
                doorLampOn_ = (f.data[4] & 0x40) != 0;
                if (!doorLampOn_) s_.haveDoors = true;
            }
            break;
        default: break;
    }
}
