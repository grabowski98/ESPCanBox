// Diagnostic hub: the only meeting point between the main loop and the web server task.
//
// Inside (all static, no heap):
//   - ring of text lines (LineLog, 8 KB): copy of the console;
//   - ring of the latest CAN frames (FrameTrace, 2048 frames = approx. 9 s of bus traffic) and the
//     "last state of every ID" table (FrameTable, up to 128 IDs);
//   - the latest state snapshot (StatusSnapshot);
//   - request queue panel -> main loop (NET_REQ_QUEUE_LEN slots);
//   - time of the last HTTP request with a correct password (for the stay-awake decision).
//
// LOCK: a single static one (xSemaphoreCreateMutexStatic), shared by everything above.
// Hold time is bounded: a writer inserts one record, a reader copies at most as much as
// its buffer holds. Under the lock it is NOT allowed to: format text or JSON, send to the network,
// write to USB, call Wi-Fi functions or the settings module, wait for anything. The longest operations:
// reading frames with an ID filter (scanning up to 2048 records) and reading lines (up to 4 KB byte by byte),
// both estimated at tens of microseconds (to be measured). The lock is released by the same task that took it.
//
// Who calls what:
//   main loop : diagBegin, diagOnCanFrame, diagPublishStatus, diagTakeRequest, diagClientActivity
//   web task  : diagGetStatus, diagReadLog, diagReadFrames, diagReadTable, diagHeads,
//               diagPostRequest, diagTouchClient
//   any       : diagLogLine (via console.cpp), diagDrops, diagReady
// Nothing from this file may be called from interrupts or from Wi-Fi event callbacks.
//
// Before a successful diagBegin() all functions are safe: writers do nothing, readers
// return "no data".
//
// Internal file of src/net (besides net.cpp it is included only by console.cpp, under #if NET_ENABLED).
// Does not depend on Arduino. Implementation: src/net/diag_hub.cpp.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "can_frame.h"
#include "diag_types.h"
#include "frame_trace.h"
#include "line_log.h"

// Maximum time the main loop waits for the lock. The web task holds the lock for microseconds, but
// it may itself be briefly preempted by Wi-Fi tasks; the main loop must not stall then.
// After this time the record is lost (counted by diagDrops()). Does not apply to CAN frames: see diagOnCanFrame.
const uint32_t DIAG_LOCK_WAIT_MAIN_MS = 2;
// Maximum time the web task (and other tasks outside the main loop) waits. After this time the read returns
// false, and the server answers 503 "busy".
const uint32_t DIAG_LOCK_WAIT_HTTP_MS = 100;
// CAN frames waiting for the lock (private buffer of the main loop): 32 frames = approx. 150 ms of bus traffic.
const uint8_t DIAG_PENDING_FRAMES = 32;

// ---------- start ----------

// Call once in setup() (via netBegin). Creates the lock and the request queue (xQueueCreateStatic).
// false = failed; the network must then not start, the rest of the program runs normally.
bool diagBegin();
bool diagReady();                 // true after a successful diagBegin(); any task

// ---------- writing: main loop ----------

// Every CAN frame accepted by the logic (received from the bus or pasted in the console).
// NEVER waits: tries to take the lock with timeout 0. When it is taken, the frame goes to a private
// buffer of DIAG_PENDING_FRAMES; the next call that gets the lock (this one or
// diagPublishStatus, which takes the lock every STATUS_PERIOD_MS anyway) first copies the pending
// frames in reception order (with their own timestamps). Thanks to this the last frames before silence on
// the bus also reach the preview, at the latest after 200 ms. Overflow of the
// buffer = frame lost for the preview (diagDrops() grows); the car logic loses nothing.
// Under the lock: FrameTrace::push and FrameTable::onFrame for every copied frame.
void diagOnCanFrame(const CanFrame& f, uint32_t ms);

// Under a single lock: first copies the pending CAN frames (see diagOnCanFrame), then replaces
// the latest state snapshot (copy of the whole structure). Waits at most DIAG_LOCK_WAIT_MAIN_MS; if it
// fails, the snapshot is lost (next one in STATUS_PERIOD_MS) and diagDrops() grows.
void diagPublishStatus(const StatusSnapshot& s);

// ---------- writing: any task ----------

// One line into the text ring. 'text' need not be zero-terminated; 'len' characters.
// Called by console.cpp (conPrintf, conLogf); other modules write via console.h.
// Waits for the lock DIAG_LOCK_WAIT_MAIN_MS in the main loop, DIAG_LOCK_WAIT_HTTP_MS in other
// tasks (recognised by the task handle remembered in diagBegin). No lock = the line
// is lost and diagDrops() grows.
void diagLogLine(const char* text, size_t len, uint32_t ms);

// ---------- reading: web task ----------
// All of them wait for the lock at most DIAG_LOCK_WAIT_HTTP_MS. Buffers are supplied by the caller; they are
// static working buffers of the web task (sizes in http_server.h), not the stack.

// Copy of the latest state snapshot. false = none published yet or the lock is taken.
bool diagGetStatus(StatusSnapshot& out);

// Lines from number 'cursor'; parameters and result exactly as LineLog::read (lib/core/line_log.h).
// Return value: number of lines; 'ok' = false when the lock could not be taken (then nothing was
// copied, and 'cursor' and 'dropped' are unchanged).
size_t diagReadLog(uint32_t& cursor, LineLog::Entry* entries, size_t maxEntries, char* text, size_t textCap,
                   uint32_t& dropped, bool& ok);

// Frames from number 'cursor'; parameters and result exactly as FrameTrace::read (lib/core/frame_trace.h).
// 'idFilter' = FrameTrace::NO_FILTER or an identifier 0..0x7FF. 'ok' as above.
size_t diagReadFrames(uint32_t& cursor, TraceRecord* out, size_t max, uint32_t& dropped, uint16_t idFilter,
                      bool& ok);

// Copy of the ID table, ascending by identifier. Returns the number of entries (at most 'max'; supply a buffer for
// FrameTable::MAX_IDS). 'overflow' = counter of frames with IDs that did not fit in the table. 'ok' as above.
uint8_t diagReadTable(TableEntry* out, uint8_t max, uint32_t& overflow, bool& ok);

// Current "next line" and "next frame" numbers (the cursor from which a client starts "from now on")
// and the numbers of the oldest records still in the rings. false = lock taken.
bool diagHeads(uint32_t& logHead, uint32_t& logOldest, uint32_t& traceHead, uint32_t& traceOldest);

// ---------- request queue ----------

// Web task: inserts a request (copy). Does not wait for space. false = queue full or the hub
// has not started; the server then answers 503 "busy". The FreeRTOS queue has its own protection,
// the hub lock is not taken here.
bool diagPostRequest(const NetRequest& r);

// Main loop: takes the oldest request. Does not wait. false = empty.
bool diagTakeRequest(NetRequest& out);

// ---------- panel client activity ----------

// Web task: call after every request that passed the password check (httpRequireAuth
// does this). Requests without a password or with a wrong password do NOT count as activity:
// a stranger in the network cannot keep the device awake. Two volatile variables, no lock.
void diagTouchClient(uint32_t nowMs);

// Main loop: whether such a request has happened since start and when the last one was (inputs clientSeen
// and lastClientMs for NetPolicy). No lock. NOTE: the web task runs on the other core, so
// the returned time may be NEWER than 'nowMs' of the current loop pass; compute the age as
// (int32_t)(nowMs - lastMs), a negative result = 0 (NetPolicy does this itself, see net_policy.h).
bool diagClientActivity(uint32_t& lastMs);

// ---------- counters ----------

// How many records (lines, frames, state snapshots) have been lost since start because the lock was taken or
// the pending-frame buffer overflowed. 0 in a healthy system. Any task, no lock.
uint32_t diagDrops();
