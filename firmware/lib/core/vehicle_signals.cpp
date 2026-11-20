#include "vehicle_signals.h"

namespace vehicle {

static uint8_t minOf(uint8_t a, uint8_t b) { return a < b ? a : b; }

uint8_t ParkingState::nearestRear() const { return minOf(minOf(rearLeft, rearCenter), rearRight); }
uint8_t ParkingState::nearestFront() const { return minOf(minOf(frontLeft, frontCenter), frontRight); }
uint8_t ParkingState::nearestZone() const { return minOf(nearestRear(), nearestFront()); }

}  // namespace vehicle
