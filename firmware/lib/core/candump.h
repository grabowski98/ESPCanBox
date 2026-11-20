// Reads a frame from text in candump format, the same as in the sniffer captures:
//   (123.456789) can0 0E1#B0F010FCFC7EC200
// or just "0E1#B0F010FCFC7EC200". Used for tests without the car: in the USB console
// and in the test_host program.
#pragma once
#include "can_frame.h"

// Returns true if the line contains a frame. timeSec receives the time from the parentheses
// (or -1 when there is none).
bool parseCandumpLine(const char* line, CanFrame& out, double* timeSec = nullptr);
