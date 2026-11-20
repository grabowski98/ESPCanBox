// Console: the only path through which the program writes to USB.
//
// Why a separate module:
//  - The USB port (HWCDC) works with a transmit timeout of 0 ms, so that a missing computer does not block
//    the program. Side effect: when two tasks write at the same time, the text of one of them is lost
//    without a trace. That is why ONLY the main loop (the task that called consoleBegin) writes to USB.
//    The web server task and the Wi-Fi event functions write only to the panel ring (conLogf).
//  - The web panel shows the same as the USB console: every complete line printed by conPrintf also goes
//    to the line ring (src/net/diag_hub) when the program is built with network (NET_ENABLED).
//  - Serial.printf with text longer than 63 characters reaches for the heap (malloc in Print::vprintf).
//    conPrintf formats into a buffer on the stack, so there is no allocation in the main loop.
//
// The USB line format is fixed and read by tools: frame dump "(s.mmm) can0 ID#DATA",
// comments starting with "#", lines "[tag] ...". This module adds nothing to the text and changes nothing.
//
// SECRETS: the device password and the home network password must not pass through conPrintf, conPrintln
// or conLogf, because they would end up in the ring read by the web panel. A password is printed only through
// conUsbPrintf / conUsbPrintln (USB only = physical access to the device).
//
// The file does not depend on Arduino. Implementation: src/console.cpp, compiled in all
// environments. Without network (NET_ENABLED = 0) there is no copy to the panel, and conLogf does nothing.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Longest console command from the web panel including the terminating zero (NetRequest::text).
// Longest anticipated command: "wifi sta " (9) + network name (32) + "|" + password (64) = 106 characters.
// Lines from USB may be longer (USB_LINE_MAX in console_cmds.cpp: room for "settings import" with the longest
// export); a longer USB line is dropped whole with a "[con]" line, never executed cut off.
const size_t CON_LINE_MAX = 128;

// conPrintf / conUsbPrintf buffer on the caller's stack. Longer text is truncated to
// CON_PRINTF_MAX - 1 characters (the longest current line, the console help, has approx. 130 characters).
const size_t CON_PRINTF_MAX = 256;

// conLogf buffer on the caller's stack: one ring line (LineLog::MAX_LINE = 200) and the zero.
const size_t CON_LOG_MAX = 201;

// Call once in setup(), right after Serial.begin() and Serial.setTxTimeoutMs(0), from the main loop task.
// Remembers this task as the only owner of the USB port. Does not block, allocates nothing.
// NET_ENABLED only: also installs esp_log_set_vprintf() with its own hook capturing error lines
// from the ready-made ESP-IDF libraries (Wi-Fi, lwIP, HTTP server, mDNS). Without it such lines would go
// straight to the USB port (secondary USB-Serial-JTAG console in sdkconfig), outside the main loop, and could
// land in the middle of a "can0 ID#DATA" line. The hook runs in any task (also
// Wi-Fi), therefore: it formats into a buffer on the stack (at most CON_IDF_LINE_MAX bytes), strips end-of-line
// characters and colour codes, puts the line into a small static ring (CON_IDF_RING lines) under a
// portMUX critical section, without waiting and without the diag_hub lock; with a full ring the line
// is lost (counter). It never writes to USB. Whether IDF lines actually reached USB before
// with HWCDC active: to be checked on the bench (trigger an IDF error and watch the port).
void consoleBegin();

// Main loop, on every pass. Prints the lines collected by the hook as
// conPrintf("# idf: %s\n", line): a comment line (the USB format allows it) and a copy in the panel.
// At most CON_IDF_RING lines per call; without lines microseconds. Without network: does nothing.
void consolePoll();
const size_t CON_IDF_LINE_MAX = 128;
const uint8_t CON_IDF_RING = 8;

// Replaces Serial.printf: sends to USB exactly the same bytes that Serial.printf would send.
// Who calls: ONLY the main loop (and setup()).
// Blocking: does not wait for USB (timeout 0 ms: with no computer the text is lost, as before);
//   waits for the ring lock at most DIAG_LOCK_WAIT_MAIN_MS (src/net/diag_hub.h).
// Copy to the panel (NET_ENABLED only): the text is assembled into lines in the module's static buffer;
//   a complete line (up to the '\n' character) goes to the ring with the millis() time of its completion.
//   The '\r' character is skipped, a line longer than 200 characters is truncated in the ring, an empty one skipped.
//   A line can be assembled from several calls (that is how frames from the radio are printed).
// Safeguard (NET_ENABLED only): a call from a task other than the main loop does NOT touch USB;
//   the text then goes only to the ring, as through conLogf. It is a programmer error, but it must not
//   end with lost text of the main loop.
void conPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Replaces Serial.println(text): the text and the characters "\r\n". Without an argument: just the end of line.
// The same rules as conPrintf. 'text' zero terminated, of any length (it is not copied
// into a buffer on the stack).
void conPrintln(const char* text = "");

// The same as conPrintf / conPrintln, but ONLY to USB, without a copy to the panel. For two things:
//  1. dump of raw CAN frames (command "dump"): approx. 218 lines per second would push everything else
//     out of the ring, and the panel has its own frame preview anyway;
//  2. the device password in the reply to the "wifi" command.
// Who calls: ONLY the main loop. A call from another task is skipped entirely.
void conUsbPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void conUsbPrintln(const char* text = "");

// One line ONLY to the panel ring; it does not reach USB.
// Who calls: any task (web server, update handling, also the main loop).
//   Do NOT call from interrupts or from Wi-Fi event functions (those only set flags).
// One call = one line; end-of-line characters at the end of the text are stripped, text longer than
// 200 characters is truncated. Line time: millis() at the moment of the call.
// Blocking: waits for the ring lock at most DIAG_LOCK_WAIT_HTTP_MS; after that time the line
// is lost and the lost-records counter grows (diagDrops()).
// What absolutely has to be visible on USB (e.g. the start and the result of an update) is printed by the main loop
// through conPrintf when it notices the state change; conLogf does not replace that.
// Without network (NET_ENABLED = 0): the function exists and does nothing.
void conLogf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
