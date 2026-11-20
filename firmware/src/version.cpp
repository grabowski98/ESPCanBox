// Firmware identity marker (description and rules: version.h). Compiled in all environments.
#include "version.h"
#include "config.h"

// The only instance of the marker in the image. The "used" attribute keeps the compiler from removing it.
// FW_IDENT_MAGIC, __DATE__ and __TIME__ appear in the whole program ONLY here (see version.h).
__attribute__((used)) static const FwIdent s_ident = {
    FW_IDENT_MAGIC, FW_PRODUCT, FW_VERSION, __DATE__ " " __TIME__,
    (uint8_t)(WORK_MODE == MODE_DECODER ? 1 : 0), (uint8_t)(NET_ENABLED ? 1 : 0),
    // canTx: 1 = the image can transmit frame 15B (only environment decoder_tx_wifi), 0 = listen only
    (uint8_t)(CAN_TX_MODE == CAN_TX_SETTINGS ? 1 : 0),
    0};

const FwIdent& fwIdent() { return s_ident; }
