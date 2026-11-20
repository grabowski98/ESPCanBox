// Host test: the button table split into two tables.
// Earlier one table LEVEL_KEYS (lib/radio_xp/xp_keys.cpp) held CAN id, byte, mask, value and the XP code.
// Now the car profile holds CAN -> logical key (kCarPsaAee2010.keys / knobs) and the radio protocol holds
// logical key -> XP code (xp::keyCode). This test keeps a literal copy of the old table and checks that every
// row, in the same order, gives the same code through the two tables; the same for the two knobs. It also checks
// the generic profile (no keys) and the radio "none" (no key codes).
#include <cstdio>

#include "profile_generic.h"
#include "profile_psa_aee2010.h"
#include "protocol_none.h"
#include "protocol_xp.h"

namespace {

struct OldLevelKey {
    uint16_t canId;
    uint8_t byteIndex;
    uint8_t mask;
    uint8_t value;
    uint8_t code;
};

// Copy of LEVEL_KEYS from lib/radio_xp/xp_keys.cpp as of firmware 1.0.0, same order.
const OldLevelKey OLD[] = {
    {0x21F, 0, 0x0C, 0x08, 0x14}, {0x21F, 0, 0x0C, 0x04, 0x15}, {0x21F, 0, 0x0C, 0x0C, 0x16},
    {0x21F, 0, 0x02, 0x02, 0x30}, {0x21F, 0, 0x01, 0x01, 0x50}, {0x21F, 2, 0x80, 0x80, 0x07},
    {0x0A2, 1, 0x04, 0x04, 0x21}, {0x221, 0, 0x01, 0x01, 0x23}, {0x122, 1, 0x80, 0x80, 0x9E},
    {0x122, 1, 0x20, 0x20, 0xA0}, {0x122, 1, 0x01, 0x01, 0x97}, {0x122, 2, 0x40, 0x40, 0x9C},
    {0x122, 2, 0x80, 0x80, 0x98}, {0x122, 1, 0x08, 0x08, 0x9F}, {0x122, 1, 0x02, 0x02, 0x99},
    {0x122, 1, 0x10, 0x10, 0x9B}, {0x122, 1, 0x40, 0x40, 0xA4}, {0x122, 2, 0x20, 0x20, 0x9D},
    {0x122, 1, 0x04, 0x04, 0xA1}, {0x122, 0, 0x01, 0x01, 0xA3}, {0x122, 0, 0x02, 0x02, 0x22},
    {0x036, 6, 0x0F, 0x02, 0x16},
};
const unsigned OLD_COUNT = sizeof(OLD) / sizeof(OLD[0]);

// Old knob rules (xp_keys.cpp as of firmware 1.0.0): 21F byte 2 (index 1), counter up -> 18, down -> 17;
// 122 byte 7 (index 6), up -> 81, down -> 82.
struct OldKnob {
    uint16_t canId;
    uint8_t byteIndex;
    uint8_t codeUp;
    uint8_t codeDown;
};
const OldKnob OLD_KNOBS[] = {{0x21F, 1, 0x18, 0x17}, {0x122, 6, 0x81, 0x82}};
const unsigned OLD_KNOB_COUNT = sizeof(OLD_KNOBS) / sizeof(OLD_KNOBS[0]);

int bad = 0;

void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) bad++;
}

}  // namespace

int main() {
    const vehicle::CarProfile& p = kCarPsaAee2010;
    printf("profile %s: %u key rows, %u knobs, bit rate %lu\n", p.id, p.keyCount, p.knobCount,
           (unsigned long)p.canBitrate);
    check(p.keyCount == OLD_COUNT, "key row count equals the old table");
    for (unsigned i = 0; i < OLD_COUNT && i < p.keyCount; i++) {
        const vehicle::KeyMapEntry& k = p.keys[i];
        const OldLevelKey& o = OLD[i];
        const uint8_t code = xp::keyCode(k.key);
        const bool same = k.canId == o.canId && k.byteIndex == o.byteIndex && k.mask == o.mask &&
                          k.value == o.value && code == o.code;
        printf("%s row %2u: %03X byte %u mask %02X value %02X -> key %2u -> XP %02X (old %02X)\n",
               same ? "ok  " : "FAIL", i, k.canId, k.byteIndex, k.mask, k.value, (unsigned)k.key, code, o.code);
        if (!same) bad++;
    }
    check(p.knobCount == OLD_KNOB_COUNT, "knob count equals the old rules");
    for (unsigned i = 0; i < OLD_KNOB_COUNT && i < p.knobCount; i++) {
        const vehicle::KnobEntry& k = p.knobs[i];
        const OldKnob& o = OLD_KNOBS[i];
        const uint8_t up = xp::keyCode(k.onIncrease), down = xp::keyCode(k.onDecrease);
        const bool same = k.canId == o.canId && k.byteIndex == o.byteIndex && up == o.codeUp && down == o.codeDown;
        printf("%s knob %u: %03X byte %u -> up XP %02X, down XP %02X (old %02X / %02X)\n", same ? "ok  " : "FAIL", i,
               k.canId, k.byteIndex, up, down, o.codeUp, o.codeDown);
        if (!same) bad++;
    }
    // Every logical key except NONE / COUNT has an XP code (the table has no hole).
    unsigned withCode = 0;
    for (unsigned k = 1; k < (unsigned)vehicle::KeyId::COUNT; k++)
        if (xp::keyCode((vehicle::KeyId)k) != 0) withCode++;
    printf("XP codes for %u of %u logical keys\n", withCode, (unsigned)vehicle::KeyId::COUNT - 1);
    check(withCode == (unsigned)vehicle::KeyId::COUNT - 1, "every logical key has an XP code");
    check(xp::keyCode(vehicle::KeyId::NONE) == 0, "KeyId::NONE has no code");

    check(kRadioXp.keyCode == xp::keyCode && kRadioXp.uartBaud == 38400, "radio xp: key table and 38400 baud");
    check(kRadioNone.keyCode == nullptr && kRadioNone.uartBaud == 0, "radio none: no key codes, no UART");
    check(kCarGeneric.keyCount == 0 && kCarGeneric.knobCount == 0 && kCarGeneric.txAllowCount == 0 &&
              !kCarGeneric.decodeBody && !kCarGeneric.decodeParking && !kCarGeneric.decodeLights &&
              !kCarGeneric.powerOutputs,
          "profile generic: no keys, empty txAllow, no decoders, no 12 V outputs");
    check(kCarGeneric.keyRemapDefaultCount == 0, "profile generic: no default key remaps");
    // The copy in test_host/custom_keys_doors.cpp (PSA_DEFAULTS) must match these rows.
    check(p.keyRemapDefaultCount == 2 && p.keyRemapDefaults[0].from == xp::keyCode(vehicle::KeyId::TEL) &&
              p.keyRemapDefaults[0].to == 0x9B && p.keyRemapDefaults[1].from == xp::keyCode(vehicle::KeyId::SOURCE) &&
              p.keyRemapDefaults[1].to == 0x1F,
          "profile psa_aee2010: default remaps TEL (9F) -> 9B, SOURCE (21) -> 1F");
    check(p.txAllowCount >= 1 && p.txAllow && p.txAllow[0].canId == 0x15B && p.txAllow[0].len == 8,
          "profile psa_aee2010: txAllow starts with 15B, 8 bytes (details: profile_tables)");
    printf("%s\n", bad ? "KEY TABLES DIFFER" : "key tables identical");
    return bad ? 1 : 0;
}
