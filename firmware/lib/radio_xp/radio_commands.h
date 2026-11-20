// Radio commands the head unit does not execute (they would require transmitting on CAN): parsing
// a frame into kind, number and value, plus name tables for console lines.
// Pure functions: no state, no queue, no hardware and no Arduino framework header. They transmit nothing.
//
// Table sources: vehicles/peugeot-508-sw-2015/settings-map.md, section "Climate control from the
// radio screen" (command 8A), and radios/xp-simple-soft/provenance.md (factory amplifier sound,
// command 84). Every number has its recording in provenance.md. Names of command 80 items are in vehicle_settings.h.
#pragma once
#include <stdint.h>

#include "xp_protocol.h"

// Radio command number "factory amplifier sound" (not present in xp_protocol.h).
const uint8_t RADIO_CMD_SOUND = 0x84;

// RadioCmdKind and RadioCmd (the decoded command) are car- and radio-neutral: lib/core/radio_protocol.h.

// Lengths: 80, 8A, 84 require at least 2 data bytes, 90 and 81 at least 1.
// Frame too short = NONE (we never read a byte beyond f.len). 81 with a value other than 00 / 01 = NONE.
RadioCmd parseRadioCmd(const xp::Frame& f);

// Name of a climate-screen button (no Polish diacritics). For numbers 4, 5, 10 the value
// 1 = plus, 2 = minus (airIsPlusMinus). nullptr = number outside the table.
const char* airCommandName(uint8_t n);
bool airIsPlusMinus(uint8_t n);

// Sound-screen row (names from the owner markers of 2026-11-19). nullptr = number outside the table
// (recorded: 2-6, 9, 10). The meaning of the value byte (00 / 01) was not determined.
const char* soundCommandName(uint8_t n);
