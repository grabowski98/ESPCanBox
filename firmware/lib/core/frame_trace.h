// Bus view for the web panel: a ring of the latest CAN frames and a "last state of each ID" table.
// No hardware and no heap. Locking between tasks is provided by the owner (src/net/diag_hub).
//
// Ring: at about 218 frames per second 2048 records are more than 9 s of bus traffic, so
// a browser polling every half second does not lose frames even during a momentary network stall.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "can_frame.h"

struct TraceRecord {
    uint32_t ms = 0;        // reception time (millis)
    uint16_t id = 0;        // 11-bit identifier
    uint8_t len = 0;        // 0..8
    uint8_t data[8] = {0};
};

class FrameTrace {
public:
    static const uint32_t CAPACITY = 2048;      // must be a power of two
    static const uint16_t NO_FILTER = 0xFFFF;

    void push(const CanFrame& f, uint32_t ms);

    uint32_t head() const { return head_; }     // number the next frame will get
    uint32_t oldest() const;                    // number of the oldest frame in the ring

    // Copies into 'out' at most 'max' frames with numbers >= cursor. With an ID filter it skips the others
    // (inspected records also advance the cursor). Returns the number copied.
    // On return 'cursor' = number of the first frame not yet inspected.
    // 'dropped' = how many frames from the requested number have already fallen out of the ring.
    // A cursor "from the future" (greater than head) is treated as head.
    size_t read(uint32_t& cursor, TraceRecord* out, size_t max, uint32_t& dropped,
                uint16_t idFilter = NO_FILTER) const;

    void clear();

private:
    TraceRecord rec_[CAPACITY];
    uint32_t head_ = 0;
    uint32_t filled_ = 0;     // how many records are in the ring (0..CAPACITY); oldest() = head_ - filled_
};

struct TableEntry {
    uint16_t id = 0;
    uint8_t len = 0;
    uint8_t data[8] = {0};
    uint32_t count = 0;       // how many frames of this ID since startup
    uint32_t lastMs = 0;      // time of the last frame
    uint32_t periodMs = 0;    // interval between the two last frames (0 = only one so far)
};

class FrameTable {
public:
    static const uint8_t MAX_IDS = 128;         // the car sends about 70 identifiers (measured 10.11.2026)

    void onFrame(const CanFrame& f, uint32_t ms);

    uint8_t size() const { return size_; }
    const TableEntry& at(uint8_t i) const { return e_[i]; }   // sorted ascending by ID
    uint32_t overflow() const { return overflow_; }           // frames with IDs that did not fit in the table

    void clear();

private:
    TableEntry e_[MAX_IDS];
    uint8_t size_ = 0;
    uint32_t overflow_ = 0;
};
