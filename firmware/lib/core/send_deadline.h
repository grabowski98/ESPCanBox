// Total time limit for sending one HTTP response (src/net/http_server.cpp, sessSend). No hardware.
//
// esp_http_server limits only a single send (send_wait_timeout), counted again for every portion. A client that
// reads a few bytes every 2 s (slow-read attack, e.g. slowhttptest -X) kept the only server task busy for as long
// as it liked, also on GET / and GET /i18n/<lang>.json, which need no login (found in a code review). The clock
// starts at the first send of a response; once the limit has passed every further send fails and the server
// closes the connection. Test on a PC: test_host/net_util.cpp.
#pragma once
#include <stdint.h>

struct SendDeadline {
    bool running = false;
    uint32_t startMs = 0;

    // Start of a new response (handler entered): the clock starts again at its first send.
    void reset() { running = false; }

    // Before every send of the response. true = send; false = the limit has passed, drop the connection.
    bool allow(uint32_t nowMs, uint32_t limitMs) {
        if (!running) {
            running = true;
            startMs = nowMs;
            return true;
        }
        return (uint32_t)(nowMs - startMs) < limitMs;
    }
};
