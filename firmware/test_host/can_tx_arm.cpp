// Host test: arming CAN transmission, parsing of console commands and the settings item mask
// (lib/core/can_tx_arm.h, vset::maskChange / sanitizeMask).
//
// TRANSMITS NOTHING: pure functions only, no hardware.
//
// Run: bash tools/run_host_tests.sh (README.md, section on tests on recordings).
#include <cstdio>
#include <cstring>

#include "can_tx_arm.h"
#include "vehicle_settings.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

static void testTag() {
    std::printf("--- arming tied to the build string ---\n");
    const char* build = "Nov 18 2026 01:33:28";
    check(cantx::armTagValid("Nov 18 2026 01:33:28", build), "same string: armed");
    check(!cantx::armTagValid("Nov 18 2026 01:33:29", build), "different build time (1 s): disarmed");
    check(!cantx::armTagValid("Nov 18 2026 01:33:2", build) && !cantx::armTagValid("Nov 18 2026 01:33:288", build),
          "string one character shorter and longer: disarmed");
    check(!cantx::armTagValid("", build) && !cantx::armTagValid(nullptr, build) && !cantx::armTagValid(build, nullptr) &&
              !cantx::armTagValid("", ""),
          "nothing stored, empty string, no program string: disarmed");
    check(!cantx::armTagValid("1", build) && !cantx::armTagValid("on", build) && !cantx::armTagValid("true", build),
          "record \"1\", \"on\", \"true\" (not the build string): disarmed");
    // Field without a terminating zero (24 chars): untrusted, disarmed, no reads past the field.
    char full[cantx::BUILD_TAG_MAX];
    std::memset(full, 'A', sizeof(full));
    char full2[cantx::BUILD_TAG_MAX];
    std::memset(full2, 'A', sizeof(full2));
    check(!cantx::armTagValid(full, full2), "string without a terminating zero in 24 bytes: disarmed");
    char longest[cantx::BUILD_TAG_MAX];
    std::memset(longest, 'B', sizeof(longest));
    longest[cantx::BUILD_TAG_MAX - 1] = 0;
    check(cantx::armTagValid(longest, longest), "longest valid string (23 characters): armed");
    check(!cantx::armTagValid("nov 18 2026 01:33:28", build), "letter case differs: disarmed");
}

static bool is(const char* line, cantx::Cmd cmd, uint8_t id = 0, uint8_t value = 0) {
    cantx::Parsed p = cantx::parse(line);
    return p.cmd == cmd && (cmd == cantx::Cmd::BAD || cmd == cantx::Cmd::NONE || (p.id == id && p.value == value));
}

static void testParse() {
    std::printf("--- command parsing ---\n");
    check(is("cantx", cantx::Cmd::STATUS) && is("cantx on", cantx::Cmd::ARM_ON) && is("cantx off", cantx::Cmd::ARM_OFF) &&
              is("cantx reset", cantx::Cmd::RESET),
          "cantx, cantx on, cantx off, cantx reset");
    check(is("cantx  on", cantx::Cmd::ARM_ON) && is("cantx onn", cantx::Cmd::BAD) && is("cantx 1", cantx::Cmd::BAD) &&
              is("cantx on 1", cantx::Cmd::BAD) && is("cantx ", cantx::Cmd::BAD),
          "malformed cantx (onn, 1, on 1, only a space): BAD, not arming");
    check(is("cantxon", cantx::Cmd::NONE) && is("CANTX ON", cantx::Cmd::NONE) && is("xcantx on", cantx::Cmd::NONE) &&
              is("", cantx::Cmd::NONE) && is(nullptr, cantx::Cmd::NONE),
          "cantxon, CANTX ON, xcantx on, empty line: not a command");
    check(is("vset", cantx::Cmd::NONE), "bare vset (settings table): NONE");
    check(is("vset on 02", cantx::Cmd::MASK_ON, 0x02) && is("vset off 0F", cantx::Cmd::MASK_OFF, 0x0F) &&
              is("vset on 9", cantx::Cmd::MASK_ON, 0x09) && is("vset on 0f", cantx::Cmd::MASK_ON, 0x0F),
          "vset on 02, vset off 0F, vset on 9, vset on 0f (number in hexadecimal)");
    check(is("vset on", cantx::Cmd::BAD) && is("vset on 123", cantx::Cmd::BAD) && is("vset on 0G", cantx::Cmd::BAD) &&
              is("vset onn 02", cantx::Cmd::BAD) && is("vset 02", cantx::Cmd::BAD) && is("vset on 02 03", cantx::Cmd::BAD) &&
              is("vset off", cantx::Cmd::BAD),
          "vset on without a number, 123, 0G, onn, without on/off, two numbers: BAD");
    check(is("set 01 01", cantx::Cmd::SET, 0x01, 0x01) && is("set 9 4", cantx::Cmd::SET, 0x09, 0x04) &&
              is("set 0F 00", cantx::Cmd::SET, 0x0F, 0x00),
          "set 01 01, set 9 4, set 0F 00");
    check(is("set", cantx::Cmd::BAD) && is("set 01", cantx::Cmd::BAD) && is("set 01 ", cantx::Cmd::BAD) &&
              is("set 01 01 01", cantx::Cmd::BAD) && is("set 100 1", cantx::Cmd::BAD) && is("set 01 1x", cantx::Cmd::BAD),
          "set without a value, with three numbers, 100, 1x: BAD");
    check(is("settings", cantx::Cmd::NONE) && is("stat", cantx::Cmd::NONE) && is("15B#0034", cantx::Cmd::NONE) &&
              is("vsetx", cantx::Cmd::NONE),
          "settings, stat, frame 15B#..., vsetx: not a command");
}

static void testAllowed() {
    std::printf("--- who may do what ---\n");
    using cantx::Cmd;
    using cantx::Source;
    const Cmd all[] = {Cmd::NONE, Cmd::STATUS, Cmd::ARM_ON, Cmd::ARM_OFF, Cmd::RESET, Cmd::MASK_ON, Cmd::MASK_OFF, Cmd::SET, Cmd::BAD};
    bool usb = cantx::allowed(Cmd::STATUS, Source::USB) && cantx::allowed(Cmd::ARM_ON, Source::USB) &&
               cantx::allowed(Cmd::ARM_OFF, Source::USB) && cantx::allowed(Cmd::RESET, Source::USB) &&
               cantx::allowed(Cmd::MASK_ON, Source::USB) && cantx::allowed(Cmd::MASK_OFF, Source::USB) &&
               cantx::allowed(Cmd::SET, Source::USB);
    check(usb, "USB: state, arming, disarming, reset, mask, set");
    bool ap = cantx::allowed(Cmd::STATUS, Source::PANEL_AP) && cantx::allowed(Cmd::ARM_ON, Source::PANEL_AP) &&
              cantx::allowed(Cmd::ARM_OFF, Source::PANEL_AP) && cantx::allowed(Cmd::MASK_ON, Source::PANEL_AP) &&
              cantx::allowed(Cmd::MASK_OFF, Source::PANEL_AP) && !cantx::allowed(Cmd::RESET, Source::PANEL_AP) &&
              !cantx::allowed(Cmd::SET, Source::PANEL_AP);
    check(ap, "panel on the device's own network: state, arming, disarming, mask; reset and set NOT");
    int otherAllowed = 0;
    for (Cmd c : all)
        if (cantx::allowed(c, Source::PANEL_OTHER)) otherAllowed++;
    check(otherAllowed == 1 && cantx::allowed(Cmd::STATUS, Source::PANEL_OTHER),
          "panel through the home network: only reading the state (1 command of 9)");
    check(!cantx::allowed(Cmd::NONE, Source::USB) && !cantx::allowed(Cmd::BAD, Source::USB), "NONE and BAD: never");
    int changing = 0;
    for (Cmd c : all)
        if (cantx::changesState(c)) changing++;
    check(changing == 6 && !cantx::changesState(Cmd::STATUS) && !cantx::changesState(Cmd::NONE) && !cantx::changesState(Cmd::BAD),
          "commands that change the state: 6 (on, off, reset, vset on, vset off, set)");
}

static void testMask() {
    std::printf("--- item mask ---\n");
    const uint32_t sendable = vset::sendableMask();
    char line[160];
    std::snprintf(line, sizeof(line), "items that can be enabled: mask 0x%08lX (01, 02, 05, 06, 07, 09, 0C, 0F)", (unsigned long)sendable);
    check(sendable == 0x000092E6UL, line);
    check(vset::ENABLED_DEFAULT == (1UL << 1) && (vset::ENABLED_DEFAULT & ~sendable) == 0, "default mask = item 01 only");
    uint32_t m = 0;
    bool on = vset::maskChange(vset::ENABLED_DEFAULT, 0x02, true, m) == vset::MaskResult::OK && m == 0x00000006UL;
    uint32_t m2 = 0;
    bool off = vset::maskChange(m, 0x01, false, m2) == vset::MaskResult::OK && m2 == 0x00000004UL;
    check(on && off, "vset on 02: mask 0x06; vset off 01: mask 0x04");
    bool same = vset::maskChange(m, 0x02, true, m2) == vset::MaskResult::OK && m2 == m;
    check(same, "enabling an enabled item: unchanged");
    // Groups D and E: cannot be enabled; mask unchanged.
    const uint8_t fixed[] = {0x15, 0x18, 0x1A, 0x0A, 0x0B, 0x10, 0x03, 0x14, 0x16, 0x19, 0x1F, 0x0D};
    bool refused = true;
    for (uint8_t id : fixed) {
        uint32_t out = 0xDEAD;
        if (vset::maskChange(0x02, id, true, out) != vset::MaskResult::NOT_SENDABLE || out != 0x02) refused = false;
    }
    check(refused, "12 items of groups D and E (15, 18, 1A, 0A, 0B, 10, 03, 14, 16, 19, 1F, 0D): NOT_SENDABLE");
    uint32_t out = 0;
    check(vset::maskChange(0x02, 0x11, true, out) == vset::MaskResult::UNKNOWN_ID && out == 0x02 &&
              vset::maskChange(0x02, 0x00, true, out) == vset::MaskResult::UNKNOWN_ID &&
              vset::maskChange(0x02, 0xFF, true, out) == vset::MaskResult::UNKNOWN_ID &&
              vset::maskChange(0x02, 0x04, true, out) == vset::MaskResult::UNKNOWN_ID &&
              vset::maskChange(0x02, 0x08, true, out) == vset::MaskResult::UNKNOWN_ID &&
              vset::maskChange(0x02, 0x0E, true, out) == vset::MaskResult::UNKNOWN_ID && out == 0x02,
          "numbers outside the table (11, 00, FF; 04, 08, 0E removed on 19.11.2026): UNKNOWN_ID");
    check(vset::sanitizeMask(0xFFFFFFFFUL) == sendable && vset::sanitizeMask(0) == 0 &&
              vset::sanitizeMask((1UL << 0x04) | (1UL << 0x15) | (1UL << 1)) == (1UL << 1),
          "mask from flash: bits of items that cannot be sent are cleared (FFFFFFFF -> sendable only)");
    // Every enableable item can be enabled and disabled; nothing outside its bit changes.
    bool each = true;
    for (uint8_t id = 0; id < 32; id++) {
        if (!(sendable & (1UL << id))) continue;
        uint32_t a = 0, b = 0;
        if (vset::maskChange(0, id, true, a) != vset::MaskResult::OK || a != (1UL << id)) each = false;
        if (vset::maskChange(sendable, id, false, b) != vset::MaskResult::OK || b != (sendable & ~(1UL << id))) each = false;
    }
    check(each, "each of the 8 enableable items: enabling and disabling changes only its bit");
}

int main() {
    testTag();
    testParse();
    testAllowed();
    testMask();
    std::printf("can_tx_arm: %d checks, %d errors\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
