#include "xp_protocol.h"

#include <string.h>

namespace xp {

uint8_t checksum(uint8_t cmd, uint8_t len, const uint8_t* data) {
    uint8_t sum = (uint8_t)(cmd + len);
    for (uint8_t i = 0; i < len; i++) sum = (uint8_t)(sum + data[i]);
    return (uint8_t)~sum;
}

size_t encode(const Frame& f, uint8_t* out, size_t capacity) {
    if (f.len > MAX_DATA) return 0;
    size_t total = (size_t)f.len + 4;          // header, command, length, data, checksum
    if (capacity < total) return 0;
    out[0] = HEADER;
    out[1] = f.cmd;
    out[2] = f.len;
    memcpy(out + 3, f.data, f.len);
    out[3 + f.len] = checksum(f.cmd, f.len, f.data);
    return total;
}

Frame makeFrame(uint8_t cmd, const uint8_t* data, uint8_t len) {
    Frame f;
    f.cmd = cmd;
    f.len = len > MAX_DATA ? MAX_DATA : len;
    if (data && f.len) memcpy(f.data, data, f.len);
    return f;
}

void Parser::reset() {
    state_ = WAIT_HEADER;
    got_ = 0;
}

Parser::Result Parser::feed(uint8_t byte, uint32_t nowMs, Frame& out) {
    // Frame cut off halfway: start over, so that one lost byte
    // does not shift the reception of all following frames.
    if (state_ != WAIT_HEADER && (uint32_t)(nowMs - lastByteMs_) > timeoutMs_) {
        stats_.timeouts++;
        reset();
    }
    lastByteMs_ = nowMs;

    switch (state_) {
        case WAIT_HEADER:
            if (byte == HEADER) {
                state_ = WAIT_CMD;
            } else if (byte == ACK_FROM_RADIO || byte == ACK_FROM_BOX) {
                stats_.acks++;
                return ACK;
            } else {
                stats_.strayBytes++;
            }
            return NONE;

        case WAIT_CMD:
            cur_.cmd = byte;
            state_ = WAIT_LEN;
            return NONE;

        case WAIT_LEN:
            if (byte > MAX_DATA) {             // we do not trust the length from the line
                stats_.badLength++;
                reset();
                return NONE;
            }
            cur_.len = byte;
            got_ = 0;
            state_ = byte ? WAIT_DATA : WAIT_CHECKSUM;
            return NONE;

        case WAIT_DATA:
            cur_.data[got_++] = byte;
            if (got_ >= cur_.len) state_ = WAIT_CHECKSUM;
            return NONE;

        case WAIT_CHECKSUM:
            reset();
            if (byte != checksum(cur_.cmd, cur_.len, cur_.data)) {
                stats_.badChecksum++;
                return NONE;
            }
            stats_.frames++;
            out = cur_;
            return FRAME;
    }
    reset();
    return NONE;
}

}  // namespace xp
