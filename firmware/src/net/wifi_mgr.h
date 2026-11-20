// Wi-Fi handling: the device's own network (access point, always while Wi-Fi is running), optional
// connection to a home network, the name espcanbox.local (mDNS), and starting and stopping the web server.
//
// This module is the only place in the program that calls the WiFi and ESPmDNS libraries, and the only one
// that starts and stops the web server (httpServerStart / httpServerStop). This keeps the order
// in one file: network -> mDNS -> server at start, server -> mDNS -> radio at stop.
//
// TASK "net_wifi" (change after the project review, 17.11.2026):
// WiFi library and server functions BLOCK: WiFi.softAP and WiFi.begin wait up to 1 s for the driver,
// WiFi.mode(WIFI_OFF) for the radio to stop, and httpServerStop() for the running request to finish,
// i.e. with a slow or malicious client even minutes. The main loop cannot wait for that
// (owner's priority: no CAN frame and no frame to the radio may be lost because of the network).
// Therefore ALL blocking calls are executed by one dedicated task:
//   - created in wifiMgrBegin() by xTaskCreateStaticPinnedToCore (static stack and task descriptor),
//     core 0, priority WIFI_TASK_PRIO (below the web server and the sound task);
//   - waits for a notification (ulTaskNotifyTake with timeout WIFI_TASK_TICK_MS) and then works towards
//     the state requested by the main loop, step by step, measuring the time of each step;
//   - does not write to USB (at most conLogf); "[wifi] ..." messages on USB are printed by the main loop
//     in wifiMgrPoll() when it notices a change of phase or of the event counter.
// The main loop calls here only functions that do NOT wait: they set volatile variables
// and wake the task (xTaskNotifyGive), or copy the state under a short critical section (portMUX, copy
// of approx. 90 bytes).
// When the task gets stuck (e.g. httpServerStop waits for a client), the main loop keeps running normally;
// deep sleep still happens after NET_STOP_WAIT_MS (see netPrepareSleep in net.h).
//
// Start-up steps are still held back while carBusy == true (reverse or active sensors):
// starting the radio loads core 0 and at the first start writes radio calibration data to flash memory
// (a flash write stops CAN reception). Stopping is NOT held back (it does not write flash).
//
// Wi-Fi event callbacks (WiFi.onEvent) run in the "arduino_events" task (4 KB stack, high
// priority) under the library lock. They may ONLY store a flag, a reason code and a time into volatile
// variables. They must not call WiFi.*, conPrintf, conLogf, diag* or settings*.
// The flags are read by the net_wifi task (actions) and by wifiMgrPoll (messages on USB).
// A time stored in another task may be newer than the main loop's 'nowMs': compute time differences
// as (int32_t)(nowMs - stamp), a negative result = 0.
//
// Order of the start-up steps (net_wifi task; the result of every library call is checked):
//   0. once, in wifiMgrBegin() (main loop, setup): WiFi.persistent(false), WiFi.setHostname(NET_HOSTNAME),
//      WiFi.setAutoReconnect(false), WiFi.onEvent(...); then the task is created;
//   1. only when there is no valid device password (settingsGetDevicePassword() == false):
//      task: WiFi.mode(WIFI_STA), so that the radio runs (esp_fill_random then gives true randomness),
//      then the "password needed" flag and waiting. Main loop (wifiMgrPoll, when !carBusy):
//      settingsGeneratePassword + settingsSetDevicePassword (settings may be written only from the main
//      loop), message "[wifi] device password generated; the command wifi showpass shows it" and the flag
//      "password ready" or "password failed". A result other than OK (e.g. STORE_FAILED) is a FAILURE
//      of step 1;
//   2. reading the password: settingsGetDevicePassword() must return true, and the password must satisfy
//      webauth::passwordAcceptable. Otherwise FAILURE: WiFi.softAP NEVER gets
//      an empty or short password (empty password = OPEN network).
//      Then WiFi.softAP(name, password, NET_AP_CHANNEL, 0, NET_AP_MAX_STATIONS) and RIGHT AFTER IT
//      WiFi.softAPConfig(NET_AP_IP, NET_AP_IP, 255.255.255.0). softAP() first enables the interface,
//      and only then writes the name and password; softAPConfig also enables the interface
//      by itself, so calling it first would extend the time spent with the default settings.
//      If either of the two functions fails: WiFi.mode(WIFI_OFF) immediately, in the same
//      step, regardless of carBusy. Network name: NET_AP_SSID_PREFIX + 4 hex digits
//      from bytes 4 and 5 of the MAC address from ESP.getEfuseMac() (byte 0 of the address is the lowest byte of that number;
//      WiFi.macAddress() in access-point-only mode returns zeros). On the bench, check
//      with a Wi-Fi scanner on a phone that no open "ESP_..." network appears briefly at start,
//      and that the DHCP addresses are from the NET_AP_IP subnet;
//   3. WiFi.setSleep(false) and WiFi.setTxPower((wifi_power_t)NET_TX_POWER_QDBM) (works only after start);
//      a failure of this step does not abort the start-up;
//   4. mode: without a saved home network only the access point remains (WiFi.mode(WIFI_AP), if
//      client mode was enabled in step 1); with a home network WiFi.begin(name, password), which adds client
//      mode. When engineRunning == true at the time of step 4, WiFi.begin is skipped (home network
//      only on staAttempt);
//   5. MDNS.begin(NET_HOSTNAME) and MDNS.addService("http", "tcp", HTTP_PORT); a failure does not
//      abort the start-up (the panel works at the IP address);
//   6. httpServerStart(); only now phase RUNNING.
// Failure of step 1, 2 or 6: full stop, phase FAILED, next attempt after the pause
// from the WIFI_RETRY_MS table (30 s, 2 min, 10 min). After WIFI_START_TRIES_MAX failed starts in a row
// the task stops trying (WifiStatus::gaveUp = true) until: the "wifi on" command on USB,
// wifiMgrReconfigure() or the next program start; the main loop prints once
// "[wifi] start failed 3 times, network off until restart". A successful start resets the counter.
//
// Stop: httpServerStop() -> MDNS.end() -> WiFi.mode(WIFI_OFF). Only WiFi.mode(WIFI_OFF)
// really turns off the radio driver; it also destroys the network interfaces, which is why the server and mDNS must
// be stopped earlier. httpServerStop() first sets httpStopRequested(), so long loops
// in the handlers (upload, discarding the body, the trace.log file) end at the next chunk.
//
// Connection to the home network: the library does not retry by itself (setAutoReconnect(false)), because its
// retrying has no pauses and breaks connectivity with the device's own network. A new attempt
// (WiFi.reconnect()) is made by the task only on the staAttempt pulse from NetPolicy (every 30 s, and with a
// client in the own network every 5 min; never with the engine running). In access point +
// client mode the own network moves to the router's channel: a phone connected to the device may briefly
// lose connectivity.
//
// Internal file of src/net; included only from files compiled with NET_ENABLED = 1.
// The header itself does not depend on Arduino. Implementation: src/net/wifi_mgr.cpp.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "diag_types.h"      // WifiStatus, WifiPhase

// The first start-up step no earlier than this after program start: the car logic must first
// receive the first frames and give the radio ACC (the box did this approx. 30 ms after ignition on).
const uint32_t WIFI_BOOT_DELAY_MS = 2000;
// Pauses before further attempts after a failed start (1st, 2nd, 3rd failure in a row).
const uint32_t WIFI_RETRY_MS[3] = {30000, 120000, 600000};
// After this many failed starts in a row the task stops trying (see the description above).
const uint8_t WIFI_START_TRIES_MAX = 3;
// How often the task refreshes: number of clients of the own network, signal strength, home network address, channel.
const uint32_t WIFI_STATUS_REFRESH_MS = 1000;
// A step longer than this: the main loop prints "[wifi] step <name> took <n> ms" (a measurement, not an error).
const uint16_t WIFI_STEP_WARN_MS = 300;
// net_wifi task: stack (bytes, static), priority, core, longest wait for a notification.
// 6 KB stack: the WiFi and ESPmDNS libraries build String objects internally and call IDF functions;
// headroom to be checked via uxTaskGetStackHighWaterMark (field sys.stackWifiFree).
const size_t WIFI_TASK_STACK = 6144;
const unsigned WIFI_TASK_PRIO = 2;
const int WIFI_TASK_CORE = 0;
const uint32_t WIFI_TASK_TICK_MS = 100;

// Call once in setup() (via netBegin), after settingsBegin(). Performs step 0 from the description above and creates
// the net_wifi task; does not turn on the radio. Does not block. false = registering the event handler
// or creating the task failed (Wi-Fi stays off permanently, the rest of the program runs).
bool wifiMgrBegin();

// Main loop, on every pass (from netLoop). Does NOT WAIT (microseconds).
//   wantOn        : the radio should run (NetDecision::wifiOn);
//   staAttempt    : pulse "try to connect to the home network now" (NetDecision::staAttempt);
//                   the task skips it when no home network is saved, a connection already exists,
//                   the phase is other than RUNNING, or carBusy;
//   carBusy       : the car needs the device; start-up and reconfiguration steps wait;
//   engineRunning : the engine is running; WiFi.begin / reconnect are skipped (the car is driving away from home, and every
//                   search for the home network broadcasts its name and breaks connectivity with the own network).
// Operation: copies the requested state into volatile variables and wakes the task when something changed;
// compares the phase and event counters with the previous call and prints changes via conPrintf
// ("[wifi] ...", never with the password), including a one-time message after the password is generated, after
// giving up attempts (gaveUp), and a warning about a step longer than WIFI_STEP_WARN_MS.
void wifiMgrPoll(bool wantOn, bool staAttempt, bool carBusy, bool engineRunning, uint32_t nowMs);

// The Wi-Fi settings in the settings module have changed (device password, home network): at the next
// opportunity stop the network and start it again with the new values. Also resets the failed-start counter
// and gaveUp. Only sets a flag and wakes the task (main loop, does not wait). When the radio is
// off, the new values will be used at the next start. Interruption of the own
// network: approx. 1-3 s (not measured); clients have to reconnect.
void wifiMgrReconfigure();

// The "wifi on" command from USB: resets the failed-start counter and gaveUp (main loop, does not wait).
void wifiMgrClearFailures();

// Request for a full stop before deep sleep (netPrepareSleep). Does NOT WAIT: sets a flag
// and wakes the task, which performs the stop regardless of carBusy. Returns true when the radio is already
// off (phase OFF, WiFi.mode(WIFI_OFF) succeeded or the radio was not on). The flag stays in force
// until wifiMgrPoll() gets wantOn == true.
bool wifiMgrRequestStopNow();

// Copy of the current state (for the state snapshot and for NetPolicy). Does not call the driver: copies values
// refreshed by the task, under a critical section (portMUX). Any task.
void wifiMgrGetStatus(WifiStatus& out);

// true = phase RUNNING: the own network is up and the web server accepts requests. The "network works" condition
// when confirming the firmware after an update (netConfirmHealthy). Any task, no lock.
bool wifiMgrServing();

// Reply to the console command "wifi" (no arguments; main loop). Lines "[wifi] ..." via
// conPrintf: phase, own network name, address http://a.b.c.d, number of clients, home network (name,
// connected or not, address, signal, code of the last disconnect), address http://espcanbox.local,
// access to changes from the home network (wifi lan). At the end the line "[wifi] password: command wifi showpass".
// DOES NOT PRINT THE PASSWORD (console captures end up in logs and recordings that are meant to be published).
void wifiMgrPrintInfo();

// Command "wifi showpass": ONE line via conUsbPrintf (USB only): "[wifi] password: <password>"
// or "[wifi] password: (not generated yet)". The home network password is never printed.
void wifiMgrPrintPassword();
