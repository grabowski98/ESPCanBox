# Licences and source of the firmware images

This file goes into the `licenses/` folder of every release archive of ESPCanBox, next to
`LICENSE` (Apache-2.0, the project code), `LICENSE-docs` (CC BY 4.0), `LICENSE-hardware`
(CERN-OHL-S-2.0), `NOTICE` (the third-party parts and their licences), `LGPL-2.1.txt` and the
folder `third-party/` (the licence texts of the third-party parts listed below).

This note is not legal advice; it repeats what the licence texts named here say.

## Exact versions used to build the images

Read from `firmware/platformio.ini`, `.github/workflows/release.yml` and the manifests of the
installed PlatformIO packages (`package.json`, `.piopm`, `versions.txt`, `core_version.h`).

| Part | Version | Source at that exact version |
|---|---|---|
| PlatformIO Core (build tool) | 6.2.0 | <https://pypi.org/project/platformio/6.2.0/> |
| PlatformIO platform `espressif32` (pinned in `firmware/platformio.ini`) | 8.0.0 | <https://github.com/platformio/platform-espressif32/releases/tag/v8.0.0> |
| Package `framework-arduinoespressif32` = Arduino core for ESP32 (arduino-esp32) | 4.30312.261024 = arduino-esp32 3.3.12, commit `94afccf35fb1e401facddbcf9e13bcf7c76a31d8` (`ARDUINO_ESP32_GIT_VER 0x94afccf3`) | <https://github.com/espressif/arduino-esp32/releases/tag/3.3.12>, archive <https://github.com/espressif/arduino-esp32/archive/refs/tags/3.3.12.tar.gz> |
| Package `framework-arduinoespressif32-esp32s3-libs` (pre-built ESP-IDF libraries) | 3.3.12, built by esp32-arduino-lib-builder commit `6671d0bd65cdb9d4cc1001b759e8610de945a8d5` from ESP-IDF v5.5.5 | <https://github.com/espressif/esp32-arduino-lib-builder/tree/6671d0bd65cdb9d4cc1001b759e8610de945a8d5> |
| ESP-IDF | v5.5.5, commit `b774170ff46c393eeb5e495ea37936038d3f4f4f` | <https://github.com/espressif/esp-idf/tree/v5.5.5> |
| Package `toolchain-xtensa-esp-elf` (GCC, newlib) | 14.2.0+20260121 = crosstool-NG tag `esp-14.2.0_20260121` | <https://github.com/espressif/crosstool-NG/releases/tag/esp-14.2.0_20260121> |

The platform 8.0.0 asks for the two framework packages by range (`~4.30312.0`, `~3.3.12`).
The release workflow stops when the Arduino core it installed is not 3.3.12 and writes the
package versions it really used at the end of this file (section "Build of this archive").

## Arduino core for ESP32 (LGPL-2.1-or-later) and where its source is

Every firmware image (`.bin`) of this project is linked with the **Arduino core for ESP32**
(arduino-esp32 3.3.12: the core and its libraries WiFi, Network, ESPmDNS and Preferences), which is
licensed under the GNU Lesser General Public License, version 2.1 or later. The full licence text
is in `LGPL-2.1.txt`. The package metadata of the pre-built ESP-IDF libraries also states
LGPL-2.1-or-later for the package as a whole; the code inside it is ESP-IDF and the components
listed below, each under its own licence.

LGPL-2.1, section 6, asks whoever distributes a program linked with the library to let the user
change the library and relink the program, for example by offering access to the library source
from the same place (section 6(d)). For this project:

- **The source of the library is offered next to the images.** The release workflow downloads the
  archive of the exact arduino-esp32 tag from GitHub and puts it next to the images as
  `source-arduino-esp32-3.3.12.tar.gz` (checksum in `SHA256SUMS`). The project does not change
  the library. The files of the installed package were compared with that tag: the core and the
  four libraries are identical; only the packaging files differ (`package.json`, `platform.txt`,
  the generated `cores/esp32/core_version.h`).
- **The source of the program is public under Apache-2.0**: the complete source code of the images,
  the pinned versions above (`firmware/platformio.ini`) and the build instructions
  (`docs/en/quick-start/flash.md`) are in the public repository the images were published from.
  The exact commit is given at the end of this file.
- **Relinking with a changed library:** check out that commit, point PlatformIO at your version of
  the core (option `platform_packages`,
  <https://docs.platformio.org/en/latest/projectconf/sections/env/options/platform/platform_packages.html>,
  for example in `firmware/platformio_override.ini`) and build the same environment.

The images do not show copyright notices while they run.

## Third-party parts inside the images and their licence texts

Which parts are in the images was read from the linker maps of the five release environments
(only object files with code or data in the image counted; local build of version 2.0.0,
2026-11-20). "All" = every image; "Wi-Fi" = `listen_wifi`, `decoder_wifi`,
`listen_generic_wifi`. Texts are verbatim copies from the source named in the last column, in
`third-party/<folder>/`. Several licences (for example BSD-3-Clause clause 2, BSD-2-Clause clause
2, the MIT notice condition and Apache-2.0 section 4) ask that the copyright notice and the licence
text go with a binary distribution; this folder is that copy.

| Part (library in the map) | Images | Version | Licence (SPDX) | Copyright | Text in `third-party/` and its source |
|---|---|---|---|---|---|
| ESP-IDF own code (drivers, system, `esp_*`, `nvs_flash`, `heap`, `log`, …) | All | v5.5.5 | Apache-2.0 | Espressif Systems | `../LICENSE` (byte-identical to <https://github.com/espressif/esp-idf/blob/v5.5.5/LICENSE>); Espressif's own list of third-party code: `esp-idf/COPYRIGHT.rst` from <https://github.com/espressif/esp-idf/blob/v5.5.5/docs/en/COPYRIGHT.rst> |
| FreeRTOS kernel (`libfreertos`) | All | 10.5.1 (ESP-IDF SMP) | MIT | Amazon.com, Inc. or its affiliates (2021) | `freertos/LICENSE.md` from <https://github.com/espressif/esp-idf/blob/v5.5.5/components/freertos/FreeRTOS-Kernel/LICENSE.md>; copyright from `tasks.c` of the same tag |
| newlib C library (`libc`, `libm`) | All | 4.3.0 (in toolchain 14.2.0+20260121) | BSD-style, several (see text) | Red Hat, Inc. and the parties listed in the text | `newlib/COPYING.NEWLIB` from the toolchain package (`share/licenses/newlib/COPYING.NEWLIB`) |
| GCC runtime (`libgcc`, `libstdc++`) | All | 14.2.0 | GPL-3.0-or-later WITH GCC-exception-3.1 | Free Software Foundation, Inc. | `gcc-runtime/COPYING.RUNTIME`, `gcc-runtime/COPYING3` from the toolchain package (`share/licenses/gcc/`). The exception allows distributing the compiled program under any terms. |
| Mbed TLS (`libmbedcrypto`; SHA only without Wi-Fi) | All | 3.6.6 (espressif/mbedtls `9d669eadb1955d348986b9280156710aaaadf79f`) | Apache-2.0 OR GPL-2.0-or-later (used under Apache-2.0) | The Mbed TLS Contributors | `mbedtls/LICENSE` from <https://github.com/espressif/mbedtls/blob/9d669eadb1955d348986b9280156710aaaadf79f/LICENSE> |
| TLSF allocator (`libheap`, `tlsf.c`) | All | v3.1 (espressif/tlsf `2867f6883a12920b1969ff9624c0ab0e4185c2ce`) | BSD-3-Clause | 2006-2016 Matthew Conte | `tlsf/BSD-3-Clause.txt` from <https://github.com/spdx/license-list-data/blob/v3.29.0/text/BSD-3-Clause.txt> (the upstream file states only `SPDX-License-Identifier: BSD-3-Clause` and `SPDX-FileCopyrightText: 2006-2016 Matthew Conte`, <https://github.com/espressif/tlsf/blob/2867f6883a12920b1969ff9624c0ab0e4185c2ce/tlsf.c>) |
| UBSAN runtime (`libesp_system`, `ubsan.c`) | All | ESP-IDF v5.5.5 | BSD-2-Clause | 2016 Linaro Limited; Jiří Zárevúcky; 2020 Espressif Systems | `ubsan/license-header-ubsan.c.txt` = lines 1-25 of <https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_system/ubsan.c> |
| Xtensa HAL (`libxt_hal`, `libxtensa`) | All | ESP-IDF v5.5.5 | MIT | 1999-2015 Cadence Design Systems, Inc.; Tensilica Inc. | `xtensa/license-header-hal.h.txt` = lines 1-37 of <https://github.com/espressif/esp-idf/blob/v5.5.5/components/xtensa/include/xtensa/hal.h> |
| Wi-Fi, PHY and coexistence libraries (`libnet80211`, `libpp`, `libsmartconfig`, `libespnow`, `libphy`, `libcoexist`) | Wi-Fi (`libcoexist`: all) | esp32-wifi-lib `22bb3517b08760ed43b889013f8a122db0d46da0`, esp-phy-lib `59c1234e929212aec0fdda75769b759951235536`, esp-coex-lib `79e618f033e926d134decc6d95bb1b1850bd032c` | Apache-2.0 | Espressif Systems | `../LICENSE` (byte-identical to the `LICENSE` of each of the three repositories at those commits). ESP-IDF's `COPYRIGHT.rst` adds that the Wi-Fi library contains FreeBSD net80211 (2004-2008 Sam Leffler, Errno Consulting, BSD licence); no separate licence text for it is published in those repositories. |
| lwIP (`liblwip`) | Wi-Fi | 2.2.0 (espressif/esp-lwip `fd432e4ee2cfb7f7f1c7eb7227e0173412e7b84e`) | BSD-3-Clause | 2001, 2002 Swedish Institute of Computer Science | `lwip/COPYING` from <https://github.com/espressif/esp-lwip/blob/fd432e4ee2cfb7f7f1c7eb7227e0173412e7b84e/COPYING> |
| wpa_supplicant (`libwpa_supplicant`) | Wi-Fi | 2.10 (ESP-IDF v5.5.5) | BSD-3-Clause | 2002-2022 Jouni Malinen and contributors | `wpa_supplicant/COPYING`, `wpa_supplicant/README` from <https://github.com/espressif/esp-idf/tree/v5.5.5/components/wpa_supplicant> |
| cJSON (`libespressif__cjson`) | Wi-Fi | component 1.7.19~2 = cJSON 1.7.19 | MIT | 2009-2017 Dave Gamble and cJSON contributors | `cjson/LICENSE` from <https://github.com/DaveGamble/cJSON/blob/v1.7.19/LICENSE> (identical to the registry's <https://components.espressif.com/components/espressif/cjson/versions/1.7.19~2>) |
| HTTP parser (`libhttp_parser`) | Wi-Fi | 2.7.0 (ESP-IDF v5.5.5) | MIT | Igor Sysoev (NGINX); Joyent, Inc. and other Node contributors | `http_parser/LICENSE.txt` from <https://github.com/espressif/esp-idf/blob/v5.5.5/components/http_parser/LICENSE.txt> |
| mDNS (`libespressif__mdns`) | Wi-Fi | 1.13.1 (esp-protocols tag `mdns-v1.13.1`) | Apache-2.0 | Espressif Systems | `../LICENSE` (byte-identical to <https://github.com/espressif/esp-protocols/blob/mdns-v1.13.1/components/mdns/LICENSE>) |
| network_provisioning (`libespressif__network_provisioning`) | Wi-Fi | 1.0.2 | Apache-2.0 | 2020 Piyush Shah (as stated in its licence file) | `network_provisioning/LICENSE` from <https://components-file.espressif.com/components/espressif/network_provisioning/1.0.2/license.txt> |
| esp_diagnostics (`libespressif__esp_diagnostics`) | All | 1.3.4 | Apache-2.0 (registry: "Custom"; the file is the Apache-2.0 text) | Espressif Systems | `../LICENSE` (byte-identical to <https://components-file.espressif.com/components/espressif/esp_diagnostics/1.3.4/license.txt>) |

Not in the images (checked in the linker maps): protobuf-c (pulled in by the linker, but all its
code removed as unused), TinyUSB, nghttp, esp-tls, Mbed TLS X.509 and TLS parts, the Bluetooth
controller and host stacks (only a small ESP-IDF helper, `bt.c`, Apache-2.0, is linked).
The merged images (`*-merged.bin`) also hold the second-stage bootloader, pre-built by Espressif
from ESP-IDF v5.5.5 in the same package; it has no linker map in this build, so its parts were not
listed separately.

## Build of this archive

The release workflow (`.github/workflows/release.yml`) adds below the repository address, the tag
or branch, the commit the images were built from and the package versions it used. In the
repository copy of this file there is nothing below.
