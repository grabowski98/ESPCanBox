// State snapshot for the web panel: fields from canOk to lastKeyMs (src/net/diag_types.h), read from Decoder
// and from the drivers. Cut out of main.cpp without changing the fields. Compiled only in the program with network.
#pragma once
#include <stdint.h>

#include "config.h"   // NET_ENABLED
#include "parking_beeper.h"

// Call only when netStatusDue() returns true (every 200 ms), from the main loop.
#if NET_ENABLED
void appPublishStatus(uint32_t now, bool busAlive, const BeeperOutput& out);
#else
inline void appPublishStatus(uint32_t, bool, const BeeperOutput&) {}
#endif
