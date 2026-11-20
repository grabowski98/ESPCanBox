// Shared program state: what main.cpp shares with the console (console_cmds.cpp) and with the state snapshot
// for the panel (app_status.cpp). Declarations only; the objects live in main.cpp.
// Everything is touched only by the main loop (no other tasks), so no locks.
#pragma once
#include <stdint.h>

#include "can_frame.h"
#include "decoder.h"
#include "loop_policy.h"

struct AppState {
    bool canOk = false, audioOk = false, radioOk = false;   // driver initialisation results from setup()
    bool dumpCan = false;          // printing of raw CAN frames (sniffer log format), command "dump"
    Deadline test;                 // test beeping (command "test"): a deadline with an "active" flag
    // Transmitting on CAN (only the program with CAN_TX_MODE = CAN_TX_SETTINGS; elsewhere always false):
    bool txArmed = false;          // this startup is armed (controller NORMAL, gate ready)
    bool txArmNext = false;        // NVS holds a valid arming: the next startup will be armed
};

extern AppState app;               // main.cpp
// All logic (buttons, sensors, radio power, frames for the radio): lib/app/decoder.h.
extern Decoder decoder;            // main.cpp

// One CAN frame: preview in the panel, dump to USB ("dump"), then the logic. main.cpp.
// fromBus = frame from the bus or pasted on USB (true); false = pasted from the web panel.
void handleFrame(const CanFrame& f, uint32_t now, bool fromBus);
