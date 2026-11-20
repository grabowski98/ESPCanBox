#include "xp_parking.h"

namespace xp {

uint8_t parkingZoneToRadio(uint8_t canZone) {
    // Measurement: 0->0, 1->1, 2->1, 3->2, 4->2, 5->3, 6->3, 7->5.
    static const uint8_t MAP[8] = {0, 1, 1, 2, 2, 3, 3, 5};
    return MAP[canZone & 7];
}

Frame buildParkingFrame(const vehicle::ParkingState& pa, bool valid) {
    Frame f;
    f.cmd = BOX_PARKING;
    f.len = 7;
    bool rear = valid && pa.rearStatus == vehicle::PARK_ACTIVE;
    bool front = valid && pa.frontStatus == vehicle::PARK_ACTIVE;
    if (!rear && !front) {
        f.data[0] = 0x00;
        for (uint8_t i = 1; i < 7; i++) f.data[i] = 0xFF;
        return f;
    }
    // Measured: (rear waiting, front active) -> 01; (rear active, front active) -> 02.
    // The case "rear active, front not" did not occur in the recording; we treat it as 02.
    f.data[0] = rear ? 0x02 : 0x01;
    f.data[1] = parkingZoneToRadio(pa.rearLeft);
    f.data[2] = parkingZoneToRadio(pa.rearCenter);
    f.data[3] = parkingZoneToRadio(pa.rearRight);
    f.data[4] = parkingZoneToRadio(pa.frontLeft);
    f.data[5] = parkingZoneToRadio(pa.frontCenter);
    f.data[6] = parkingZoneToRadio(pa.frontRight);
    return f;
}

}  // namespace xp
