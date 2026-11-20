// Host test: the old (Polish) console command name "ust" still works as an alias of "vset"
// (lib/core/can_tx_arm.h, cantx::parse). Also the USB line assembly (lib/core/line_assembler.h): an overlong
// line is dropped whole, never executed cut off.
//
// The other aliases (wifi showpass / haslo, wifi newpass / nowehaslo) are parsed in src/
// (console_cmds.cpp, net/net.cpp) and have no test on the PC: they are checked on the bench.
//
// TRANSMITS NOTHING: pure functions only, no hardware.
//
// Run: bash tools/run_host_tests.sh
#include <cstdio>
#include <cstring>

#include "can_tx_arm.h"
#include "line_assembler.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

// The alias gives exactly the same result as the English command.
static bool same(const char* english, const char* alias) {
    const cantx::Parsed a = cantx::parse(english);
    const cantx::Parsed b = cantx::parse(alias);
    return a.cmd == b.cmd && a.id == b.id && a.value == b.value;
}

int main() {
    std::printf("--- vset and its old name ust ---\n");
    check(same("vset on 02", "ust on 02") && cantx::parse("ust on 02").cmd == cantx::Cmd::MASK_ON &&
              cantx::parse("ust on 02").id == 0x02,
          "ust on 02 = vset on 02 (MASK_ON 02)");
    check(same("vset off 0F", "ust off 0F") && cantx::parse("ust off 0F").cmd == cantx::Cmd::MASK_OFF,
          "ust off 0F = vset off 0F (MASK_OFF 0F)");
    check(same("vset on  9", "ust on  9") && same("vset  on 0f", "ust  on 0f"), "extra spaces: the same for both names");
    check(same("vset", "ust") && cantx::parse("ust").cmd == cantx::Cmd::NONE, "bare ust = bare vset (NONE: settings table)");
    check(same("vset on", "ust on") && cantx::parse("ust on").cmd == cantx::Cmd::BAD &&
              cantx::parse("ust on 123").cmd == cantx::Cmd::BAD && cantx::parse("ust 02").cmd == cantx::Cmd::BAD,
          "malformed ust: BAD like vset");
    check(cantx::parse("ustx on 02").cmd == cantx::Cmd::NONE && cantx::parse("vsetust on 02").cmd == cantx::Cmd::NONE &&
              cantx::parse("UST on 02").cmd == cantx::Cmd::NONE,
          "ustx, vsetust, UST: not a command");
    check(cantx::allowed(cantx::parse("ust on 02").cmd, cantx::Source::PANEL_AP) ==
              cantx::allowed(cantx::parse("vset on 02").cmd, cantx::Source::PANEL_AP) &&
              cantx::allowed(cantx::parse("ust on 02").cmd, cantx::Source::PANEL_OTHER) ==
              cantx::allowed(cantx::parse("vset on 02").cmd, cantx::Source::PANEL_OTHER),
          "who may: the same rights for ust and vset");
    // USB line assembly (lib/core/line_assembler.h). Review: a line longer than the buffer was cut and executed.
    std::printf("--- console line assembly ---\n");
    {
        char buf[8];                                 // room for 7 characters
        LineAssembler a(buf, sizeof(buf));
        int lines = 0, tooLong = 0;
        const char* in = "stat\r\nvol 50\n";
        for (const char* p = in; *p; p++) {
            LineAssembler::Result r = a.feed(*p);
            if (r == LineAssembler::LINE) {
                lines++;
                if (lines == 1) check(std::strcmp(a.buffer(), "stat") == 0, "line 'stat' ends at CR");
                if (lines == 3) check(std::strcmp(a.buffer(), "vol 50") == 0, "line 'vol 50' (7 characters fit)");
            }
            if (r == LineAssembler::TOO_LONG) tooLong++;
        }
        check(lines == 3 && tooLong == 0, "CR LF: 'stat', an empty line, 'vol 50'");
        lines = tooLong = 0;
        const char* over = "0E1#B0F000FC\nstat\n";        // 12 characters: longer than 7
        for (const char* p = over; *p; p++) {
            LineAssembler::Result r = a.feed(*p);
            if (r == LineAssembler::LINE) {
                lines++;
                check(std::strcmp(a.buffer(), "stat") == 0, "after an overlong line the next line is whole");
            }
            if (r == LineAssembler::TOO_LONG) tooLong++;
        }
        check(tooLong == 1 && lines == 1, "an overlong line is reported once and never handed over cut off");
        const char* exact = "1234567\n";             // exactly 7 characters
        bool got = false;
        for (const char* p = exact; *p; p++)
            if (a.feed(*p) == LineAssembler::LINE) got = std::strcmp(a.buffer(), "1234567") == 0;
        check(got, "a line of exactly cap - 1 characters is accepted");
        const char* eight = "12345678\n";
        LineAssembler::Result last = LineAssembler::NONE;
        for (const char* p = eight; *p; p++) last = a.feed(*p);
        check(last == LineAssembler::TOO_LONG, "cap characters: too long");
    }
    std::printf("console_alias: %d checks, %d errors\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
