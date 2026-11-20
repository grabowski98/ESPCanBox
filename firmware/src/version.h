// Firmware version and product name.
//
// The version number is HERE, not in platformio.ini: any change of platformio.ini wipes the whole build
// directory (everything is rebuilt from scratch), while a change of this file rebuilds only src/.
//
// The same data goes into the program image as the FwIdent marker (lib/core/fw_ident.h). From it
// the update over Wi-Fi recognises that the uploaded file is a program of this device, in which operating
// mode it was built and whether it has the network panel. The marker is in ALL environments
// (also without network): an image without network must also be recognisable before it is flashed.
//
// The file does not depend on Arduino.
#pragma once
#include <stdint.h>

#include "fw_ident.h"
#include "project_identity.h"

#define FW_PRODUCT PROJECT_FW_PRODUCT  // at most 15 characters (field FwIdent::product); value in project.json
// Semantic Versioning: x.y.z, or x.y.z-beta.N for a pre-release (the "beta" is then visible in "ver", the
// panel and the names of the release files). Only characters 0-9, a-z, A-Z, '.', '-': .github/workflows/release.yml
// reads the value with that pattern and requires the tag v<FW_VERSION>. Nothing compares versions.
#define FW_VERSION "2.0.0-beta.1"      // at most 15 characters (field FwIdent::version)
// Version of the docs/PANEL_API.md description. Increases only on a backward incompatible change (removed field,
// different meaning). The panel and the Python mock compare it with the "api" field of the /api/hello response.
#define FW_API_VERSION 1

static_assert(sizeof(FW_PRODUCT) <= sizeof(FwIdent::product), "FW_PRODUCT too long");
static_assert(sizeof(FW_VERSION) <= sizeof(FwIdent::version), "FW_VERSION too long");

// Marker of this firmware: product, version, build time, operating mode, presence of network,
// ability to transmit on CAN (canTx: 1 only in the environment with CAN_TX_MODE = CAN_TX_SETTINGS).
// May be called from any task (a constant in flash memory, blocks nothing).
//
// Definition: src/version.cpp (to be written; compiled in all environments). The whole file content:
//
//     #include "version.h"
//     #include "config.h"
//     // The only instance of the marker in the image. The "used" attribute keeps the compiler from removing it.
//     __attribute__((used)) static const FwIdent s_ident = {
//         FW_IDENT_MAGIC, FW_PRODUCT, FW_VERSION, __DATE__ " " __TIME__,
//         (uint8_t)(WORK_MODE == MODE_DECODER ? 1 : 0), (uint8_t)(NET_ENABLED ? 1 : 0),
//         (uint8_t)(CAN_TX_MODE == CAN_TX_SETTINGS ? 1 : 0) /* canTx */, 0};
//     const FwIdent& fwIdent() { return s_ident; }
//
// Rules this file must follow:
//  - FW_IDENT_MAGIC may be used ONLY there. Elsewhere the needed pattern is taken from
//    fwIdent().magic (e.g. IdentScanner(fwIdent().magic)); a second copy of the string in the image would confuse the scanner.
//  - __DATE__ and __TIME__ may be used ONLY there: in every .cpp file they would have a different value.
//  - The marker must be in src/, not in lib/core: from the libcore.a library the linker takes only objects
//    the code reaches for, and main.cpp reaches for fwIdent() (greeting line and the "ver" command).
//
// The build field is the compile time of version.cpp ("Nov 16 2026 21:14:44"). tools/build_time.py
// recompiles it on every change in src/, lib/ or web/, so it is the time of the last build
// with changed sources (a repeated "pio run" without changes does not touch it). To tell apart two images
// of the same version use the ELF file hash (field "elf" in /api/status and /api/ota/status, function
// otaRunningElf() in src/net/ota.h; NOT esp_app_get_elf_sha256(), which here returns only 9 characters).
const FwIdent& fwIdent();

// Operating mode name in JSON responses: 1 -> "decoder", any other value -> "listen".
inline const char* fwModeName(uint8_t mode) { return mode == 1 ? "decoder" : "listen"; }
