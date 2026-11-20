// Host test: parsing of radio commands 80 / 8A / 84 / 90 / 81 (lib/radio_xp/radio_commands.h) and the console
// lines "[vset]", "[climate]", "[sound]" from the Decoder class.
//
//   - each command with a correct and a too short length (too short = NONE, no reading of bytes outside the frame);
//   - names from the climate and sound tables of the settings map (vehicles/peugeot-508-sw-2015/settings-map.md;
//     climate: for 4, 5, 10: 1 = plus, 2 = minus);
//   - Decoder: one line per command 80 / 8A / 84, none for too short frames and other commands,
//     queue to the radio unchanged (a command sends nothing to the radio or on CAN).
//
// Run: bash tools/run_host_tests.sh (README.md, section on tests on recordings).
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "decoder.h"
#include "radio_commands.h"
#include "xp_protocol.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

static xp::Frame frame(uint8_t cmd, std::initializer_list<uint8_t> d) {
    xp::Frame f;
    f.cmd = cmd;
    f.len = (uint8_t)d.size();
    uint8_t i = 0;
    for (uint8_t b : d) f.data[i++] = b;
    return f;
}

// Frame with data in the buffer but a shorter length: the parser must not reach bytes outside len.
static xp::Frame shortFrame(uint8_t cmd, uint8_t len) {
    xp::Frame f = frame(cmd, {0x01, 0x01, 0x01});
    f.len = len;
    return f;
}

static bool is(const RadioCmd& c, RadioCmdKind k, uint8_t id, uint8_t value) {
    return c.kind == k && c.id == id && c.value == value;
}

static void testParse() {
    std::printf("--- parseRadioCmd ---\n");
    check(is(parseRadioCmd(frame(0x80, {0x01, 0x01})), RadioCmdKind::SETTING, 0x01, 0x01), "80 01 01 -> SETTING 01 = 1");
    check(is(parseRadioCmd(frame(0x80, {0x0B, 0x08, 0x55})), RadioCmdKind::SETTING, 0x0B, 0x08), "80 0B 08 55 (longer) -> SETTING 0B = 8");
    check(is(parseRadioCmd(frame(0x8A, {0x04, 0x02})), RadioCmdKind::AIR, 0x04, 0x02), "8A 04 02 -> AIR 04 = 2");
    check(is(parseRadioCmd(frame(0x84, {0x03, 0x05})), RadioCmdKind::SOUND, 0x03, 0x05), "84 03 05 -> SOUND 03 = 5");
    check(is(parseRadioCmd(frame(0x90, {0x71})), RadioCmdKind::REQUEST, 0x71, 0), "90 71 -> REQUEST 71");
    check(is(parseRadioCmd(frame(0x90, {0x21, 0x00, 0x00, 0x00})), RadioCmdKind::REQUEST, 0x21, 0), "90 21 00 00 00 -> REQUEST 21");
    check(is(parseRadioCmd(frame(0x81, {0x01})), RadioCmdKind::RADIO_ON, 0, 1), "81 01 -> RADIO_ON");
    check(is(parseRadioCmd(frame(0x81, {0x00})), RadioCmdKind::RADIO_OFF, 0, 0), "81 00 -> RADIO_OFF");
    check(parseRadioCmd(frame(0x81, {0x02})).kind == RadioCmdKind::NONE, "81 02 -> NONE");
    bool ok = true;
    for (uint8_t cmd : {0x80, 0x8A, 0x84})
        for (uint8_t len = 0; len < 2; len++)
            if (!is(parseRadioCmd(shortFrame(cmd, len)), RadioCmdKind::NONE, 0, 0)) ok = false;
    check(ok, "80 / 8A / 84 with length 0 and 1 -> NONE (number and value not read)");
    check(is(parseRadioCmd(shortFrame(0x90, 0)), RadioCmdKind::NONE, 0, 0) && is(parseRadioCmd(shortFrame(0x81, 0)), RadioCmdKind::NONE, 0, 0),
          "90 and 81 with length 0 -> NONE");
    ok = true;
    for (uint8_t cmd : {0xA6, 0xC0, 0xC2, 0xC4, 0xC5, 0xCA, 0xCB, 0x8F, 0x20, 0x38, 0x00, 0xFF})
        if (parseRadioCmd(frame(cmd, {0x01, 0x01})).kind != RadioCmdKind::NONE) ok = false;
    check(ok, "other commands (A6, C0, C2, C4, C5, CA, CB, 8F, 20, 38, 00, FF) -> NONE");
}

static void testNames() {
    std::printf("--- names ---\n");
    for (uint8_t n = 0; n <= 13; n++) {
        const char* s = airCommandName(n);
        std::printf("  8A %02X: %s%s\n", n, s ? s : "(none)", airIsPlusMinus(n) ? " [1 = plus, 2 = minus]" : "");
    }
    check(airCommandName(0) == nullptr && airCommandName(13) == nullptr && airCommandName(0xFF) == nullptr, "8A 00, 0D, FF: no name");
    bool ok = true;
    for (uint8_t n = 1; n <= 12; n++)
        if (!airCommandName(n)) ok = false;
    check(ok && airIsPlusMinus(4) && airIsPlusMinus(5) && airIsPlusMinus(10) && !airIsPlusMinus(9) && !airIsPlusMinus(12),
          "8A 01-0C have names; plus / minus only for 4, 5, 10");
    for (uint8_t n = 0; n <= 11; n++) {
        const char* s = soundCommandName(n);
        std::printf("  84 %02X: %s\n", n, s ? s : "(none)");
    }
    check(soundCommandName(3) && std::strcmp(soundCommandName(3), "fader") == 0 && !soundCommandName(0) && !soundCommandName(1) &&
              !soundCommandName(7) && !soundCommandName(8) && !soundCommandName(11),
          "84: names for 2-6, 9, 10 (recorded 19.11.2026); none for 0, 1, 7, 8, 11");
}

static std::vector<std::string> g_lines;
static void logLine(const char* line) { g_lines.push_back(line); }

static void testDecoderLines() {
    std::printf("--- console lines from Decoder::onRadioFrame ---\n");
    Decoder dec;
    DecoderConfig cfg;
    dec.begin(cfg, logLine);
    uint32_t now = 1000;
    const xp::Frame in[] = {
        frame(0x80, {0x01, 0x01}), frame(0x80, {0x01, 0x00}), frame(0x80, {0x02, 0x01}), frame(0x80, {0x06, 0x03}),
        frame(0x80, {0x09, 0x04}), frame(0x80, {0x0E, 0x01}), frame(0x80, {0x04, 0x05}), frame(0x80, {0x0B, 0x08}),
        frame(0x80, {0x05, 0x01}), frame(0x80, {0x0C, 0x00}), frame(0x80, {0x0D, 0x01}), frame(0x80, {0x10, 0x01}),
        frame(0x80, {0x14, 0x01}), frame(0x80, {0x1F, 0x01}), frame(0x80, {0x77, 0x02}),
        frame(0x8A, {0x04, 0x01}), frame(0x8A, {0x04, 0x02}), frame(0x8A, {0x05, 0x01}), frame(0x8A, {0x0A, 0x02}),
        frame(0x8A, {0x04, 0x03}), frame(0x8A, {0x0B, 0x00}), frame(0x8A, {0x01, 0x01}), frame(0x8A, {0x09, 0x02}),
        frame(0x8A, {0x0C, 0x01}), frame(0x8A, {0x30, 0x01}),
        frame(0x84, {0x03, 0x05}), frame(0x84, {0x0A, 0x01}), frame(0x84, {0x07, 0x01}),
    };
    for (const xp::Frame& f : in) dec.onRadioFrame(f, now++);
    for (const std::string& s : g_lines) std::printf("  %s", s.c_str());
    check(g_lines.size() == sizeof(in) / sizeof(in[0]), "one line for every command 80 / 8A / 84");
    check(g_lines.size() > 25 &&
              g_lines[0] == "[vset] radio: 80 01 01 parking assist = 1: REFUSED (no CAN transmit in this build)\n" &&
              g_lines[10] == "[vset] radio: 80 0D 01 Start/Stop = 1: no known frame\n" &&
              g_lines[15] == "[climate] radio: 8A 04 01 temperature left plus (no action)\n" &&
              g_lines[25] == "[sound] radio: 84 03 05 fader = 5 (no action)\n",
          "four lines of the design pattern (item 2.9), character for character");
    bool ok = true;
    for (size_t i = 0; i < 15 && i < g_lines.size(); i++) {
        const std::string& s = g_lines[i];
        bool a = s.find(": REFUSED (no CAN transmit in this build)\n") != std::string::npos;
        bool b = s.find(": no known frame\n") != std::string::npos;
        bool c = s.find(": known frame (1A9), transmit blocked until measured\n") != std::string::npos;
        if ((a ? 1 : 0) + (b ? 1 : 0) + (c ? 1 : 0) != 1) ok = false;
        if (c != (i == 8 || i == 9)) ok = false;                // only 80 05 and 80 0C
    }
    check(ok, "every command 80 ends with a refusal: 'REFUSED (no CAN transmit in this build)', 'no known frame' or (05, 0C) 'known frame (1A9)...'");

    // Too short frames and other commands: no line.
    size_t before = g_lines.size();
    dec.onRadioFrame(shortFrame(0x80, 1), now++);
    dec.onRadioFrame(shortFrame(0x8A, 1), now++);
    dec.onRadioFrame(shortFrame(0x84, 0), now++);
    dec.onRadioFrame(frame(0x81, {0x00}), now++);
    dec.onRadioFrame(frame(0x90, {0x21, 0x00, 0x00, 0x00}), now++);
    dec.onRadioFrame(frame(0xC4, {0x10}), now++);
    dec.onRadioFrame(frame(0xA6, {26, 11, 17, 21, 30}), now++);
    check(g_lines.size() == before, "frames too short and 81 00, 90 21, C4, A6: no line");

    // The commands send nothing to the radio (ACC off blocks anyway; we check the queue itself).
    check(dec.link().txQueued() == 0, "after all commands the queue to the radio is empty");
    check(dec.link().info().settingCount == 15 && dec.link().info().airCount == 10,
          "RadioLink still counts commands as before: 15 settings, 10 climate");

    // Data for the panel: the last command 80 and the counters. The last one was 80 77 02 at 1014 ms;
    // commands 8A / 84, too short frames and others change nothing here.
    const SettingCmdStats& sc = dec.settingCmds();
    check(sc.haveLast && sc.lastId == 0x77 && sc.lastValue == 0x02 && sc.lastMs == 1014 &&
              sc.lastResult == SettingCmdResult::NO_FRAME,
          "settingCmds: last command 80 77 02 at 1014 ms, result no_frame");
    check(sc.refused == 15 && sc.accepted == 0 && sc.notAccepted == 0, "settingCmds: 15 refused, 0 accepted, 0 not accepted");
    dec.onRadioFrame(frame(0x80, {0x01, 0x01}), 5000);
    check(dec.settingCmds().lastId == 0x01 && dec.settingCmds().lastValue == 1 && dec.settingCmds().lastMs == 5000 &&
              dec.settingCmds().lastResult == SettingCmdResult::REFUSED && dec.settingCmds().refused == 16,
          "settingCmds: 80 01 01 -> refused, counter 16");
    dec.onRadioFrame(frame(0x80, {0x0C, 0x00}), 5001);
    check(dec.settingCmds().lastResult == SettingCmdResult::BLOCKED && dec.settingCmds().refused == 17,
          "settingCmds: 80 0C 00 -> blocked, counter 17");
    check(std::strcmp(settingCmdResultName(SettingCmdResult::REFUSED), "refused") == 0 &&
              std::strcmp(settingCmdResultName(SettingCmdResult::NO_FRAME), "no_frame") == 0 &&
              std::strcmp(settingCmdResultName(SettingCmdResult::BLOCKED), "blocked") == 0,
          "result names for JSON: refused, no_frame, blocked");
    Decoder fresh;
    fresh.begin(cfg, logLine);
    check(!fresh.settingCmds().haveLast && fresh.settingCmds().refused == 0, "settingCmds after startup: no command, counter 0");
    g_lines.resize(g_lines.size() - 2);     // two "[vset]" lines from the commands above: excluded from the counting below

    // logRadioCommands = false (only the golden_stream --without 38,3C test): no line.
    Decoder quiet;
    DecoderConfig qc;
    qc.logRadioCommands = false;
    quiet.begin(qc, logLine);
    before = g_lines.size();
    quiet.onRadioFrame(frame(0x80, {0x01, 0x01}), 1);
    quiet.onRadioFrame(frame(0x8A, {0x04, 0x01}), 2);
    check(g_lines.size() == before, "logRadioCommands = false: no lines");
}

int main() {
    testParse();
    testNames();
    testDecoderLines();
    std::printf("radio_commands: %d checks, %d errors\n", checks, failures);
    return failures ? 1 : 0;
}
