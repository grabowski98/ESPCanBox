#include "radio_commands.h"

RadioCmd parseRadioCmd(const xp::Frame& f) {
    RadioCmd c;
    switch (f.cmd) {
        case xp::RADIO_SETTING:
        case xp::RADIO_AIR:
        case RADIO_CMD_SOUND:
            if (f.len < 2) return c;           // too short: we read neither the number nor the value
            c.kind = f.cmd == xp::RADIO_SETTING ? RadioCmdKind::SETTING
                     : f.cmd == xp::RADIO_AIR ? RadioCmdKind::AIR : RadioCmdKind::SOUND;
            c.id = f.data[0];
            c.value = f.data[1];
            return c;
        case xp::RADIO_REQUEST:
            if (f.len < 1) return c;
            c.kind = RadioCmdKind::REQUEST;
            c.id = f.data[0];
            return c;
        case xp::RADIO_STATE:
            if (f.len < 1 || f.data[0] > 1) return c;
            c.kind = f.data[0] ? RadioCmdKind::RADIO_ON : RadioCmdKind::RADIO_OFF;
            c.value = f.data[0];
            return c;
        default:
            return c;
    }
}

// settings-map.md, section "Climate control from the radio screen". Every number was recorded on 2026-11-19 after
// an owner marker naming the button on the radio climate screen (radios/xp-simple-soft/provenance.md); 0C was
// recorded from the vehicle settings screen, row "temperature unit".
const char* airCommandName(uint8_t n) {
    switch (n) {
        case 1: return "AUTO";
        case 2: return "A/C";
        case 3: return "MAX A/C";
        case 4: return "temperature left";
        case 5: return "temperature right";
        case 6: return "air to face";
        case 7: return "air to windscreen";
        case 8: return "air to feet";
        case 9: return "fan";
        case 10: return "fan speed";
        case 11: return "MONO";
        case 12: return "temperature unit";
        default: return nullptr;
    }
}

bool airIsPlusMinus(uint8_t n) { return n == 4 || n == 5 || n == 10; }

// Rows of the radio sound screen, named after the owner markers of 2026-11-19 (radios/xp-simple-soft/provenance.md).
const char* soundCommandName(uint8_t n) {
    switch (n) {
        case 2: return "surround";
        case 3: return "fader";
        case 4: return "balance";
        case 5: return "bass";
        case 6: return "treble";
        case 9: return "sound selection";
        case 10: return "volume / loudness";
        default: return nullptr;
    }
}
