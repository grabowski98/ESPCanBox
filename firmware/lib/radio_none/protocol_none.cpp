#include "protocol_none.h"

const RadioProtocol kRadioNone = {
    "none",
    0,          // no UART
    nullptr,    // no key codes
    // no car data for a radio
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};
