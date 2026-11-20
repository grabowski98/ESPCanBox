// Host test: a foreign radio protocol (the template lib/radio_example, invented framing 55 AA LEN CMD DATA SUM)
// gets its OWN framing and not a single XP frame or XP byte sequence.
//   radio_example <synthetic example capture> <PSA AEE2010 capture>...
// Runs the Decoder (lib/app), unchanged, with:
//   1. the template car profile lib/car_example on the synthetic capture;
//   2. the reference car profile PSA AEE2010 on recordings from the car.
// For every run, every transmission handed to the UART must be exactly one example frame (sync 55 AA, valid sum,
// nothing left over); the whole byte stream fed to the XP parser must give no XP frame, and no transmission may
// start with the XP header 2E. Frames from the radio go through the example parser only: the request "send
// everything" brings the version and the state frames, an XP-looking command (80 01 01) reaches nothing.
// The protocol properties (latch key, rate limit, no acknowledgement) are checked from the protocol table.
#include <cstdio>
#include <cstring>
#include <vector>

#include "profile_example.h"
#include "protocol_example.h"
#include "replay_frames.h"
#include "xp_protocol.h"

namespace {

int g_bad = 0;

void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) g_bad++;
}

struct Seen {
    unsigned long tx = 0, bytes = 0, bad = 0, xpHeader = 0;
    unsigned long perCmd[256];
    std::vector<uint8_t> stream;     // every byte handed to the UART, in order
};
Seen g_seen;
uint32_t g_firstSteeringMs = 0, g_lastSteeringMs = 0;
unsigned long g_steeringTooSoon = 0;

void onStep(uint32_t ms, const DecoderOutputs& o, const uint8_t* buf, size_t n) {
    (void)o;
    if (n == 0) return;
    g_seen.tx++;
    g_seen.bytes += n;
    g_seen.stream.insert(g_seen.stream.end(), buf, buf + n);
    if (buf[0] == xp::HEADER) g_seen.xpHeader++;
    // Exactly one example frame per transmission.
    example_radio::Parser p;
    RadioFrame f;
    unsigned frames = 0;
    for (size_t i = 0; i < n; i++)
        if (p.feed(buf[i], ms, f) == RadioRxParser::FRAME) frames++;
    const RadioRxStats& st = p.stats();
    if (frames != 1 || st.strayBytes || st.badChecksum || st.badLength || buf[0] != example_radio::SYNC1 ||
        f.len + 5u != n) {
        g_seen.bad++;
        return;
    }
    g_seen.perCmd[f.cmd]++;
    if (f.cmd == example_radio::CMD_STEERING) {
        if (g_lastSteeringMs && ms - g_lastSteeringMs < 200) g_steeringTooSoon++;
        if (!g_firstSteeringMs) g_firstSteeringMs = ms;
        g_lastSteeringMs = ms;
    }
}

void resetSeen() {
    g_seen.tx = g_seen.bytes = g_seen.bad = g_seen.xpHeader = 0;
    std::memset(g_seen.perCmd, 0, sizeof(g_seen.perCmd));
    g_seen.stream.clear();
    g_firstSteeringMs = g_lastSteeringMs = 0;
    g_steeringTooSoon = 0;
}

// The whole stream through the XP parser: number of XP frames found.
unsigned long xpFramesIn(const std::vector<uint8_t>& s) {
    xp::Parser p;
    xp::Frame f;
    unsigned long n = 0;
    for (size_t i = 0; i < s.size(); i++)
        if (p.feed(s[i], 0, f) == xp::Parser::FRAME) n++;
    return n;
}

void printCounts(const char* name) {
    std::printf("%s: transmissions=%lu bytes=%lu bad=%lu xp_header=%lu xp_frames=%lu steering_too_soon=%lu\n", name,
                g_seen.tx, g_seen.bytes, g_seen.bad, g_seen.xpHeader, xpFramesIn(g_seen.stream), g_steeringTooSoon);
    std::printf("%s: frames by number:", name);
    for (unsigned c = 0; c < 256; c++)
        if (g_seen.perCmd[c]) std::printf(" %02X=%lu", c, g_seen.perCmd[c]);
    std::printf("\n");
}

bool runOne(const char* path, const vehicle::CarProfile* car, const char* name) {
    std::vector<ReplayFrame> frames;
    if (!loadCapture(path, frames) || frames.empty()) {
        std::printf("cannot read %s\n", path);
        return false;
    }
    std::printf("--- %s + example radio\n", name);
    resetSeen();
    static Decoder d;
    d = Decoder();
    DecoderConfig cfg;
    cfg.car = car;
    cfg.radio = &kRadioExample;
    d.begin(cfg, nullptr);
    replay(d, frames, 1000, onStep);
    printCounts(name);
    char what[160];
    std::snprintf(what, sizeof(what), "%s: every transmission is one example frame, none starts with 2E, no XP frame",
                  name);
    check(g_seen.tx > 0 && g_seen.bad == 0 && g_seen.xpHeader == 0 && xpFramesIn(g_seen.stream) == 0, what);
    // The rate limit acts when a frame is queued; the queue and the 20 ms gap may move single transmissions
    // closer together (steering_too_soon, for information), but never more than one frame per 200 ms on average.
    const unsigned long steering = g_seen.perCmd[example_radio::CMD_STEERING];
    const unsigned long allowed = g_firstSteeringMs ? (g_lastSteeringMs - g_firstSteeringMs) / 200 + 1 : 0;
    std::snprintf(what, sizeof(what), "%s: steering angle frames %lu <= %lu (protocol rate limit 200 ms)", name,
                  steering, allowed);
    check(steering <= allowed, what);
    return true;
}

// Frames from the radio, the radio side of the session and the protocol properties, on a short synthetic run:
// PSA AEE2010 profile (ignition frame 0F6 byte 1 = 88 every 100 ms gives ACC) + example radio.
void radioSide() {
    std::printf("--- frames from the radio\n");
    static Decoder d;
    d = Decoder();
    DecoderConfig cfg;
    cfg.car = &kCarPsaAee2010;
    cfg.radio = &kRadioExample;
    d.begin(cfg, nullptr);
    uint8_t buf[RADIO_TX_MAX_BYTES];
    // An XP frame "80 01 01" (vehicle setting) on the line: the example parser finds nothing in it.
    uint8_t xpSetting[6] = {xp::HEADER, 0x80, 0x02, 0x01, 0x01, 0};
    xpSetting[5] = xp::checksum(0x80, 2, xpSetting + 3);
    example_radio::Parser p;
    RadioFrame f;
    unsigned found = 0;
    for (uint8_t b : xpSetting)
        if (p.feed(b, 0, f) == RadioRxParser::FRAME) found++;
    check(found == 0, "XP bytes 2E 80 02 01 01 CHK: no example frame");
    // A frame with the XP setting command number and content in the example framing: unknown to the example,
    // never a setting command (the protocol has no decodeCommand).
    RadioFrame cmd;
    cmd.cmd = 0x90;
    cmd.len = 2;
    cmd.data[0] = 0x01;
    cmd.data[1] = 0x01;
    d.onRadioFrame(cmd, 10);
    check(!d.settingCmds().haveLast && d.link().info().unknownCount == 1,
          "example frame 90 01 01: counted as unknown, no setting command (protocol has no decodeCommand)");
    RadioFrame vol;
    vol.cmd = example_radio::RX_VOLUME;
    vol.len = 1;
    vol.data[0] = 17;
    d.onRadioFrame(vol, 20);
    check(d.link().info().volume == 17, "example frame 81 11: volume 17 in RadioInfo");
    // Latch key from the protocol table: first press = pressed, second press = released, the code unchanged.
    RadioKeyEvent in, out;
    in.code = example_radio::KEY_MOON;
    in.state = 1;
    const bool s1 = d.keyMap().onEvent(in, out);
    const uint8_t first = out.state;
    const bool s2 = d.keyMap().onEvent(in, out);
    check(s1 && first == 1 && s2 && out.state == 0 && out.code == example_radio::KEY_MOON,
          "latch key 08 (protocol table): press -> pressed, press -> released");
    in.code = 0xA4;           // the XP moon code is an ordinary key here
    const bool s3 = d.keyMap().onEvent(in, out);
    check(s3 && out.state == 1 && !d.keyMap().screenOff(), "code A4 has no latch with this protocol");
    check(!kRadioExample.sendAck, "no acknowledgement byte (RadioProtocol::sendAck = false)");

    // Ignition on: frame 0F6 byte 1 = 88 every 100 ms for 2 s; then the radio asks for everything.
    CanFrame ign;
    ign.id = 0x0F6;
    ign.len = 8;
    for (uint8_t i = 0; i < 8; i++) ign.data[i] = 0;
    ign.data[0] = 0x88;
    unsigned power = 0, versions = 0, xpLike = 0, total = 0;
    for (uint32_t ms = 100; ms < 3000; ms++) {
        if (ms % 100 == 0) d.onCanFrame(ign, ms, true);
        if (ms == 2000) {
            RadioFrame all;
            all.cmd = example_radio::RX_SEND_ALL;
            d.onRadioFrame(all, ms);
        }
        d.tick(ms, false);
        const size_t n = d.nextRadioTx(ms, true, buf, sizeof(buf));
        if (n == 0) continue;
        total++;
        if (buf[0] == xp::HEADER) xpLike++;
        if (n == 6 && buf[3] == example_radio::CMD_POWER && buf[4] == 1) power++;
        if (n == 8 && buf[3] == example_radio::CMD_VERSION && buf[4] == 'E' && buf[5] == 'X' && buf[6] == '1') versions++;
    }
    std::printf("radio side: transmissions=%u power_on=%u version=%u xp_header=%u\n", total, power, versions, xpLike);
    check(power == 1 && versions == 1 && xpLike == 0,
          "ACC on: one power frame 55 AA 01 10 01; request 80: one version frame 55 AA 03 7F 'EX1'; nothing XP-framed");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: radio_example <example capture> <psa capture>...\n");
        return 2;
    }
    if (!runOne(argv[1], &kCarExample, "example")) g_bad++;
    for (int i = 2; i < argc; i++)
        if (!runOne(argv[i], &kCarPsaAee2010, "psa_aee2010")) g_bad++;
    radioSide();
    std::printf("%s\n", g_bad ? "RULES BROKEN" : "the example radio got only its own framing, no XP frame");
    return g_bad ? 1 : 0;
}
