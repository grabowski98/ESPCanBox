// Stream golden files: what the device in decoder mode would send to the radio on a capture from the car.
// The glue logic from version 0.6.0 (ref_glue_060.h).
// With the --decoder parameter the same is done by the Decoder class (lib/app/decoder.h); the files of both
// modes must be identical byte for byte.
// Decoder computes frame 38 DATA[1..5] and frame 3C from CAN, so its stream differs from 0.6.0
// in those frames. With the parameter --without 38,3C Decoder works as in 0.7.0 (constant 38, no 3C)
// and must still produce files identical to the 0.6.0 reference (test_host/golden_060/).
//
// Usage:
//   golden_stream <capture.log> <output_dir> [--decoder] [--without 38,3C] [--stall N,M] [--drop ID]
//                 [--box-report file]
//
// Replay: one step = 1 ms, in the order of the main loop (src/main.cpp):
//   1. CAN frames from this millisecond (common step time),
//   2. middle of the loop: buttons, radio power, RadioLink,
//   3. bytes of line uartA (radio -> box) from this millisecond through xp::Parser to onRadioFrame;
//      at most 96 bytes per step, the rest waits (like RX_BUDGET in src/radio_uart.cpp:25),
//   4. one attempt to send a frame to the radio (10 ms interval),
//   5. sensor sound and derived states.
// Start: the millisecond of the first capture line. End: 5000 ms after the last line (so that the
// ACC drop and the silence after the frames stop are visible).
//
// Three output files, name <capture>[.variant]:
//   .radio_tx.txt   "ms BYTES": frame after xp::encode (2E CMD LEN DATA CHK), in hex;
//   .outputs.txt    "ms name=value": changes of acc / ill / rev (12 V outputs to the radio), pikanie
//                   (beeping in progress), ton_ciagly (continuous tone), ton (speaker playing, with frequency)
//                   and the derived states czujniki (sensors), magistrala (bus), zajete (car busy),
//                   silnik (engine), acc_siec (network ACC);
//   .console.txt    "ms line": console lines from the logic ([park], [car], [key]). When the file would
//                   exceed 2 MB, [park] lines are only counted (the last line of the file gives the count).
//
// Variants:
//   --stall N,M      every N seconds the loop stalls for M ms; CAN frames and radio bytes from that
//                    time land in the first step after the stall with a single timestamp (as after a flash write);
//   --drop ID        every second occurrence of the CAN frame with this ID (hex) is cut out
//                    (the 1st, 3rd, 5th ... remain).
//   --box-report file   writes to the file: how many box frames (line uartB) have in our
//                    stream a frame with the same command and content within a +-1200 ms window. Informational.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "candump.h"
#include "decoder.h"
#include "ref_glue_060.h"
#include "xp_protocol.h"

// ---------- what the harness reads from the code under test after each step ----------
struct StepOut {
    bool acc = false, ill = false, rev = false;          // 12 V outputs to the radio
    bool beeping = false, continuous = false;            // BeeperOutput::active, ::continuous
    bool tone = false;                                   // BeeperOutput::toneOn
    uint16_t freq = 0;
    bool sensorsOn = false, busAlive = false, carBusy = false, engineRunning = false, accForNet = false;
};

// Code under test: RefDut (ref_glue_060.h) and a second implementation on the Decoder class.
struct Dut {
    virtual ~Dut() {}
    virtual void onCan(const CanFrame& f, uint32_t now) = 0;
    virtual void tick(uint32_t now) = 0;
    virtual void onRadio(const xp::Frame& f, uint32_t now) = 0;
    virtual size_t nextTx(uint32_t now, uint8_t* buf, size_t cap) = 0;
    virtual StepOut finish(uint32_t now) = 0;
};

// ---------- console lines ----------
static uint32_t g_now = 0;
static std::vector<std::string> g_console;
static void logLine(const char* line) {
    char pre[16];
    std::snprintf(pre, sizeof(pre), "%lu ", (unsigned long)g_now);
    g_console.push_back(std::string(pre) + line);
}

// Settings states in frame 38 from version 0.6.0 (DATA[1..5]): constants in xp::DoorEncoder in earlier versions,
// now supplied from outside. The 0.6.0 reference gets them here so that it still produces the 0.6.0 golden files.
static const uint8_t SETTINGS_060[5] = {0x02, 0xE1, 0xA9, 0x98, 0x40};
// --without 38,3C: Decoder without frames 38 / 3C from CAN (DecoderConfig::settingsFromCan = false).
static bool g_without3838C = false;

struct RefDut : Dut {
    RefGlue060 g;
    PowerOutputs po;
    RefDut() {
        g.log = logLine;
        g.doors.setSettings(SETTINGS_060);
        // Default key remaps: built into xp::KeyCustomizer in earlier versions, now in the car profile.
        // The reference gets the table of the reference profile (PSA AEE2010) so that it still produces the
        // same golden files.
        g.keyCustom.setDefaults(kCarPsaAee2010.keyRemapDefaults, kCarPsaAee2010.keyRemapDefaultCount);
    }
    void onCan(const CanFrame& f, uint32_t now) override { g.handleFrame(f, now, true); }
    void tick(uint32_t now) override { po = g.loopStep(now, false); }
    void onRadio(const xp::Frame& f, uint32_t now) override { g.onRadioFrame(f, now); }
    size_t nextTx(uint32_t now, uint8_t* buf, size_t cap) override { return g.nextRadioTx(now, true, buf, cap); }
    StepOut finish(uint32_t now) override {
        BeeperOutput b = g.beeperStep(now);
        StepOut o;
        o.acc = po.acc;
        o.ill = po.illumination;
        o.rev = po.reverse;
        o.beeping = b.active;
        o.continuous = b.continuous;
        o.tone = b.toneOn;
        o.freq = b.freqHz;
        o.sensorsOn = g.sensorsOn(now);
        o.busAlive = g.busAlive(now);
        o.carBusy = g.carBusyNow(now);
        o.engineRunning = g.engineRunning(now);
        o.accForNet = g.accForNet(now);
        return o;
    }
};

// Decoder returns a line with a '\n' character at the end (like conPrintf in main.cpp); the golden files do not have it.
static void logLineDecoder(const char* line) {
    std::string s = line;
    if (!s.empty() && s.back() == '\n') s.pop_back();
    logLine(s.c_str());
}

// The same role as RefDut, but through the Decoder class. Fixed inputs as in RefDut:
// transmission to the radio enabled, flashBusy = false, frames from the bus (fromBus = true).
struct DecoderDut : Dut {
    Decoder d;
    DecoderOutputs last;
    DecoderDut() {
        DecoderConfig cfg;
        cfg.decoderMode = true;
        cfg.settingsFromCan = !g_without3838C;
        cfg.logRadioCommands = !g_without3838C;   // the [vset] / [climate] / [sound] lines were added after 0.7.0
        d.begin(cfg, logLineDecoder);
    }
    void onCan(const CanFrame& f, uint32_t now) override { d.onCanFrame(f, now, true); }
    void tick(uint32_t now) override { last = d.tick(now, false); }
    void onRadio(const xp::Frame& f, uint32_t now) override { d.onRadioFrame(f, now); }
    size_t nextTx(uint32_t now, uint8_t* buf, size_t cap) override { return d.nextRadioTx(now, true, buf, cap); }
    StepOut finish(uint32_t) override {
        StepOut o;
        o.acc = last.power.acc;
        o.ill = last.power.illumination;
        o.rev = last.power.reverse;
        o.beeping = last.beep.active;
        o.continuous = last.beep.continuous;
        o.tone = last.beep.toneOn;
        o.freq = last.beep.freqHz;
        o.sensorsOn = last.sensorsOn;
        o.busAlive = last.busAlive;
        o.carBusy = last.carBusy;
        o.engineRunning = last.engineRunning;
        o.accForNet = last.accForNet;
        return o;
    }
};

// ---------- capture ----------
enum EvKind : uint8_t { EV_CAN, EV_UART_A, EV_UART_B };
struct Event {
    uint32_t ms;
    EvKind kind;
    CanFrame can;
    std::vector<uint8_t> bytes;
};

// Time "(123.456789)" -> whole milliseconds (truncated), computed via microseconds so that the result does
// not depend on floating-point rounding.
static uint32_t toMs(double sec) { return (uint32_t)(std::llround(sec * 1e6) / 1000); }

static bool parseUartLine(const char* line, double& ts, EvKind& kind, std::vector<uint8_t>& out) {
    if (line[0] != '(') return false;
    char name[16];
    char hex[400];
    if (std::sscanf(line, "(%lf) %15s %399s", &ts, name, hex) != 3) return false;
    if (std::strcmp(name, "uartA") == 0) kind = EV_UART_A;
    else if (std::strcmp(name, "uartB") == 0) kind = EV_UART_B;
    else return false;
    out.clear();
    size_t n = std::strlen(hex);
    for (size_t i = 0; i + 1 < n; i += 2) {
        char b[3] = {hex[i], hex[i + 1], 0};
        out.push_back((uint8_t)std::strtoul(b, nullptr, 16));
    }
    return true;
}

static std::string hexOf(const uint8_t* d, size_t n) {
    static const char* H = "0123456789ABCDEF";
    std::string s;
    for (size_t i = 0; i < n; i++) {
        s += H[d[i] >> 4];
        s += H[d[i] & 15];
    }
    return s;
}

struct TxRec {
    uint32_t ms;
    uint8_t cmd;
    std::string data;      // DATA content only, in hex
    bool used = false;
};

static bool writeLines(const std::string& path, const std::vector<std::string>& lines, size_t* bytes) {
    FILE* fp = std::fopen(path.c_str(), "wb");   // "wb": no \n to \r\n conversion
    if (!fp) {
        std::fprintf(stderr, "cannot write %s\n", path.c_str());
        return false;
    }
    size_t total = 0;
    for (const auto& l : lines) {
        std::fwrite(l.data(), 1, l.size(), fp);
        std::fputc('\n', fp);
        total += l.size() + 1;
    }
    std::fclose(fp);
    if (bytes) *bytes = total;
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: golden_stream <capture.log> <directory> [--decoder] [--without 38,3C] [--stall N,M] [--drop ID]"
                     " [--box-report file]\n");
        return 2;
    }
    const char* logPath = argv[1];
    std::string outDir = argv[2];
    uint32_t stallEveryMs = 0, stallMs = 0;
    bool drop = false;
    uint32_t dropId = 0;
    const char* reportPath = nullptr;
    bool useDecoder = false;
    std::string variant;
    for (int i = 3; i < argc; i++) {
        if (std::strcmp(argv[i], "--stall") == 0 && i + 1 < argc) {
            unsigned n = 0, m = 0;
            if (std::sscanf(argv[++i], "%u,%u", &n, &m) != 2 || n == 0 || m == 0 || m >= n * 1000) {
                std::fprintf(stderr, "bad --stall (N seconds, M ms, M < N*1000)\n");
                return 2;
            }
            stallEveryMs = n * 1000;
            stallMs = m;
            char v[48];
            std::snprintf(v, sizeof(v), ".stall_%u_%u", n, m);
            variant += v;
        } else if (std::strcmp(argv[i], "--drop") == 0 && i + 1 < argc) {
            dropId = (uint32_t)std::strtoul(argv[++i], nullptr, 16);
            drop = true;
            char v[32];
            std::snprintf(v, sizeof(v), ".drop_%03lX", (unsigned long)dropId);
            variant += v;
        } else if (std::strcmp(argv[i], "--decoder") == 0) {
            useDecoder = true;                   // does not change file names or the summary
        } else if (std::strcmp(argv[i], "--without") == 0 && i + 1 < argc) {
            if (std::strcmp(argv[++i], "38,3C") != 0) {
                std::fprintf(stderr, "--without accepts only 38,3C\n");
                return 2;
            }
            g_without3838C = true;               // only with --decoder; does not change file names
        } else if (std::strcmp(argv[i], "--box-report") == 0 && i + 1 < argc) {
            reportPath = argv[++i];
        } else {
            std::fprintf(stderr, "unknown parameter: %s\n", argv[i]);
            return 2;
        }
    }

    // ---------- loading the capture ----------
    FILE* fp = std::fopen(logPath, "r");
    if (!fp) {
        std::fprintf(stderr, "cannot open %s\n", logPath);
        return 2;
    }
    std::vector<Event> events;
    char line[1024];
    unsigned long canTotal = 0, canDropped = 0, dropSeen = 0, uartABytes = 0;
    while (std::fgets(line, sizeof(line), fp)) {
        Event ev;
        double ts;
        if (parseCandumpLine(line, ev.can, &ts) && ts >= 0) {
            canTotal++;
            if (drop && ev.can.id == dropId) {
                dropSeen++;
                if (dropSeen % 2 == 0) {          // every second occurrence cut out
                    canDropped++;
                    continue;
                }
            }
            ev.kind = EV_CAN;
            ev.ms = toMs(ts);
            events.push_back(ev);
        } else if (parseUartLine(line, ts, ev.kind, ev.bytes)) {
            ev.ms = toMs(ts);
            if (ev.kind == EV_UART_A) uartABytes += (unsigned long)ev.bytes.size();
            events.push_back(ev);
        }
    }
    std::fclose(fp);
    if (events.empty()) {
        std::fprintf(stderr, "capture without frames: %s\n", logPath);
        return 2;
    }

    // Capture name without the directory and without ".log".
    std::string base = logPath;
    size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    if (base.size() > 4 && base.compare(base.size() - 4, 4, ".log") == 0) base.resize(base.size() - 4);
    base += variant;

    // ---------- replay ----------
    static RefDut dutRef;
    static DecoderDut dutDecoder;
    Dut& dut = useDecoder ? (Dut&)dutDecoder : (Dut&)dutRef;

    const uint32_t RX_BUDGET = 96;               // src/radio_uart.cpp:25
    const uint32_t TAIL_MS = 5000;
    uint32_t t0 = events.front().ms;
    uint32_t tEnd = t0;
    for (const auto& e : events)
        if (e.ms > tEnd) tEnd = e.ms;
    tEnd += TAIL_MS;

    std::vector<std::string> txLines, outLines;
    std::vector<TxRec> ours, box;
    std::map<int, unsigned long> ourCount;
    std::deque<uint8_t> rxFifo;                  // bytes from the radio waiting in the port buffer
    xp::Parser radioParser, boxParser;
    unsigned long radioFrames = 0, radioFramesLow = 0, steps = 0, stalls = 0;
    StepOut prev;
    size_t next = 0;
    char buf[96];

    for (uint32_t ms = t0; ms <= tEnd; ms++) {
        if (stallEveryMs) {
            uint32_t rel = ms - t0;
            if (rel >= stallEveryMs && rel % stallEveryMs < stallMs) {
                if (rel % stallEveryMs == 0) stalls++;
                continue;                        // the loop is stalled: events wait for the first step after the stall
            }
        }
        g_now = ms;
        steps++;

        // 1. CAN frames that arrived up to this millisecond; UART bytes into the queues
        size_t first = next;
        while (next < events.size() && events[next].ms <= ms) next++;
        for (size_t i = first; i < next; i++)
            if (events[i].kind == EV_CAN) dut.onCan(events[i].can, ms);
        for (size_t i = first; i < next; i++) {
            const Event& e = events[i];
            if (e.kind == EV_UART_A) {
                for (uint8_t b : e.bytes) rxFifo.push_back(b);
            } else if (e.kind == EV_UART_B) {    // box line: for the report only, with the time from the capture
                for (uint8_t b : e.bytes) {
                    xp::Frame f;
                    if (boxParser.feed(b, e.ms, f) == xp::Parser::FRAME)
                        box.push_back({e.ms, f.cmd, hexOf(f.data, f.len)});
                }
            }
        }

        // 2. middle of the loop
        dut.tick(ms);

        // 3. bytes from the radio
        uint32_t budget = RX_BUDGET;
        while (budget-- > 0 && !rxFifo.empty()) {
            uint8_t b = rxFifo.front();
            rxFifo.pop_front();
            xp::Frame f;
            if (radioParser.feed(b, ms, f) == xp::Parser::FRAME) {
                radioFrames++;
                if (f.cmd < 0x80) radioFramesLow++;
                dut.onRadio(f, ms);
            }
        }

        // 4. one send attempt
        uint8_t tx[xp::MAX_DATA + 4];            // like the buffer in src/radio_uart.cpp:79
        size_t n = dut.nextTx(ms, tx, sizeof(tx));
        if (n > 0) {
            std::snprintf(buf, sizeof(buf), "%lu ", (unsigned long)ms);
            txLines.push_back(std::string(buf) + hexOf(tx, n));
            if (n >= 4) {
                ours.push_back({ms, tx[1], hexOf(tx + 3, n - 4)});
                ourCount[tx[1]]++;
            }
        }

        // 5. sound and derived states; only changes go to the file
        StepOut o = dut.finish(ms);
        auto change = [&](const char* name, bool was, bool is) {
            if (was == is) return;
            std::snprintf(buf, sizeof(buf), "%lu %s=%u", (unsigned long)ms, name, is ? 1 : 0);
            outLines.push_back(buf);
        };
        change("acc", prev.acc, o.acc);
        change("ill", prev.ill, o.ill);
        change("rev", prev.rev, o.rev);
        // The names below (also "ton") are the format of the .outputs.txt references, recorded in
        // Polish: beeping, continuous tone, tone, sensors, bus, car busy, engine, ACC for the network. They stay,
        // so that those files keep their bytes (the proof that the 12 V outputs behave the same).
        change("pikanie", prev.beeping, o.beeping);
        change("ton_ciagly", prev.continuous, o.continuous);
        if (o.tone != prev.tone || (o.tone && o.freq != prev.freq)) {
            if (o.tone) std::snprintf(buf, sizeof(buf), "%lu ton=1 %uHz", (unsigned long)ms, o.freq);
            else std::snprintf(buf, sizeof(buf), "%lu ton=0", (unsigned long)ms);
            outLines.push_back(buf);
        }
        change("czujniki", prev.sensorsOn, o.sensorsOn);
        change("magistrala", prev.busAlive, o.busAlive);
        change("zajete", prev.carBusy, o.carBusy);
        change("silnik", prev.engineRunning, o.engineRunning);
        change("acc_siec", prev.accForNet, o.accForNet);
        prev = o;
    }

    // ---------- console: 2 MB limit ----------
    size_t conBytes = 0;
    unsigned long parkLines = 0;
    for (const auto& l : g_console) conBytes += l.size() + 1;
    bool parkCounted = conBytes > 2u * 1024u * 1024u;
    if (parkCounted) {
        std::vector<std::string> kept;
        for (const auto& l : g_console) {
            if (l.find(" [park] ") != std::string::npos) parkLines++;
            else kept.push_back(l);
        }
        std::snprintf(buf, sizeof(buf), "# [park] lines counted, not written: %lu", parkLines);
        kept.push_back(buf);
        g_console.swap(kept);
    }

    // ---------- files ----------
    size_t bTx = 0, bOut = 0, bCon = 0;
    std::string stem = outDir + "/" + base;
    if (!writeLines(stem + ".radio_tx.txt", txLines, &bTx)) return 2;
    if (!writeLines(stem + ".outputs.txt", outLines, &bOut)) return 2;
    if (!writeLines(stem + ".console.txt", g_console, &bCon)) return 2;

    // ---------- summary (stdout, compared with the golden file by run_host_tests.sh) ----------
    std::printf("%s: steps %lu, stalls %lu, CAN frames %lu (dropped %lu), bytes from the radio %lu\n",
                base.c_str(), steps, stalls, canTotal - canDropped, canDropped, uartABytes);
    const xp::ParserStats& rs = radioParser.stats();
    std::printf("  from the radio: frames %lu (with a command number < 80: %lu), bad checksum %lu, bad length %lu, truncated %lu\n",
                radioFrames, radioFramesLow, (unsigned long)rs.badChecksum, (unsigned long)rs.badLength,
                (unsigned long)rs.timeouts);
    std::printf("  to the radio: frames %zu:", txLines.size());
    for (const auto& kv : ourCount) std::printf(" %02X=%lu", kv.first, kv.second);
    std::printf("\n  outputs: %zu lines, console: %zu lines%s\n", outLines.size(), g_console.size(),
                parkCounted ? " ([park] counted)" : "");

    // ---------- report: box frames vs our stream (informational) ----------
    if (reportPath) {
        FILE* rp = std::fopen(reportPath, "ab");
        if (!rp) {
            std::fprintf(stderr, "cannot write %s\n", reportPath);
            return 2;
        }
        const long WINDOW_MS = 1200;
        std::map<int, unsigned long> boxTotal, boxMatch;
        for (const auto& b : box) {
            boxTotal[b.cmd]++;
            // Our closest-in-time frame with the same command and content, not yet used.
            long best = -1, bestDt = WINDOW_MS + 1;
            for (size_t i = 0; i < ours.size(); i++) {
                const TxRec& o = ours[i];
                long dt = (long)o.ms - (long)b.ms;
                if (dt > WINDOW_MS) break;
                if (dt < -WINDOW_MS || o.used || o.cmd != b.cmd || o.data != b.data) continue;
                long a = dt < 0 ? -dt : dt;
                if (a < bestDt) {
                    bestDt = a;
                    best = (long)i;
                }
            }
            if (best >= 0) {
                ours[(size_t)best].used = true;
                boxMatch[b.cmd]++;
            }
        }
        std::map<int, unsigned long> oursFree;
        for (const auto& o : ours)
            if (!o.used) oursFree[o.cmd]++;
        std::fprintf(rp, "### %s: box frames %zu, ours %zu, window +-%ld ms\n", base.c_str(), box.size(),
                     ours.size(), WINDOW_MS);
        std::fprintf(rp, "command  box  with_counterpart  ours  ours_unpaired\n");
        std::map<int, int> cmds;
        for (const auto& kv : boxTotal) cmds[kv.first] = 1;
        for (const auto& kv : ourCount) cmds[kv.first] = 1;
        for (const auto& kv : cmds) {
            int c = kv.first;
            std::fprintf(rp, "%02X       %8lu  %16lu  %5lu  %14lu\n", c, boxTotal.count(c) ? boxTotal[c] : 0,
                         boxMatch.count(c) ? boxMatch[c] : 0, ourCount.count(c) ? ourCount[c] : 0,
                         oursFree.count(c) ? oursFree[c] : 0);
        }
        std::fclose(rp);
    }
    return 0;
}
