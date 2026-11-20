# Configuration and pins

Everything that decides what the device may do is chosen **when you build** the firmware, not at
run time. This page lists the build environments, the build flags, the constants in
`firmware/src/config.h` and the pins of the boards in `firmware/boards/`.

If you only use the device, you need two parts: the table of [build environments](#build-environments)
(which one to flash) and the colours of the [status LED](#status-led). The rest is for people who
build their own board or add an environment.

The rule behind it: everything that can send something (UART to the radio, 12 V outputs, CAN) has
an explicit mode and is **off by default**. You switch it on by choosing another environment, never
by editing `config.h`.

## Build environments

Defined in `firmware/platformio.ini`. A plain `uvx platformio run -d firmware -t upload` without
`-e` flashes `listen`, the safest one.

| Environment | Mode | Network (panel, Wi-Fi update) | Car profile / radio | CAN transmission |
|---|---|---|---|---|
| `listen` (default) | beside the box | no | `psa_aee2010` / `xp` | no (the program has no transmit function) |
| `decoder` | decoder | no | `psa_aee2010` / `xp` | no |
| `listen_wifi` | beside the box | yes | `psa_aee2010` / `xp` | no |
| `decoder_wifi` | decoder | yes | `psa_aee2010` / `xp` | no |
| `decoder_tx_wifi` | decoder | yes | `psa_aee2010` / `xp` | only the frames of the profile allow list, after arming: by default `15B` |
| `listen_generic_wifi` | beside the box | yes | `generic` / `none` | no |

- **Beside the box** (`MODE_BESIDE_BOX`): the device works next to the aftermarket CAN box. It only
  listens, to the car bus and to the line from the radio to the box. It sends nothing to the radio,
  and the 12 V outputs stay off. It beeps from its own speaker. The console prints this mode as
  `alongside the box`, and `ver` as `listen`: it is the same mode.
- **Decoder** (`MODE_DECODER`): the device replaces the box. It sends frames to the radio and drives
  the ACC, illumination and reverse outputs.

> [!CAUTION]
> **Decoder mode only with the box unplugged**
>
> Two transmitters on one UART line and two ACC sources short the outputs. Flash a decoder
> environment only on the bench or after you have unplugged the CAN box.

> [!CAUTION]
> **decoder_tx_wifi can transmit on the car bus**
>
> It is the only environment that compiles `firmware/src/can_tx/`. After `cantx on` and a restart
> it can send frame `15B` (checked by ID and length only). Read
> [Transmitting on the CAN bus](../safety/transmit.md) before you build it.

In every environment except `decoder_tx_wifi` the CAN controller runs in listen-only mode: it
sends no frames, no acknowledgements and no error frames. The script
`firmware/tools/check_no_can_tx.sh` checks after a build that those programs contain no
`twai_transmit`.

Older names of the environments are noted as comments next to each section of `platformio.ini`.

## Build flags

The environments set these flags. You need them only when you add your own environment, best in
`firmware/platformio_override.ini` (copy `platformio_override.sample.ini`; the file is not
tracked).

| Flag | Values | Set by | Meaning |
|---|---|---|---|
| `-DBOARD=` | `PROTO_V1`, `TEMPLATE`, `CUSTOM` | all environments: `PROTO_V1` | which board header is used, see [Boards](#boards) |
| `-DBOARD_CUSTOM_HEADER=` | `'"board_<name>.h"'` | your own environment | header for `BOARD=CUSTOM` |
| `-DCAR_PROFILE=` | `PSA_AEE2010` (default), `GENERIC`, `CUSTOM` | `listen_generic_wifi`: `GENERIC` | car profile: what the firmware knows about the car |
| `-DCAR_PROFILE_CUSTOM_HEADER=`, `-DCAR_PROFILE_CUSTOM_OBJECT=` | `'"profile_<name>.h"'`, `kCar<Name>` | your own environment | your own profile without editing `src/`; also add `lib_deps = car_<name>` |
| `-DRADIO_PROTOCOL=` | `XP` (default), `NONE` | `listen_generic_wifi`: `NONE` | radio protocol: `XP` = UART 38400 8N1; `NONE` = no UART, nothing sent to a radio |
| `-DWORK_MODE=` | `MODE_BESIDE_BOX` (default), `MODE_DECODER` | `decoder*` environments | operating mode |
| `-DNET_ENABLED=` | `0` (default), `1` | `*_wifi` environments | web panel and Wi-Fi update |
| `-DCAN_TX_MODE=` | `CAN_TX_OFF` (default), `CAN_TX_SETTINGS` | only `decoder_tx_wifi` | CAN transmission of the profile allow list |
| `-DCAN_TX_EXPERIMENTAL=` | `0` (default), `1` | `decoder_tx_wifi`: `0` | `1` adds the trial frame `1A9` of the `psa_aee2010` profile (two recorded contents: parking sensors and blind spot buttons) |
| `-DARDUINO_USB_MODE=1`, `-DARDUINO_USB_CDC_ON_BOOT=1` | | all environments | the console (`Serial`) runs over the native USB of the ESP32-S3 |
| `-I include`, `-I boards` | | all environments | project identity header and board headers |

`-DCAN_TX_1A9_RELEASE=` (`0` by default, set by no environment): `1`, only together with
`CAN_TX_EXPERIMENTAL=1`, sends one release frame `1A9 20 7F FF 00 FF 00 50 00` about 200 ms after an
acknowledged `1A9` press (in practice when the 500 ms gate pause ends). It is the periodic `1A9` the
aftermarket box sent every ~204 ms with ignition on; a press of the box replaced one periodic frame
and the next periodic frame followed it (the box sent no separate release). Whether a press without
such a following frame causes a later toggle is a hypothesis, not measured.

`build_src_filter` does the rest: without network the directory `src/net/` is not compiled at all;
outside `decoder_tx_wifi` the directory `src/can_tx/` is not compiled at all.

> [!WARNING]
> **CAN_TX_EXPERIMENTAL=1 is for measurements**
>
> Set it only in your own `platformio_override.ini`, on purpose. Frame `1A9` sent once by the
> device was accepted for some buttons and not for others on the reference car (measured: device
> console in the car). There is no climate frame in any build: single `364` frames did not change
> the climate control. See [Transmitting on the CAN bus](../safety/transmit.md).

### What stops the build

The checks in `config.h` and `boards/board_common.h` stop the build with a readable message when:

- `BOARD` is not set, unknown, or `TEMPLATE` (the template is not filled in);
- a board header misses a pin, uses GPIO 35-37 or 19/20, uses a strapping pin (0, 3, 45, 46) without a
  `BOARD_<PIN>_STRAP_OK` macro, uses a GPIO that does not exist, or uses one GPIO twice;
- `CAR_PROFILE` is unknown or `EXAMPLE` (the example profile is only for the host tests);
- `CAN_TX_MODE` is not `CAN_TX_OFF` and the mode is not decoder;
- CAN transmission is combined with `CAR_PROFILE=GENERIC` or `CAR_PROFILE=CUSTOM`, or with a
  profile whose transmit allow list is empty (a new car needs a named profile with measured frames
  first);
- `CAN_TX_EXPERIMENTAL` is not 0 or 1;
- `CAN_TX_1A9_RELEASE` is not 0 or 1, or it is 1 without `CAN_TX_EXPERIMENTAL=1`.

## Car profile and radio protocol

The car profile is one `const` structure (`firmware/lib/core/car_profile.h`): bus speed, button
table, knobs, transmit allow list, the IDs to mask in shared recordings, and the decoding functions
for parking sensors, ignition and reverse, and lights. It also says whether the firmware may drive
the 12 V outputs.

| Profile | Library | CAN speed | 12 V outputs | Transmit allow list |
|---|---|---|---|---|
| `psa_aee2010` | `lib/car_psa_aee2010` | 125 kbit/s | yes (decoder mode) | `15B`; with `CAN_TX_EXPERIMENTAL=1` also `1A9` |
| `generic` | `lib/car_generic` | 125 kbit/s by default (set your measured speed in `profile_generic.cpp`) | never | empty |

On the reference car (Peugeot 508 SW 2015) the bus at the radio runs at 125 kbit/s (measured:
reception without errors, `firmware/test_data/captures/can_20261110_190421.log`). Its line
voltages look like low-speed fault-tolerant CAN, not high-speed CAN; an oscilloscope is needed to
confirm this. The profile therefore records the physical layer as unknown.

`firmware/src/can_bus.cpp` knows the speeds 50, 100, 125, 250, 500 and 1000 kbit/s. With any other
value in the profile it does not start the CAN controller: the device stays in the safe state.

How to write your own profile: [Write a profile](../make-it-yours/write-a-profile.md).

## Constants in config.h

These are the values you may want to know. Change them only with a reason, and keep the comment
that explains the value.

| Constant | Value | Meaning |
|---|---|---|
| `AUDIO_SAMPLE_RATE` | 22050 Hz | sound output (MAX98357A over I2S) |
| `AUDIO_VOLUME_DEFAULT` | 40 % | volume when nothing is stored |
| `AUDIO_VOLUME_MAX` | 80 % | upper limit, protects the 8 Ω / 0.5 W speaker |
| `AUDIO_VOLUME_MIN_SAVED` | 15 % | lowest volume that is stored; a stored value is raised to it at start |
| `SLEEP_ENABLED`, `SLEEP_AFTER_MS` | 1, 30000 ms | deep sleep after 30 s of bus silence (and no computer on USB) |
| `CAN_TX_HW_WINDOW_US` | 25000 µs | hardware timer that closes the CAN transmitter even if the main loop hangs (the gate closes it after 20 ms) |
| `NET_AP_IP` | 10.50.8.1 | device address in its own network (/24) |
| `NET_AP_CHANNEL` | 1 | Wi-Fi channel of the device network (moves to the router channel with a home network) |
| `NET_AP_MAX_STATIONS` | 4 | phones or computers at once on the device network |
| `NET_TX_POWER_QDBM` | 44 (11 dBm) | Wi-Fi transmit power, in units of 0.25 dBm |
| `NET_MIN_RAIL_V` | 11.0 V | the device stays awake for a panel client only above this rail voltage (behind the input diode) |
| `NET_VBAT_VALID_MIN_V` | 6.0 V | below it the device assumes there is no 12 V (USB power only) |
| `NET_AP_SSID_PREFIX`, `NET_HOSTNAME` | from `project.json` | name of the device network and the `.local` name |

How the speaker limit was found (measured: AC meter on the speaker terminals, USB power): a
continuous tone at 50 % gave 0.753 V on the speaker, so full scale is about 3.0 V (1.2 W). At 80 % a
continuous tone gives about 1.2 V (0.18 W) and the peak of a beep about 2.2 V for a few ms.

The device sleeps after the car goes quiet. On the reference car the bus keeps talking for about
100 s after the key is removed (measured: CAN recording during a stop), then goes silent.

## Settings stored on the device

A few settings live in the flash memory of the device (NVS, namespace = `nvs_namespace` from
`project.json`). Flashing over USB or over Wi-Fi keeps them.

| Key | What | Changed by |
|---|---|---|
| `pass` | device password (panel and Wi-Fi key) | `wifi pass`, `wifi newpass`, panel |
| `sta_ssid`, `sta_pass` | home network | `wifi sta`, panel |
| `net_on` | Wi-Fi switch | `wifi on`, `wifi off` |
| `lan_admin` | changes allowed from the home network | `wifi lan on`, `wifi lan off` |
| `keymap` | button remapping table | `map`, panel |
| `vol` | beep volume (from 15 %) | `vol N`, panel |
| `cantx` | CAN transmit arming: the build string of the program in which `cantx on` was typed | `cantx on`, `cantx off`; any other program clears it at start |
| `vsetmask` | mask of vehicle setting items | `vset on NN`, `vset off NN` |

A flash write stops CAN reception for a few milliseconds. That is why the device delays or refuses
these writes while reverse gear is engaged or the parking sensors are active.

> [!WARNING]
> **Changing nvs_namespace forgets everything**
>
> A program with another `nvs_namespace` starts with empty settings: a new random password,
> default volume, no home network, no remappings.

## Boards

The pins live only in a board header in `firmware/boards/`. `config.h` takes the values from it.

| File | Use |
|---|---|
| `board_proto_v1.h` | the home-made prototype v1 (`hardware/prototype-v1/`) with a Waveshare ESP32-S3-DEV-KIT-N16R8 module |
| `board_template.h` | start for your own board; it stops the build until you fill it in |
| `board_common.h` | the checks common to every board |
| `board_proto_v1.snapshot.txt` | the resolved pin and board values of v1; `python -I tools/check_board_macros.py` (in `firmware/`) compares the current build with it, so a change of a header cannot move a pin unnoticed |

### Pins of prototype v1

| Function | Macro | GPIO | Notes |
|---|---|---|---|
| CAN RX (from the transceiver) | `BOARD_PIN_CAN_RX` | 4 | Adafruit CAN Pal module, TJA1051T/3 |
| CAN TX (to the transceiver) | `BOARD_PIN_CAN_TX` | 5 | through jumper JP1. JP1 open = no transmission possible: keep it open for everyday use, close it only to transmit on purpose. On the author's prototype JP1 is closed |
| CAN SLNT (silent) | `BOARD_PIN_CAN_SILENT` | 6 | high = transmitter off. R3 10 kΩ to 3.3 V on the board |
| I2S BCLK | `BOARD_PIN_I2S_BCLK` | 15 | MAX98357A module |
| I2S LRC | `BOARD_PIN_I2S_LRC` | 16 | |
| I2S DIN | `BOARD_PIN_I2S_DIN` | 17 | |
| amplifier SD | `BOARD_PIN_AMP_SD` | 18 | low = amplifier off; 10 kΩ pull-down, silence before the program starts |
| 12 V output ACC | `BOARD_PIN_OUT_ACC` | 12 | P-MOSFET switch, high = on; box connector "bottom 3" |
| 12 V output illumination | `BOARD_PIN_OUT_ILL` | 13 | "top 3"; follows the lights bit of frame `036` |
| 12 V output reverse | `BOARD_PIN_OUT_REV` | 14 | "bottom 4" |
| 12 V output spare | `BOARD_PIN_OUT_4` | 21 | "top 4", wire "BRAKE" |
| UART to the radio, TX | `BOARD_PIN_RADIO_TX` | 10 | pad J4 "radio RX"; not used in beside mode |
| UART from the radio, RX | `BOARD_PIN_RADIO_RX` | 11 | pad J4 "radio TX" |
| I2C SDA / SCL | `BOARD_PIN_I2C_SDA`, `BOARD_PIN_I2C_SCL` | 8 / 9 | |
| 12 V sense | `BOARD_PIN_VBAT_SENSE` | 7 | 470 kΩ / 100 kΩ divider, behind the input diode |
| status LED | `BOARD_PIN_STATUS_LED` | 38 | RGB LED (WS2812) on the Waveshare module |
| button | `BOARD_PIN_BUTTON` | 3 | to ground, internal pull-up; strapping pin, see below |

"Bottom 3", "top 7" and so on name the positions of the 2×8 connector of the aftermarket CAN box
harness. UART directions (measured: logger on both data lines): the box transmits on "bottom 8",
the radio transmits on "top 7". The JP_UART jumpers connect `BOARD_PIN_RADIO_TX` to "bottom 8" and
`BOARD_PIN_RADIO_RX` to "top 7". The prototype has 1 kΩ and 4.7 kΩ series resistors on these lines
(measured with a meter); which one sits on which line is not checked yet.

> [!NOTE]
> **The SLNT pin has an internal pull-down**
>
> The S pin of the TJA1051 is pulled to ground inside the chip, so an unconnected S pin means
> "transmitter active" (unverified: NXP TJA1051 datasheet, Rev. 9, section 7.2.2). On the board
> only R3 and the SLNT wire keep it high. Check that wire with a meter. The time the chip needs to
> leave silent mode is not given in the datasheet: not measured yet.

GPIO 3 is a strapping pin of the ESP32-S3. By the ESP32-S3 datasheet it is sampled at reset only
when the eFuse `STRAP_JTAG_SEL` is burned, so with default eFuses the button does not affect
booting (unverified on this unit: read the eFuses with `espefuse.py summary` before you rely on it).

The voltage on GPIO 7 shows the rail **behind** input diode D1, about 0.8 V (0.7-0.9 V) below the battery
(not measured exactly). On USB power alone it shows 0 to about 1.5 V of noise; below 6 V the
console and the panel say "no 12 V".

### Status LED

| Colour | Meaning |
|---|---|
| blue | start |
| off | silence on the bus (with the ignition off the LED stays dark to save current) |
| green | frames are coming |
| yellow | sound is playing |
| red | a driver did not start (outputs stay off), or the CAN controller is not in the running state (a `[can] controller: ...` line appears on every change) |

### Pin rules for any board

For the ESP32-S3 N16R8 module, checked at compile time by `board_common.h`:

- never GPIO 35, 36, 37 (used by the octal PSRAM and flash of the module);
- never GPIO 19, 20 (native USB, the console runs over it);
- GPIO 0, 3, 45, 46 (strapping pins) only with a `BOARD_<PIN>_STRAP_OK 1` macro and a comment that
  explains why the level at reset is safe;
- GPIO 22-25 do not exist on the ESP32-S3;
- one GPIO for one function.

### Add your own board

1. Copy `boards/board_template.h` to `boards/board_<name>.h` and set every `BOARD_PIN_*`.
2. Remove the `#error` line.
3. For the CAN transceiver, pick a silent / standby pin where **high means "transmitter off"**. If
   the pin has an internal pull-down, add an external pull-up.
4. Build without editing `src/`: in `platformio_override.ini` use `-DBOARD=CUSTOM` and
   `'-DBOARD_CUSTOM_HEADER="board_<name>.h"'` (see `platformio_override.sample.ini`, environment
   `listen_mine`).
5. Start with a listen-only environment and check it on the bench:
   [Bench test](../quick-start/bench-test.md).

The sample file also has `board_template_check`, an environment that must fail: it shows that an
unfilled template stops the build.

## Platform

PlatformIO with the Arduino framework, platform `espressif32@8.0.0` (pinned in `platformio.ini`),
board `esp32-s3-devkitc-1` with 16 MB flash. The TWAI (CAN), I2S, ADC and LEDC drivers changed
between platform versions: before you move to another version, check that the functions used in
`src/` exist there. The firmware version number is in `firmware/src/version.h`, not in
`platformio.ini`, because any change of `platformio.ini` rebuilds everything from scratch.

Next: check a new board with the [Bench test](../quick-start/bench-test.md), then flash the
environment you need: [Flash the firmware](../quick-start/flash.md).
