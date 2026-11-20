// Car profile PSA AEE2010: Peugeot 508 SW 2015, INFO-DIV bus at the radio connector.
// The reference and default profile (DecoderConfig::car in lib/app/decoder.h, -DCAR_PROFILE=PSA_AEE2010).
#pragma once
#include "car_profile.h"

extern const vehicle::CarProfile kCarPsaAee2010;

namespace psa {

// Frames this profile may transmit (table and validators in tx_allowlist.cpp).
const uint16_t ID_TX_SETTINGS = 0x15B;   // vehicle settings change in the BSI (BSI accepted it 18.11.2026)
const uint16_t ID_TX_BUTTONS = 0x1A9;    // "screen button" (trial, CAN_TX_EXPERIMENTAL only)

// Number of rows of the transmit allow list, known at compile time: src/config.h refuses to build a
// transmitting environment with a profile whose list is empty (static_assert). tx_allowlist.cpp checks
// that the table has exactly this many rows.
constexpr uint8_t TX_ALLOW_COUNT = CAN_TX_EXPERIMENTAL ? 2 : 1;

extern const vehicle::TxAllowEntry TX_ALLOW[TX_ALLOW_COUNT];

}  // namespace psa
