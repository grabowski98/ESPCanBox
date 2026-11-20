// Console commands (USB and web panel): stat, radio, ver, vset, vol, test, key, map, doorswap, dump, rxlog,
// keys, help, cantx (CAN transmit state; in the program with transmission also cantx on|off|reset,
// vset on|off NN, set NN VV) and a CAN frame pasted by hand. Old (Polish) names kept as aliases:
// ust = vset, wifi haslo = wifi showpass, wifi nowehaslo = wifi newpass.
// Cut out of main.cpp without changing the line contents.
// All functions are called only by the main loop.
#pragma once
#include <stdint.h>

#include "can_tx_arm.h"   // cantx::Source
#include "config.h"       // NET_ENABLED

// Reads characters from the USB port and executes complete lines. Does not wait.
void cmdPollUsb(uint32_t now);

// Settings saves (volume, button table) deferred because the car needed the device (reverse,
// sensors): executes at most one per loop pass, once that state ends. Does not wait.
void cmdPollDeferredSaves(uint32_t now);

// Whether a settings save to flash memory is pending (deferred while reversing / sensors active). The CAN
// transmit gate refuses during that time (TxConditions::nvsPending).
bool cmdSavesPending();

// One command. 'src' = where the line came from: USB console, web panel from a client on the device's own
// network, or the panel via the home network (lib/core/can_tx_arm.h). Transmit arming and the item
// mask: only USB and the panel on the own network. 'line' zero terminated; the function may modify it.
void cmdHandleLine(char* line, uint32_t now, cantx::Source src);

// Requests from the web panel (console lines, button remap table). Does not wait.
// In the program without network an empty inline function: the request queue does not exist anyway.
#if NET_ENABLED
void cmdPollNetRequests(uint32_t now);
#else
inline void cmdPollNetRequests(uint32_t) {}
#endif
