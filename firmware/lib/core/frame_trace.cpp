#include "frame_trace.h"

static_assert((FrameTrace::CAPACITY & (FrameTrace::CAPACITY - 1)) == 0, "CAPACITY must be a power of two");

// Number comparison robust against wrap-around of the 32-bit counter.
static bool before(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }

// ---------------------------------------------------------------- FrameTrace

// Called for every frame from the bus (under the owner's lock): only a 16-byte copy.
void FrameTrace::push(const CanFrame& f, uint32_t ms) {
    TraceRecord& r = rec_[head_ & (CAPACITY - 1)];
    r.ms = ms;
    r.id = (uint16_t)(f.id & 0x7FF);    // the car's bus has only 11-bit identifiers
    r.len = f.len > 8 ? 8 : f.len;      // the length from outside is not trusted
    for (uint8_t i = 0; i < 8; i++) r.data[i] = i < r.len ? f.data[i] : 0;
    head_++;
    if (filled_ < CAPACITY) filled_++;
}

uint32_t FrameTrace::oldest() const { return head_ - filled_; }

size_t FrameTrace::read(uint32_t& cursor, TraceRecord* out, size_t max, uint32_t& dropped,
                        uint16_t idFilter) const {
    dropped = 0;
    if (before(head_, cursor)) cursor = head_;
    const uint32_t old = oldest();
    if (before(cursor, old)) {
        dropped = old - cursor;
        cursor = old;
    }
    if (out == nullptr) return 0;

    size_t n = 0;
    while (cursor != head_ && n < max) {
        const TraceRecord& r = rec_[cursor & (CAPACITY - 1)];
        cursor++;    // an inspected frame advances the cursor even when the filter skips it
        if (idFilter != NO_FILTER && r.id != idFilter) continue;
        out[n++] = r;
    }
    return n;
}

// Clears the content but does NOT reset the numbering (browser cursors stay valid).
void FrameTrace::clear() { filled_ = 0; }

// ---------------------------------------------------------------- FrameTable

// Called for every frame: binary search (at most 7 steps with 128 IDs). The table is shifted
// only on a new ID, i.e. a few dozen times after startup.
void FrameTable::onFrame(const CanFrame& f, uint32_t ms) {
    const uint16_t id = (uint16_t)(f.id & 0x7FF);
    const uint8_t len = f.len > 8 ? 8 : f.len;

    uint8_t lo = 0, hi = size_;    // look for the first position with e_[i].id >= id
    while (lo < hi) {
        uint8_t mid = (uint8_t)((lo + hi) / 2);
        if (e_[mid].id < id) lo = (uint8_t)(mid + 1);
        else hi = mid;
    }

    if (lo < size_ && e_[lo].id == id) {
        TableEntry& e = e_[lo];
        e.periodMs = ms - e.lastMs;
        e.lastMs = ms;
        e.count++;
        e.len = len;
        for (uint8_t i = 0; i < 8; i++) e.data[i] = i < len ? f.data[i] : 0;
        return;
    }

    if (size_ >= MAX_IDS) {
        overflow_++;
        return;
    }
    for (uint8_t i = size_; i > lo; i--) e_[i] = e_[i - 1];
    TableEntry& e = e_[lo];
    e.id = id;
    e.len = len;
    for (uint8_t i = 0; i < 8; i++) e.data[i] = i < len ? f.data[i] : 0;
    e.count = 1;
    e.lastMs = ms;
    e.periodMs = 0;    // only one frame so far
    size_++;
}

void FrameTable::clear() {
    size_ = 0;
    overflow_ = 0;
}
