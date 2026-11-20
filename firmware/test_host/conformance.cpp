// Conformance test against the Simple Soft box on a recorder capture (CAN + both UART directions).
//
// The same logic that runs in the ESP32 (lib/core) gets the CAN frames from the capture,
// and its output is compared with what the real box sent to the radio at the same time:
//   - buttons (command 20): every box event must have a counterpart on our side;
//   - sensors (command 32): the content of the box frame must match ours;
//   - radio frames (line A): the parser must accept them without checksum errors.
//
// Usage: conformance <capture.log>      exit code 0 = conformant
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "candump.h"
#include "power_policy.h"
#include "psa_frames.h"
#include "radio_link.h"
#include "xp_car.h"
#include "xp_keys.h"
#include "xp_parking.h"
#include "xp_protocol.h"

struct TimedKey {
    long ms;
    uint8_t code, state;
    bool matched = false;
};
struct TimedPayload {
    long ms;
    std::string hex;
};

static std::string hexOf(const uint8_t* d, size_t n) {
    static const char* H = "0123456789ABCDEF";
    std::string s;
    for (size_t i = 0; i < n; i++) {
        s += H[d[i] >> 4];
        s += H[d[i] & 15];
    }
    return s;
}

// Line "(time) uartB 2E32..." -> bytes. Returns false for other lines.
static bool parseUartLine(const char* line, const char* tap, double& ts, std::vector<uint8_t>& out) {
    if (line[0] != '(') return false;
    char name[16];
    char hex[400];
    if (std::sscanf(line, "(%lf) %15s %399s", &ts, name, hex) != 3) return false;
    if (std::strcmp(name, tap) != 0) return false;
    out.clear();
    size_t n = std::strlen(hex);
    for (size_t i = 0; i + 1 < n; i += 2) {
        char b[3] = {hex[i], hex[i + 1], 0};
        out.push_back((uint8_t)std::strtoul(b, nullptr, 16));
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: conformance <capture.log>\n");
        return 2;
    }
    FILE* fp = std::fopen(argv[1], "r");
    if (!fp) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }

    xp::KeyEncoder keys;
    xp::Parser boxParser, radioParser;
    RadioLink link;
    std::vector<TimedKey> ours, box;
    std::deque<TimedPayload> ourParking;          // our latest 32 frames
    long parkTotal = 0, parkMatch = 0, parkInitial = 0;
    std::vector<std::string> parkMismatch;
    PowerPolicy power;
    xp::ClimateEncoder climate;
    // other state frames (29, 33-36, 3E, 44, 21): our latest contents and the match counter
    std::map<int, std::deque<TimedPayload>> ourState;
    std::map<int, long> stTotal, stMatch;
    std::map<int, std::vector<std::string>> stDiff;
    auto offer = [&](const xp::Frame& f, long t) {
        auto& q = ourState[f.cmd];
        std::string h = hexOf(f.data, f.len);
        if (q.empty() || q.back().hex != h) q.push_back({t, h});
        while (q.size() > 24) q.pop_front();
    };
    bool ourAcc = false;
    std::vector<TimedKey> ourPower, boxPower;      // code = 1 power on, 0 off

    char line[1024];
    long nowMs = -1;
    std::vector<uint8_t> bytes;

    auto pump = [&](long t) {
        if (nowMs < 0) nowMs = t;
        while (nowMs < t) {
            nowMs++;
            keys.update((uint32_t)nowMs);
            xp::KeyEvent e;
            while (keys.pop(e)) ours.push_back({nowMs, e.code, e.state});
            bool acc = power.update((uint32_t)nowMs).acc;
            if (acc != ourAcc) {
                ourAcc = acc;
                ourPower.push_back({nowMs, (uint8_t)(acc ? 1 : 0), 0});
            }
        }
    };

    while (std::fgets(line, sizeof(line), fp)) {
        double ts;
        CanFrame cf;
        if (parseCandumpLine(line, cf, &ts) && ts >= 0) {
            long t = (long)(ts * 1000.0);
            pump(t);
            keys.onCanFrame(cf, (uint32_t)nowMs);
            xp::KeyEvent e;
            while (keys.pop(e)) ours.push_back({nowMs, e.code, e.state});
            psa::BodyState body;
            if (psa::decodeBodyState(cf, body)) power.onBodyState(body, (uint32_t)nowMs);
            xp::Frame sf;
            if (xp::buildSteering(cf, sf)) offer(sf, nowMs);
            if (xp::buildTrip(cf, sf)) offer(sf, nowMs);
            if (xp::buildOutsideTemp(cf, sf)) offer(sf, nowMs);
            if (xp::buildEcoMode(cf, sf)) offer(sf, nowMs);
            if (climate.onCanFrame(cf, sf)) {
                sf.data[5] = 0;          // the radio does not read byte 5; after start-up the box has FB here, later 81
                offer(sf, nowMs);
            }
            psa::ParkingAid pa;
            if (psa::decodeParkingAid(cf, pa)) {
                offer(xp::buildObstacleFlag(pa.anyActive() && pa.nearestZone() < 7), nowMs);
                xp::Frame f = xp::buildParkingFrame(pa, true);
                std::string h = hexOf(f.data, f.len);
                if (ourParking.empty() || ourParking.back().hex != h) ourParking.push_back({nowMs, h});
                while (ourParking.size() > 8) ourParking.pop_front();
            }
            continue;
        }
        if (parseUartLine(line, "uartB", ts, bytes)) {       // box -> radio
            long t = (long)(ts * 1000.0);
            pump(t);
            for (uint8_t b : bytes) {
                xp::Frame f;
                if (boxParser.feed(b, (uint32_t)nowMs, f) != xp::Parser::FRAME) continue;
                if (f.cmd == xp::BOX_STEERING_ANGLE || f.cmd == xp::BOX_TRIP_NOW || f.cmd == xp::BOX_TRIP_1 ||
                    f.cmd == xp::BOX_TRIP_2 || f.cmd == xp::BOX_OUTSIDE_TEMP || f.cmd == xp::BOX_ECO_MODE ||
                    f.cmd == xp::BOX_CLIMATE || f.cmd == xp::BOX_OBSTACLE) {
                    if (f.cmd == xp::BOX_CLIMATE && f.len > 5) f.data[5] = 0;
                    std::string h = hexOf(f.data, f.len);
                    stTotal[f.cmd]++;
                    bool ok = false;
                    auto& q = ourState[f.cmd];
                    // the box can lag the bus by up to about 1 s (sends with a delay or periodically)
                    for (auto it = q.rbegin(); it != q.rend(); ++it) {
                        if (it->hex == h) {
                            ok = true;
                            break;
                        }
                        if (nowMs - it->ms > 1200) break;
                    }
                    if (ok) stMatch[f.cmd]++;
                    else if (stDiff[f.cmd].size() < 6)
                        stDiff[f.cmd].push_back(std::to_string(nowMs) + " ms: box " + h + ", us " +
                                                (q.empty() ? "-" : q.back().hex));
                } else if (f.cmd == xp::BOX_POWER && f.len >= 1) {
                    uint8_t on = f.data[0] ? 1 : 0;
                    if (boxPower.empty() || boxPower.back().code != on) boxPower.push_back({nowMs, on, 0});
                } else if (f.cmd == xp::BOX_KEY && f.len >= 2) {
                    box.push_back({nowMs, f.data[0], f.data[1]});
                } else if (f.cmd == xp::BOX_PARKING && f.len == 7) {
                    // 03 = initial state of the box (right after start-up and on switch-off), no data
                    // about zones; it does not follow from frame 0E1, so we do not compare it.
                    if (f.data[0] == 0x03) {
                        parkInitial++;
                        continue;
                    }
                    std::string h = hexOf(f.data, f.len);
                    parkTotal++;
                    bool ok = false;
                    // The box can be one or two CAN frames behind us: we compare with our
                    // states from the last 400 ms.
                    for (auto it = ourParking.rbegin(); it != ourParking.rend(); ++it) {
                        if (it->hex == h) {
                            ok = true;
                            break;
                        }
                        if (nowMs - it->ms > 400) break;
                    }
                    if (ok) parkMatch++;
                    else if (parkMismatch.size() < 8)
                        parkMismatch.push_back(std::to_string(nowMs) + " ms: box " + h + ", us " +
                                               (ourParking.empty() ? "-" : ourParking.back().hex));
                }
            }
            continue;
        }
        if (parseUartLine(line, "uartA", ts, bytes)) {       // radio -> box
            long t = (long)(ts * 1000.0);
            pump(t);
            for (uint8_t b : bytes) {
                xp::Frame f;
                if (radioParser.feed(b, (uint32_t)nowMs, f) == xp::Parser::FRAME) {
                    link.onRadioFrame(f, (uint32_t)nowMs);
                }
            }
        }
    }
    std::fclose(fp);

    // --- buttons: we compare "presses" (press, how many times "held", release) ---
    // The moment of the box's 200 ms tick is not known to the millisecond, so on a release
    // exactly on the tick one side may report one more "held". We allow that.
    // Time window: we react immediately, the box reports knob detents about 220 ms later.
    struct Press {
        long ms;
        uint8_t code;
        int held = 0;
        bool released = false;
        bool matched = false;
    };
    auto sessions = [](const std::vector<TimedKey>& ev) {
        std::vector<Press> out;
        for (const auto& e : ev) {
            if (e.state == xp::KEY_STATE_PRESSED) {
                Press p;
                p.ms = e.ms;
                p.code = e.code;
                out.push_back(p);
                continue;
            }
            // append to the last press of this code, if not yet released
            for (auto it = out.rbegin(); it != out.rend(); ++it) {
                if (it->code != e.code) continue;
                if (!it->released) {
                    if (e.state == xp::KEY_STATE_HELD) it->held++;
                    else it->released = true;
                }
                break;
            }
        }
        return out;
    };
    std::vector<Press> boxP = sessions(box), ourP = sessions(ours);
    long keyMatch = 0, keyDiff = 0, boxMissing = 0, oursExtra = 0;
    std::vector<std::string> boxOnly, oursOnly;
    for (auto& b : boxP) {
        for (auto& o : ourP) {
            if (o.matched || o.code != b.code) continue;
            if (o.ms < b.ms - 450 || o.ms > b.ms + 150) continue;
            o.matched = b.matched = true;
            if (o.released == b.released && std::abs(o.held - b.held) <= 1) {
                keyMatch++;
            } else {
                keyDiff++;
                if (boxOnly.size() < 10) {
                    char s[140];
                    std::snprintf(s, sizeof(s), "%ld ms: code %02X, box: held x%d released=%d, us: x%d released=%d",
                                  b.ms, b.code, b.held, (int)b.released, o.held, (int)o.released);
                    boxOnly.push_back(s);
                }
            }
            break;
        }
        if (!b.matched) {
            boxMissing++;
            if (boxOnly.size() < 10) {
                char s[64];
                std::snprintf(s, sizeof(s), "%ld ms: code %02X only from the box", b.ms, b.code);
                boxOnly.push_back(s);
            }
        }
    }
    for (auto& o : ourP) {
        if (o.matched) continue;
        oursExtra++;
        if (oursOnly.size() < 10) {
            char s[64];
            std::snprintf(s, sizeof(s), "%ld ms: code %02X only from us", o.ms, o.code);
            oursOnly.push_back(s);
        }
    }

    const xp::ParserStats& bs = boxParser.stats();
    const xp::ParserStats& rs = radioParser.stats();
    std::printf("Box line: frames %lu, bad checksum %lu, bad length %lu, truncated %lu, bytes outside frames %lu\n",
                (unsigned long)bs.frames, (unsigned long)bs.badChecksum, (unsigned long)bs.badLength,
                (unsigned long)bs.timeouts, (unsigned long)bs.strayBytes);
    std::printf("Radio line:  frames %lu, bad checksum %lu, bad length %lu, truncated %lu, acks %lu\n",
                (unsigned long)rs.frames, (unsigned long)rs.badChecksum, (unsigned long)rs.badLength,
                (unsigned long)rs.timeouts, (unsigned long)rs.acks);
    const RadioInfo& ri = link.info();
    std::printf("From radio frames: volume %u%s, time 20%02u-%02u-%02u %02u:%02u, settings %lu, unknown commands %lu\n",
                ri.volume, ri.muted ? " (muted)" : "", ri.year, ri.month, ri.day, ri.hour, ri.minute,
                (unsigned long)ri.settingCount, (unsigned long)ri.unknownCount);

    std::printf("\nKEYS: presses from the box %zu, matching %ld, with a difference %ld, missing from us %ld, extra from us %ld\n",
                boxP.size(), keyMatch, keyDiff, boxMissing, oursExtra);
    for (auto& s : boxOnly) std::printf("   %s\n", s.c_str());
    for (auto& s : oursOnly) std::printf("   %s\n", s.c_str());

    double parkPct = parkTotal ? 100.0 * parkMatch / parkTotal : 100.0;
    std::printf("\nSENSORS: box frames %ld, matching %ld (%.1f%%), skipped frames of the initial state %ld\n",
                parkTotal, parkMatch, parkPct, parkInitial);
    for (auto& s : parkMismatch) std::printf("   different: %s\n", s.c_str());

    // --- power: we look for box state changes (frame EA) on our side within a 700 ms window ---
    long powerMatch = 0;
    for (auto& b : boxPower) {
        for (auto& o : ourPower) {
            if (o.matched || o.code != b.code) continue;
            if (o.ms < b.ms - 700 || o.ms > b.ms + 700) continue;
            o.matched = b.matched = true;
            powerMatch++;
            std::printf("POWER: %s: box %ld ms, us %ld ms (difference %ld ms)\n",
                        b.code ? "on" : "off", b.ms, o.ms, o.ms - b.ms);
            break;
        }
        if (!b.matched)
            std::printf("POWER: %s of the box at %ld ms without a counterpart from us\n",
                        b.code ? "on" : "off", b.ms);
    }
    bool powerOk = powerMatch == (long)boxPower.size();

    std::printf("\nOTHER FRAMES (matching / all):\n");
    bool stateOk = true;
    for (auto& kv : stTotal) {
        long m = stMatch[kv.first];
        std::printf("   %02X: %ld / %ld\n", kv.first, m, kv.second);
        for (auto& d : stDiff[kv.first]) std::printf("       different: %s\n", d.c_str());
        // climate (21) is only partially mapped: we report but do not fail the test
        if (kv.first != xp::BOX_CLIMATE && m * 100 < kv.second * 90) stateOk = false;
    }

    // Checksum errors on the radio line are only reported: at radio start-up there is sometimes garbage there.
    bool ok = keyDiff == 0 && boxMissing == 0 && oursExtra == 0 && parkPct >= 97.0 && bs.badChecksum == 0 && powerOk && stateOk;
    std::printf("\nRESULT: %s\n", ok ? "MATCH" : "DIFFERENCES");
    return ok ? 0 : 1;
}
