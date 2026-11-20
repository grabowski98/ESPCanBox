// Host test: transmit allow lists of the car profiles.
//
// TRANSMITS NOTHING: only the const tables and the content validators are checked, through the same function
// the gate and the hardware port use (CanTxGate::checkFrame). Built twice by tools/run_host_tests.sh, with
// -DCAN_TX_EXPERIMENTAL=0 (default) and =1; both outputs go into expected/profile_tables.txt.
//
// Checked: the lists are const (no run-time extension), row counts match the compile-time counts used by
// src/config.h, every row has a validator and a length of at most 8, no identifier appears twice, the
// generic and example profiles have no list, and a set of frames gets the expected verdict. Also the default
// radio key remaps of each profile (only the reference profile has any).
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <type_traits>

#include "can_tx_gate.h"
#include "profile_example.h"
#include "profile_generic.h"
#include "profile_psa_aee2010.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    checks++;
    if (!ok) failures++;
}

// The list cannot be changed through the profile: pointer to const rows, const profile objects.
static_assert(std::is_const<std::remove_pointer<decltype(vehicle::CarProfile::txAllow)>::type>::value,
              "CarProfile::txAllow must point to const rows");
static_assert(std::is_const<decltype(kCarPsaAee2010)>::value && std::is_const<decltype(kCarGeneric)>::value &&
                  std::is_const<decltype(kCarExample)>::value,
              "profiles must be const");
static_assert(std::is_const<std::remove_extent<decltype(psa::TX_ALLOW)>::type>::value, "psa::TX_ALLOW must be const");
static_assert(std::is_const<std::remove_pointer<decltype(vehicle::CarProfile::keyRemapDefaults)>::type>::value,
              "CarProfile::keyRemapDefaults must point to const rows");
static_assert(generic::TX_ALLOW_COUNT == 0, "the generic profile never transmits");

static CanFrame frame(uint32_t id, std::initializer_list<uint8_t> d) {
    CanFrame f;
    f.id = id;
    f.len = (uint8_t)d.size();
    uint8_t i = 0;
    for (uint8_t b : d) f.data[i++] = b;
    return f;
}

static const char* verdict(TxRefuse r) {
    switch (r) {
        case TxRefuse::OK: return "allowed";
        case TxRefuse::ID_NOT_ALLOWED: return "refused (not on the list)";
        case TxRefuse::BAD_LEN: return "refused (length)";
        default: return "?";
    }
}

static void listRows(const vehicle::CarProfile& p) {
    std::printf("profile %s: %u transmit rows\n", p.id, (unsigned)p.txAllowCount);
    bool ok = p.txAllowCount == 0 || p.txAllow != nullptr;
    for (uint8_t i = 0; i < p.txAllowCount; i++) {
        const vehicle::TxAllowEntry& e = p.txAllow[i];
        char len[24];
        if (e.len == 0) std::snprintf(len, sizeof(len), "by validator");
        else std::snprintf(len, sizeof(len), "%u", (unsigned)e.len);
        std::printf("  row %u: id %03X, length %s, validator %s\n", (unsigned)i, (unsigned)e.canId, len,
                    e.validator ? "yes" : "NO");
        if (!e.validator || e.len > 8 || e.canId > 0x7FF) ok = false;
        for (uint8_t j = 0; j < i; j++)
            if (p.txAllow[j].canId == e.canId) ok = false;
    }
    char line[160];
    std::snprintf(line, sizeof(line), "profile %s: every row has a validator, length <= 8, 11-bit id, no duplicates", p.id);
    check(ok, line);
}

static void expect(const vehicle::CarProfile* p, const CanFrame& f, TxRefuse want, const char* what) {
    const TxRefuse r = CanTxGate::checkFrame(p, f);
    char line[200];
    std::snprintf(line, sizeof(line), "%s: %s", what, verdict(r));
    check(r == want, line);
}

int main() {
    std::printf("CAN_TX_EXPERIMENTAL = %d\n", CAN_TX_EXPERIMENTAL);
    listRows(kCarPsaAee2010);
    listRows(kCarGeneric);
    listRows(kCarExample);
    check(kCarPsaAee2010.txAllow == psa::TX_ALLOW && kCarPsaAee2010.txAllowCount == psa::TX_ALLOW_COUNT &&
              psa::TX_ALLOW_COUNT == (CAN_TX_EXPERIMENTAL ? 2 : 1),
          "psa_aee2010: profile uses psa::TX_ALLOW, count = compile-time count (1, or 2 when experimental)");
    check(kCarGeneric.txAllow == nullptr && kCarGeneric.txAllowCount == generic::TX_ALLOW_COUNT &&
              kCarExample.txAllow == nullptr && kCarExample.txAllowCount == 0,
          "generic and example: no transmit list");
    // Default radio key remaps belong to the profile: only the reference car has them (owner's preference).
    check(kCarPsaAee2010.keyRemapDefaultCount == 2 && kCarPsaAee2010.keyRemapDefaults &&
              kCarPsaAee2010.keyRemapDefaults[0].from == 0x9F && kCarPsaAee2010.keyRemapDefaults[0].to == 0x9B &&
              kCarPsaAee2010.keyRemapDefaults[1].from == 0x21 && kCarPsaAee2010.keyRemapDefaults[1].to == 0x1F,
          "psa_aee2010: default key remaps 9F->9B, 21->1F");
    check(kCarGeneric.keyRemapDefaults == nullptr && kCarGeneric.keyRemapDefaultCount == 0 &&
              kCarExample.keyRemapDefaults == nullptr && kCarExample.keyRemapDefaultCount == 0,
          "generic and example: no default key remaps");

    const vehicle::CarProfile* p = &kCarPsaAee2010;
    std::printf("--- PSA AEE2010, frame 15B ---\n");
    expect(p, frame(0x15B, {0x00, 0x34, 0x97, 0xBA, 0xD5, 0x40, 0x00, 0x00}), TxRefuse::OK, "15B 00 34 97 BA D5 40 00 00");
    expect(p, frame(0x15B, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}), TxRefuse::OK,
           "15B FF x8 (no content check for 15B yet)");
    expect(p, frame(0x15B, {0x00, 0x34, 0x97, 0xBA, 0xD5, 0x40, 0x00}), TxRefuse::BAD_LEN, "15B, 7 bytes");
    CanFrame ext = frame(0x15B, {0, 0, 0, 0, 0, 0, 0, 0});
    ext.id = 0x1000015B;
    expect(p, ext, TxRefuse::ID_NOT_ALLOWED, "29-bit 1000015B");
    expect(p, frame(0x260, {0, 0, 0, 0, 0, 0, 0}), TxRefuse::ID_NOT_ALLOWED, "260");

    std::printf("--- PSA AEE2010, trial frame 1A9 (two recorded contents); 364 never ---\n");
    const TxRefuse expOk = CAN_TX_EXPERIMENTAL ? TxRefuse::OK : TxRefuse::ID_NOT_ALLOWED;
    expect(p, frame(0x1A9, {0x00, 0x7F, 0xFF, 0x04, 0xFF, 0x00, 0x10, 0x00}), expOk, "1A9 parking sensors button");
    expect(p, frame(0x1A9, {0x00, 0x7F, 0xFF, 0x20, 0xFF, 0x00, 0x10, 0x00}), expOk, "1A9 blind spot button");
    expect(p, frame(0x1A9, {0x00, 0x7F, 0xFF, 0x00, 0xFF, 0x00, 0x90, 0x00}), TxRefuse::ID_NOT_ALLOWED,
           "1A9 Stop and Start (never recorded)");
    expect(p, frame(0x1A9, {0x00, 0x7F, 0xFF, 0x24, 0xFF, 0x00, 0x10, 0x00}), TxRefuse::ID_NOT_ALLOWED,
           "1A9 two buttons at once");
    expect(p, frame(0x1A9, {0x80, 0x7F, 0xFF, 0x04, 0xFF, 0x00, 0x10, 0x00}), TxRefuse::ID_NOT_ALLOWED,
           "1A9 with another bit set in byte 1");
    expect(p, frame(0x364, {0x00, 0x00, 0x1E, 0x00}), TxRefuse::ID_NOT_ALLOWED, "364 idle, 4 bytes");
    expect(p, frame(0x364, {0x80, 0x00, 0x1E, 0x00}), TxRefuse::ID_NOT_ALLOWED, "364 one button bit, 4 bytes");
    expect(p, frame(0x364, {0x00, 0x00, 0x1E, 0x78, 0xE0, 0x00}), TxRefuse::ID_NOT_ALLOWED, "364 sliders unavailable, 6 bytes");

    std::printf("--- generic profile and no profile ---\n");
    expect(&kCarGeneric, frame(0x15B, {0x00, 0x34, 0x97, 0xBA, 0xD5, 0x40, 0x00, 0x00}), TxRefuse::ID_NOT_ALLOWED, "generic: 15B");
    expect(nullptr, frame(0x15B, {0x00, 0x34, 0x97, 0xBA, 0xD5, 0x40, 0x00, 0x00}), TxRefuse::ID_NOT_ALLOWED, "no profile: 15B");

    std::printf("profile_tables: %d checks, %d errors\n", checks, failures);
    return failures ? 1 : 0;
}
