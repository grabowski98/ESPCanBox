// Transmit allow list of the PSA AEE2010 profile.
//
// The CAN transmit gate (lib/core/can_tx_gate.h) sends nothing that is not in this table, and the hardware
// port (src/can_tx/can_tx_port.cpp) checks every frame against it a second time. The table is const data in
// flash; nothing adds a row at run time. Row order = lookup order (first row with the identifier wins).
//
// Default build (CAN_TX_EXPERIMENTAL = 0): one row, 15B.
// Build with -DCAN_TX_EXPERIMENTAL=1: also the trial frame 1A9, with the content validator below (trial in the
// prototype car 18.11.2026: the parking sensors press accepted both ways, the blind spot press accepted once).
// There is no climate frame: single 364 frames did not change the climate control of the reference car, and
// the 364 bits were never measured with an effect, so the program cannot send them.
#include "profile_psa_aee2010.h"

namespace {

// 15B, 8 bytes: identifier and length only. There is NO content check yet: the bytes come from
// vset::build15B (lib/car_psa_aee2010/vehicle_settings.cpp) from the current frame 260. A content validator
// (allowed bits per settings-map.md, with a tx_path test) is a separate decision of the owner.
bool txSettingsAnyContent(const CanFrame& f) { return f.len == 8; }

#if CAN_TX_EXPERIMENTAL
// 1A9, 8 bytes, EXACTLY one of two contents, both recorded from the aftermarket box with a reaction of the car
// (can_20261110_190421.log): 00 7F FF 04 FF 00 10 00 (parking sensors), 00 7F FF 20 FF 00 10 00 (blind spot).
// Other bits of 1A9 mean, among others, trip reset, eCall, tyre calibration: we never send them.
bool txButtonsValid(const CanFrame& f) {
    if (f.len != 8) return false;
    const uint8_t* d = f.data;
    if (d[0] != 0x00 || d[1] != 0x7F || d[2] != 0xFF || d[4] != 0xFF || d[5] != 0x00 || d[6] != 0x10 || d[7] != 0x00)
        return false;
    return d[3] == 0x04 || d[3] == 0x20;
}
#endif

}  // namespace

namespace psa {

// No size here: a table with a row count other than TX_ALLOW_COUNT conflicts with the declaration
// in profile_psa_aee2010.h and does not compile.
const vehicle::TxAllowEntry TX_ALLOW[] = {
    {ID_TX_SETTINGS, 8, txSettingsAnyContent},
#if CAN_TX_EXPERIMENTAL
    {ID_TX_BUTTONS, 8, txButtonsValid},
#endif
};

static_assert(sizeof(TX_ALLOW) / sizeof(TX_ALLOW[0]) == TX_ALLOW_COUNT, "TX_ALLOW_COUNT does not match the table");

}  // namespace psa
