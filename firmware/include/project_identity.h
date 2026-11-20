// GENERATED from project.json by tools/set_project_name.py. Do not edit by hand.
//
// Project identity in one place. Firmware code uses these macros through version.h (FW_PRODUCT),
// fw_ident.h (FW_IDENT_MAGIC), config.h (NET_AP_SSID_PREFIX, NET_HOSTNAME) and settings.cpp (NVS).
// firmware/tools/project_identity.py reads this file for the Python tools (panel build, mock device).
//
// Compatibility values: PROJECT_FW_PRODUCT and PROJECT_FWID_MAGIC are checked by the Wi-Fi update
// of installed units, PROJECT_NVS_NAMESPACE holds their saved settings. No Arduino.h: also compiled
// on the PC (test_host).
#pragma once

#define PROJECT_NAME             "ESPCanBox"
#define PROJECT_SLUG             "espcanbox"
#define PROJECT_FW_PRODUCT       "espcanbox"
#define PROJECT_FWID_MAGIC       "ESPC-FWID"
#define PROJECT_AP_SSID_PREFIX   "ESPCanBox-"
#define PROJECT_MDNS_HOST        "espcanbox"
#define PROJECT_NVS_NAMESPACE    "espcanbox"
#define PROJECT_WEB_SESSION_KEY  "espcanboxpw"

// FwIdent::product is char[16]: at most 15 characters plus the terminating zero.
static_assert(sizeof(PROJECT_FW_PRODUCT) <= 16, "PROJECT_FW_PRODUCT too long");
// FwIdent::magic is char[12] = 0x7F + 9 characters + 0x7F + zero (lib/core/fw_ident.h, web/app.js).
static_assert(sizeof(PROJECT_FWID_MAGIC) == 10, "PROJECT_FWID_MAGIC must have exactly 9 characters");
// ESP-IDF NVS namespace: at most 15 characters.
static_assert(sizeof(PROJECT_NVS_NAMESPACE) <= 16, "PROJECT_NVS_NAMESPACE too long");
// Wi-Fi SSID is at most 32 bytes: prefix + 4 hex digits from the MAC address.
static_assert(sizeof(PROJECT_AP_SSID_PREFIX) - 1 + 4 <= 32, "PROJECT_AP_SSID_PREFIX too long");
// mDNS host name label: at most 63 characters.
static_assert(sizeof(PROJECT_MDNS_HOST) <= 64, "PROJECT_MDNS_HOST too long");
