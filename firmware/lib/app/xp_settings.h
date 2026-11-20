// Vehicle settings for the XP radio straight from the PSA AEE2010 car state: frame 38 DATA[1..5] (update38)
// and frame 3C (features361, build3C). REFERENCE ONLY: the Decoder builds these frames through the
// car-neutral interface (psa::decodeSettings -> vehicle::VehicleSettings -> xp::encodeCarState /
// xp::encodeSettingsPresent). These functions are the earlier implementation (formerly in
// lib/car_psa_aee2010/vehicle_settings.cpp, moved here because they produce XP bytes); the host test
// settings_38 checks them on vectors and recordings, so both implementations are held to the same results.
//
// Rules: vehicles/peugeot-508-sw-2015/settings-map.md, sections "XP frame `38`" (frame 38) and "Frames"
// (frame 3C). Checked on recordings from 2026-11-11: 38 of 38 frames 38 and 9 of 9 frames 3C of the
// Simple Soft box match (test test_host/settings_38.cpp).
#pragma once
#include "car_state.h"
#include "vehicle_settings.h"
#include "xp_protocol.h"

namespace vset {

// DATA[1..5] of frame 38 (d[0] = DATA[1]). Fields for which the rule says "no change"
// (language outside the table, units, Start/Stop) keep the previous result.
// Before the first valid frame 260 all zeros, like the box (recording test_data/captures/can_20261111_190551.log:
// after startup the box sends 04 00 00 00 00 00 although frames 227, 3E1 and 217 have already arrived; content only after 260).
struct State38 {
    uint8_t d[5] = {0};
    bool have260 = false;   // a full-length frame 260 has already been seen
};

// Recomputes the state from the latest frames 260, 227, 3E1, 217 (CarState). A frame not received or too
// short does not change its fields: 260 must have 7 bytes, 227 at least 3, 3E1 at least 4,
// 217 the full 8. Until the first valid 260 nothing changes (zeros).
void update38(const CarState& car, State38& s);

// The two data bytes of radio frame 3C (which rows of the feature settings screen the radio shows), computed
// from frame 361. false = no frame 361 yet or it has fewer than 4 bytes ('out' untouched).
bool features361(const CarState& car, uint8_t out[2]);

// Box -> radio frame number: which rows of the feature settings screen the radio shows.
const uint8_t CMD_FEATURES = xp::BOX_FEATURES;   // 3C

// Frame 3C (2 bytes) from frame 361. false = no frame 361 yet or it has fewer than 4 bytes ('out' untouched).
bool build3C(const CarState& car, xp::Frame& out);

}  // namespace vset
