// Host test: XP protocol frames (lib/radio_xp/xp_protocol.h).
// xp::encode and xp::Parser round trip; bad checksum, bad length (> 40), truncated frame
// (30 ms timeout), garbage on the line, acknowledgement bytes FF / F3 between frames.
// Checks the behaviour of the current code (changes nothing in it).
//
// Run: bash tools/run_host_tests.sh (README.md, section on tests on recordings).
#include <cstdio>
#include <cstring>

#include "xp_protocol.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

static bool sameFrame(const xp::Frame& a, const xp::Frame& b) {
    return a.cmd == b.cmd && a.len == b.len && std::memcmp(a.data, b.data, a.len) == 0;
}

// Feeds bytes to the parser (all in the same millisecond). Returns the number of complete frames,
// leaves the last one in 'last'; also counts the ACK results.
static int feedAll(xp::Parser& p, const uint8_t* buf, size_t n, uint32_t ms, xp::Frame& last, int* acks = nullptr) {
    int frames = 0;
    for (size_t i = 0; i < n; i++) {
        xp::Frame f;
        xp::Parser::Result r = p.feed(buf[i], ms, f);
        if (r == xp::Parser::FRAME) {
            frames++;
            last = f;
        } else if (r == xp::Parser::ACK && acks) {
            (*acks)++;
        }
    }
    return frames;
}

int main() {
    uint8_t buf[64];
    xp::Frame got;

    // --- encode: byte layout and checksum ---
    {
        const uint8_t d[] = {0x01, 0x0A};
        xp::Frame f = xp::makeFrame(xp::BOX_POWER, d, 2);      // EA 01 0A
        size_t n = xp::encode(f, buf, sizeof(buf));
        // CHK = ~(EA + 02 + 01 + 0A) = ~F7 = 08
        const uint8_t want[] = {0x2E, 0xEA, 0x02, 0x01, 0x0A, 0x08};
        check(n == 6 && std::memcmp(buf, want, 6) == 0, "encode EA 01 0A = 2E EA 02 01 0A 08");
        check(xp::checksum(0xEA, 2, d) == 0x08, "checksum EA 02 01 0A = 08");
        xp::Frame e;
        e.cmd = 0x71;
        e.len = 0;
        n = xp::encode(e, buf, sizeof(buf));
        check(n == 4 && buf[0] == 0x2E && buf[1] == 0x71 && buf[2] == 0x00 && buf[3] == 0x8E, "encode without data: 2E 71 00 8E");
    }

    // --- encode: limits ---
    {
        xp::Frame f;
        f.cmd = 0x20;
        f.len = 2;
        f.data[0] = 0x14;
        f.data[1] = 0x01;
        check(xp::encode(f, buf, 5) == 0, "encode: buffer 1 byte too small = 0");
        check(xp::encode(f, buf, 6) == 6, "encode: buffer exactly for the frame = 6");
        check(xp::encode(f, buf, 0) == 0, "encode: buffer 0 = 0");
        f.len = xp::MAX_DATA;
        check(xp::encode(f, buf, sizeof(buf)) == (size_t)xp::MAX_DATA + 4, "encode: 40 data bytes = 44");
        f.len = xp::MAX_DATA + 1;
        check(xp::encode(f, buf, sizeof(buf)) == 0, "encode: 41 data bytes = 0");
        f.len = 255;
        check(xp::encode(f, buf, sizeof(buf)) == 0, "encode: length 255 = 0");
        uint8_t src[60];
        for (int i = 0; i < 60; i++) src[i] = (uint8_t)i;
        xp::Frame m = xp::makeFrame(0xCB, src, 60);
        check(m.len == xp::MAX_DATA && m.data[39] == 39, "makeFrame: 60 bytes cut to 40");
        xp::Frame z = xp::makeFrame(0x81, nullptr, 0);
        check(z.cmd == 0x81 && z.len == 0, "makeFrame: without data");
    }

    // --- round trip: every length 0..40, every command, data with bytes 2E / FF / F3 ---
    {
        int bad = 0;
        for (unsigned len = 0; len <= xp::MAX_DATA; len++) {
            xp::Frame f;
            f.cmd = (uint8_t)(0x20 + len * 5);
            f.len = (uint8_t)len;
            for (unsigned i = 0; i < len; i++) f.data[i] = (uint8_t)(len * 7 + i * 13);
            size_t n = xp::encode(f, buf, sizeof(buf));
            xp::Parser p;
            xp::Frame out;
            int frames = feedAll(p, buf, n, 1000, out);
            if (n != len + 4 || frames != 1 || !sameFrame(f, out) || p.stats().frames != 1) bad++;
        }
        check(bad == 0, "there and back: lengths 0..40");

        bad = 0;
        xp::Parser p;       // one parser for all commands, frame after frame
        for (unsigned cmd = 0; cmd <= 0xFF; cmd++) {
            const uint8_t d[] = {0x2E, 0xFF, 0xF3, (uint8_t)cmd};
            xp::Frame f = xp::makeFrame((uint8_t)cmd, d, 4);
            size_t n = xp::encode(f, buf, sizeof(buf));
            xp::Frame out;
            if (feedAll(p, buf, n, 2000 + cmd, out) != 1 || !sameFrame(f, out)) bad++;
        }
        check(bad == 0, "there and back: commands 00..FF with data 2E FF F3");
        check(p.stats().frames == 256 && p.stats().acks == 0 && p.stats().strayBytes == 0 &&
                  p.stats().badChecksum == 0 && p.stats().badLength == 0 && p.stats().timeouts == 0,
              "256 frames, bytes FF / F3 in the data are not acks");
    }

    // --- bad checksum ---
    {
        xp::Parser p;
        const uint8_t d[] = {0x14, 0x01};
        size_t n = xp::encode(xp::makeFrame(0x20, d, 2), buf, sizeof(buf));
        buf[n - 1] ^= 0x01;
        check(feedAll(p, buf, n, 100, got) == 0 && p.stats().badChecksum == 1 && p.stats().frames == 0,
              "bad checksum: frame rejected, counter 1");
        buf[n - 1] ^= 0x01;
        buf[3] ^= 0x80;         // corrupted data byte
        check(feedAll(p, buf, n, 101, got) == 0 && p.stats().badChecksum == 2, "corrupted data byte: rejected");
        buf[3] ^= 0x80;
        check(feedAll(p, buf, n, 102, got) == 1 && got.cmd == 0x20 && got.data[0] == 0x14 && p.stats().frames == 1,
              "after a bad checksum the next good frame goes through");
    }

    // --- bad length (> 40): we do not trust the length from the line ---
    {
        xp::Parser p;
        const uint8_t hdr41[] = {0x2E, 0x20, 41};
        check(feedAll(p, hdr41, 3, 100, got) == 0 && p.stats().badLength == 1, "length 41: rejected at once");
        const uint8_t hdrFF[] = {0x2E, 0x20, 0xFF};
        check(feedAll(p, hdrFF, 3, 100, got) == 0 && p.stats().badLength == 2, "length 255: rejected at once");
        const uint8_t d[] = {0x01, 0x0A};
        size_t n = xp::encode(xp::makeFrame(0xEA, d, 2), buf, sizeof(buf));
        check(feedAll(p, buf, n, 100, got) == 1 && got.cmd == 0xEA, "after a bad length the next frame goes through");
        // length 40 is still valid
        xp::Frame f;
        f.cmd = 0xCB;
        f.len = 40;
        n = xp::encode(f, buf, sizeof(buf));
        check(feedAll(p, buf, n, 100, got) == 1 && got.len == 40 && p.stats().badLength == 2, "length 40: accepted");
    }

    // --- truncated frame: 30 ms timeout between bytes ---
    {
        const uint8_t d[] = {0x38, 0x02, 0xE1};
        size_t n = xp::encode(xp::makeFrame(0x38, d, 3), buf, sizeof(buf));
        {
            xp::Parser p;
            feedAll(p, buf, 4, 1000, got);                       // half a frame
            int fr = feedAll(p, buf, n, 1031, got);              // 31 ms later a whole new frame
            check(fr == 1 && p.stats().timeouts == 1 && got.cmd == 0x38 && got.len == 3,
                  "truncated, a new frame after 31 ms: timeout 1, the new frame accepted");
        }
        {
            xp::Parser p;
            feedAll(p, buf, 4, 1000, got);
            int fr = feedAll(p, buf + 4, n - 4, 1030, got);      // exactly 30 ms: still the same frame
            check(fr == 1 && p.stats().timeouts == 0, "gap of exactly 30 ms: frame completed");
        }
        {
            xp::Parser p;
            feedAll(p, buf, 4, 1000, got);
            int fr = feedAll(p, buf + 4, n - 4, 1031, got);      // the rest after the timeout: does not get glued on
            check(fr == 0 && p.stats().timeouts == 1 && p.stats().frames == 0, "gap of 31 ms: the rest of the frame rejected");
            check(p.stats().strayBytes == n - 4, "the rest of the truncated frame counted as bytes outside frames");
        }
        {
            xp::Parser p;                                        // the millisecond counter wraps through zero
            feedAll(p, buf, 4, 0xFFFFFFF0u, got);
            int fr = feedAll(p, buf + 4, n - 4, 0x00000005u, got);   // 21 ms later
            check(fr == 1 && p.stats().timeouts == 0, "time wrapping through zero in the middle of a frame (21 ms): no timeout");
            feedAll(p, buf, 4, 0xFFFFFFF0u, got);
            fr = feedAll(p, buf, n, 0x00000010u, got);           // 32 ms later
            check(fr == 1 && p.stats().timeouts == 1, "time wrapping through zero, gap of 32 ms: timeout");
        }
        {
            xp::Parser p(5);                                     // custom timeout
            feedAll(p, buf, 4, 100, got);
            check(feedAll(p, buf, n, 106, got) == 1 && p.stats().timeouts == 1, "timeout set to 5 ms: a gap of 6 ms cuts");
        }
        {
            xp::Parser p;                                        // a long silence BETWEEN frames is not a timeout
            feedAll(p, buf, n, 100, got);
            check(feedAll(p, buf, n, 60100, got) == 1 && p.stats().timeouts == 0 && p.stats().frames == 2,
                  "a minute of silence between frames: no timeout");
        }
    }

    // --- garbage on the line ---
    {
        xp::Parser p;
        const char* text = "SS-PS004 AIAP\r\n";               // text without byte 2E
        int acks = 0;
        check(feedAll(p, (const uint8_t*)text, std::strlen(text), 100, got, &acks) == 0 && acks == 0 &&
                  p.stats().strayBytes == std::strlen(text) && p.stats().frames == 0,
              "text on the line: only bytes outside frames");
        const uint8_t d[] = {0x00};
        size_t n = xp::encode(xp::makeFrame(0x44, d, 1), buf, sizeof(buf));
        check(feedAll(p, buf, n, 200, got) == 1 && got.cmd == 0x44, "after garbage a frame goes through");
        const uint8_t zeros[8] = {0};
        check(feedAll(p, zeros, 8, 300, got) == 0 && p.stats().strayBytes == std::strlen(text) + 8, "zeros on the line: bytes outside frames");
        // garbage with byte 2E imitates a frame start; after the timeout the parser returns to receiving
        const uint8_t fake[] = {0x2E, 0x55};
        feedAll(p, fake, 2, 400, got);
        check(feedAll(p, buf, n, 431, got) == 1 && p.stats().timeouts == 1, "false header 2E: a frame goes through after 31 ms");
    }

    // --- acknowledgement bytes FF / F3 between frames ---
    {
        xp::Parser p;
        const uint8_t d[] = {0x14, 0x01};
        size_t n = xp::encode(xp::makeFrame(0x20, d, 2), buf, sizeof(buf));
        uint8_t line[64];
        size_t k = 0;
        line[k++] = 0xFF;
        std::memcpy(line + k, buf, n);
        k += n;
        line[k++] = 0xF3;
        line[k++] = 0xFF;
        std::memcpy(line + k, buf, n);
        k += n;
        line[k++] = 0xF3;
        int acks = 0;
        int fr = feedAll(p, line, k, 100, got, &acks);
        check(fr == 2 && acks == 4, "FF frame F3 FF frame F3: 2 frames, 4 acks");
        check(p.stats().frames == 2 && p.stats().acks == 4 && p.stats().strayBytes == 0 && p.stats().badChecksum == 0,
              "counters: frames 2, acks 4, no errors");
        xp::Frame f;
        check(p.feed(0xFF, 200, f) == xp::Parser::ACK && p.feed(0xF3, 200, f) == xp::Parser::ACK, "FF and F3 outside a frame = ACK");
        check(p.feed(0x00, 200, f) == xp::Parser::NONE, "another byte outside a frame = NONE");
        // reset() in the middle of a frame: the next frame goes through immediately
        feedAll(p, buf, 3, 300, got);
        p.reset();
        check(feedAll(p, buf, n, 300, got) == 1 && p.stats().timeouts == 0, "reset() in the middle of a frame: the next goes through");
    }

    // --- RadioRxClock (lib/core/radio_protocol.h): parser time when the bytes are read from a buffer ---
    // Review: with the loop time a loop pass stalled for more than 30 ms in the middle of a frame made a frame that
    // had fully arrived in the UART buffer look cut off. The clock only advances while the buffer is seen empty.
    {
        uint8_t buf[64];
        const uint8_t data[2] = {0x01, 0x01};
        size_t n = xp::encode(xp::makeFrame(0x80, data, 2), buf, sizeof(buf));
        {
            // Loop every 1 ms; the first 2 bytes in one pass, then a 50 ms stall, the rest already waiting.
            xp::Parser p;
            RadioRxClock c;
            xp::Frame got;
            c.observe(1000, true);
            uint32_t t = c.observe(1001, false);             // bytes waiting
            int fr = feedAll(p, buf, 2, t, got);
            c.observe(1002, true);                           // buffer drained, the rest not yet there
            t = c.observe(1052, false);                      // after the stall: the rest is in the buffer
            fr += feedAll(p, buf + 2, n - 2, t, got);
            check(fr == 1 && p.stats().timeouts == 0 && got.cmd == 0x80,
                  "RadioRxClock: loop stalled 50 ms, rest of the frame already buffered: frame accepted");
            // the same bytes with the loop time (behaviour before the fix): timeout
            xp::Parser q;
            feedAll(q, buf, 2, 1001, got);
            check(feedAll(q, buf + 2, n - 2, 1052, got) == 0 && q.stats().timeouts == 1,
                  "the same bytes with the loop time: the frame was taken for cut off (the reviewed defect)");
        }
        {
            // A real gap on the line: the buffer is seen empty for 40 ms between the halves of the frame.
            xp::Parser p;
            RadioRxClock c;
            xp::Frame got;
            uint32_t t = c.observe(2000, false);
            feedAll(p, buf, 2, t, got);
            for (uint32_t ms = 2001; ms <= 2040; ms++) c.observe(ms, true);
            t = c.observe(2041, false);
            check(feedAll(p, buf + 2, n - 2, t, got) == 0 && p.stats().timeouts == 1,
                  "RadioRxClock: buffer empty for 40 ms in the middle of a frame: still a timeout");
        }
        {
            // 30 ms seen empty (equal to the timeout), across the counter wrap: no timeout.
            xp::Parser p;
            RadioRxClock c;
            xp::Frame got;
            uint32_t t = c.observe(0xFFFFFFF0u, false);
            feedAll(p, buf, 2, t, got);
            for (uint32_t k = 1; k <= 30; k++) c.observe(0xFFFFFFF0u + k, true);
            t = c.observe(0xFFFFFFF0u + 31, false);
            check(feedAll(p, buf + 2, n - 2, t, got) == 1 && p.stats().timeouts == 0,
                  "RadioRxClock: 30 ms seen empty across the counter wrap: frame accepted");
        }
    }

    std::printf("checks: %d, errors: %d\n", checks, failures);
    std::printf("\n%s\n", failures == 0 ? "RESULT: MATCH" : "RESULT: ERRORS");
    return failures == 0 ? 0 : 1;
}
