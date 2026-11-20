#include "psa_frames.h"

namespace psa {

// ParkingAid::nearestRear / nearestFront / nearestZone: lib/core/vehicle_signals.cpp.

// Frame 0E1, examples from the car (step 17 of the capture of 10.11.2026):
//   idle:                B4 00 3F FC FC FC C2 00
//   obstacle in front:   B0 F0 10 FC FC 7E C2 00
//   continuous tone:     B0 F0 00 FC FC 1E C2 00
bool decodeParkingAid(const CanFrame& f, ParkingAid& out) {
    if (f.id != ID_PARKING_AID || f.len < 6) return false;
    const uint8_t* d = f.data;
    out.rearStatus = (d[0] >> 5) & 0x07;        // byte 1, bits 7-5
    out.frontStatus = (d[0] >> 2) & 0x07;       // byte 1, bits 4-2
    out.soundSides = (d[1] >> 6) & 0x03;        // byte 2, bits 7-6
    out.soundFront = (d[1] & 0x20) != 0;        // byte 2, bit 5
    out.soundRequest = (d[1] & 0x10) != 0;      // byte 2, bit 4
    out.beepDurationCode = (d[2] >> 6) & 0x03;  // byte 3, bits 7-6
    out.intervalRaw = d[2] & 0x3F;              // byte 3, bits 5-0
    out.rearLeft = (d[3] >> 5) & 0x07;          // byte 4, bits 7-5
    out.rearCenter = (d[3] >> 2) & 0x07;        // byte 4, bits 4-2
    out.rearRight = (d[4] >> 5) & 0x07;         // byte 5, bits 7-5
    out.frontLeft = (d[4] >> 2) & 0x07;         // byte 5, bits 4-2
    out.frontCenter = (d[5] >> 5) & 0x07;       // byte 6, bits 7-5
    out.frontRight = (d[5] >> 2) & 0x07;        // byte 6, bits 4-2
    out.displayRequest = (d[5] & 0x02) != 0;    // byte 6, bit 1
    return true;
}

// Byte 1 of frame 0F6 -> neutral ignition state. Measured values (test_data/captures/can_20261111_191045.log):
// 80 off; 88 on; 90, 91, 89, 8A cranking; 8E engine running; 86, 84 transition while switching off.
// 00 (never measured, the default before the first frame) counts as off, as in earlier versions.
vehicle::Ignition ignitionFromByte(uint8_t b) {
    if (b == 0x80 || b == 0x00) return vehicle::Ignition::OFF;
    if (b == 0x88) return vehicle::Ignition::ON;
    if (b == 0x8E) return vehicle::Ignition::ENGINE;
    return vehicle::Ignition::TRANSITION;
}

bool decodeBodyState(const CanFrame& f, BodyState& out) {
    if (f.id != ID_BODY_STATE || f.len < 8) return false;
    out.ignitionRaw = f.data[0];
    out.ignition = ignitionFromByte(f.data[0]);
    out.reverseGear = (f.data[7] & 0x80) != 0;  // byte 8, bit 7
    return true;
}

}  // namespace psa
