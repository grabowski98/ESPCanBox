# Firmware

Firmware for the ESP32-S3 board of this project (reference module: Waveshare
ESP32-S3-DEV-KIT-N16R8). PlatformIO with the Arduino framework, platform `espressif32@8.0.0`
(pinned in `platformio.ini`).

The device listens to the car CAN bus, beeps for the parking sensors from its own speaker and, in
decoder mode, translates the car to the radio protocol (UART) and drives the ACC, illumination and
reverse outputs. By default it **only listens**: nothing is sent on CAN, to the radio or on the
12 V outputs. Read [Safety](../docs/en/getting-started/safety.md) before you connect it to a car.

## What is in this directory

| Path | Content |
|---|---|
| `src/` | hardware and the main loop: CAN (`can_bus.cpp`), sound, radio UART, 12 V outputs, sleep, settings, console commands (`console_cmds.cpp`), `config.h` (modes and constants) |
| `src/net/` | web panel and Wi-Fi update (compiled only in the `*_wifi` environments) |
| `src/can_tx/` | CAN transmit hardware (compiled only in `decoder_tx_wifi`, the only place with `twai_transmit`) |
| `boards/` | one header per board with the pin numbers, plus the common pin checks and a template |
| `include/project_identity.h` | project name and compatibility values, generated from `../project.json` |
| `lib/core/` | logic without hardware: radio session common to every protocol, CAN transmit gate, parking beeper, power policy, network policy, console line ring, JSON, login, firmware marker |
| `lib/car_psa_aee2010/` | car profile of the reference car (Peugeot 508 SW 2015, PSA AEE2010) |
| `lib/car_generic/` | profile for an unknown car: listen only, decodes nothing |
| `lib/car_example/` | commented template for your own car profile (host tests only) |
| `lib/radio_xp/`, `lib/radio_none/` | radio protocols: XP (UART 38400 8N1) and "none" |
| `lib/radio_example/` | commented template for your own radio protocol, invented framing (host tests only) |
| `lib/app/` | the `Decoder` class that ties the car profile and the radio protocol together |
| `web/` | the panel page, its dictionaries (`web/i18n/`) and car data (`web/profiles/`) |
| `test_host/` | tests that run on a PC, with references in `test_host/expected/` and `test_host/golden/` |
| `test_data/` | anonymised CAN recordings from the reference car (`captures/`) and fixtures |
| `tools/` | test runner, panel packer, device mock, recorder and checks |
| `docs/` | a pointer to the panel API page |

The libraries in `lib/` do not include `Arduino.h` and build on a PC too. Layers are checked by
`python -I tools/check_layering.py`.

## Build and flash

Install [uv](https://docs.astral.sh/uv/), then from the repository root:

```sh
uvx platformio run -d firmware                          # builds the default environment "listen"
uvx platformio run -d firmware -t upload                # flashes "listen" over USB
uvx platformio run -d firmware -e listen_wifi -t upload # the same with the web panel
uvx platformio device monitor -d firmware               # USB console, 115200 baud
```

| Environment | Mode | Network | CAN transmission |
|---|---|---|---|
| `listen` (default) | beside the box | no | no |
| `decoder` | decoder | no | no |
| `listen_wifi` | beside the box | yes | no |
| `decoder_wifi` | decoder | yes | no |
| `decoder_tx_wifi` | decoder | yes | only the profile allow list (by default frame `15B`, checked by ID and length), after `cantx on` and a restart |
| `listen_generic_wifi` | beside the box, generic car profile, no radio | yes | no |

Decoder mode only with the factory CAN box unplugged. `decoder_tx_wifi` can transmit on the car
bus: read [Transmitting on CAN](../docs/en/safety/transmit.md) first. Frame `1A9` exists
only in a build with `-DCAN_TX_EXPERIMENTAL=1`; no build sends a climate frame.

Your own board or car profile without editing `src/`: copy `platformio_override.sample.ini` to
`platformio_override.ini` (not tracked). Details:
[Config and pins](../docs/en/reference/config-and-pins.md).

The firmware version is in `src/version.h`.

## Tests on a PC

From Git Bash (Windows) or any shell (Linux, macOS), in this directory:

```sh
bash tools/run_host_tests.sh
```

The script builds the tests into `.build_host/`, runs them on the recordings in
`test_data/captures/` and compares the whole output with the references. It stops with a non-zero
code at the first difference or at any compiler message. The compiler is clang from the `ziglang`
package through `uvx` (nothing to install apart from uv); set `CXX=g++` or `CXX=clang++` to use
another one. Nothing is flashed and no port is opened.

`bash tools/run_host_tests.sh --record-golden` records the references again. Do it only for a
change you mean, and describe the difference in the pull request.

Other checks:

```sh
python -I tools/check_i18n.py          # panel dictionaries
python -I tools/check_layering.py      # library layers
python -I tools/check_board_macros.py  # pins of the board headers
python -I tools/embed_web.py --report  # panel page size and CSP
bash tools/check_no_can_tx.sh          # after building all environments: no twai_transmit outside decoder_tx_wifi
```

Try the web panel without a board:
`python -I tools/mock_device.py test_data/captures/can_20261111_191045.log`, then open
`http://127.0.0.1:8080/`.

## Documentation

- [How it works](../docs/en/getting-started/how-it-works.md)
- [Flash the firmware](../docs/en/quick-start/flash.md) and [Bench test](../docs/en/quick-start/bench-test.md)
- [USB console](../docs/en/reference/console.md)
- [Panel API](../docs/en/reference/panel-api.md) and [Panel guide](../docs/en/reference/panel-guide.md)
- [Config and pins](../docs/en/reference/config-and-pins.md)
- [Architecture](../docs/en/reference/architecture.md) and [Versioning](../docs/en/reference/versioning.md)
- [Host tests](../docs/en/testing/host-tests.md) and [Golden files](../docs/en/testing/golden-files.md)
- [Write a car profile](../docs/en/make-it-yours/write-a-profile.md)

## Status

The logic is checked on recordings from the reference car (host tests). Release 2.0.0 was
bench-tested on an ESP32-S3-WROOM-1-N16R8 board
([Bench test protocol](../docs/en/testing/bench-test-protocol.md)) and tried in one car; see
`CHANGELOG.md` for what was and was not exercised.
The transmit hardware code (`src/can_tx/`, normal mode in `src/can_bus.cpp`) has no PC test.

Code: Apache-2.0 (`LICENSE`); third-party parts of the images and their licences: `NOTICE`.
Not affiliated with any car or radio maker.
