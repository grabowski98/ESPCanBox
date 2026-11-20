// Sleep of the device with the car switched off.
// The device is powered from permanent +12 V, so without sleep the ESP32 would draw current
// from the battery during the whole parking time. After SLEEP_AFTER_MS of silence on the bus it enters deep
// sleep and wakes up on the first CAN frame.
//
// Note: during sleep the CAN module, the DC-DC converter and the power LED of the ESP32 module still draw current.
// The quiescent current of the whole device has to be measured with a meter (to be done on the hardware).
#pragma once
#include <stdint.h>

// Call on every loop. busAlive = a CAN frame arrived within the last second,
// accOn = the radio still has ACC supplied, holdAwake = the network asks to stay awake (netLoop in src/net/net.h:
// panel client, upload in progress, program waiting for confirmation); acts like busAlive.
// In the program without network holdAwake is always false and the behaviour is as before.
// Before sleep, when all conditions are met, it asks netPrepareSleep() (does not wait): as long as the Wi-Fi radio
// is running, sleep is postponed, at most NET_STOP_WAIT_MS from the first request.
void sleepPoll(bool busAlive, bool accOn, bool holdAwake, uint32_t nowMs);

// true if this program startup is a wake-up caused by traffic on the bus.
bool sleepWokeFromBus();
