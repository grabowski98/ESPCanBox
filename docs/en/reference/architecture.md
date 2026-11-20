# Architecture

How the firmware is split into parts, how one CAN frame travels through it, and why it is built
this way. Read this before you write a car profile or change the code. Paths are relative to
`firmware/` in the repository.

## The big picture

The firmware has two kinds of code:

- **logic** in `lib/`: plain C++ with no Arduino or ESP-IDF header. It gets frames and the time in
  milliseconds, and it returns decisions (bytes for the radio, states of the 12 V outputs, sound).
  The same files are compiled on a PC and tested against recordings from the car
  ([Host tests](../testing/host-tests.md));
- **hardware** in `src/`: the CAN controller, the UART to the radio, the 12 V outputs, sound,
  settings in flash (NVS), the USB console, Wi-Fi and the web panel.

Two choices are made when you build, never at run time:

- the **car profile** (`-DCAR_PROFILE=...`): what the CAN frames of your car mean;
- the **radio protocol** (`-DRADIO_PROTOCOL=...`): how to talk to your radio.

The pins come from a third choice, the **board header** (`-DBOARD=...`, files in `boards/`). See
[Configuration and pins](config-and-pins.md).

## Libraries in lib/

Every library is one directory. Its name gives its layer.

| Library | Layer | What is inside |
|---|---|---|
| `lib/core` | core | car- and radio-independent logic: CAN frame type, candump parser, neutral signal types (`vehicle_signals.h`), the profile and protocol structures (`car_profile.h`, `radio_protocol.h`), the session with the radio common to every protocol (`radio_session.*`: queue, sending on change, repeats), parking beeper, radio power policy (ACC, reverse), CAN transmit gate and arming (`can_tx_gate.*`, `can_tx_arm.*`), firmware identity marker, web panel helpers (JSON writer, login, line ring, frame trace), small loop and network policies |
| `lib/car_psa_aee2010` | car | the reference profile (Peugeot 508 SW 2015): CAN IDs, frame decoding, the car data for the radio screen (`psa_signals.*`), car state, vehicle settings (`260`, `15B`, `361`), the settings change sequence, the transmit allow list |
| `lib/car_generic` | car | the entry point for an unknown car: decodes nothing, no buttons, empty transmit allow list, 12 V outputs never on |
| `lib/car_example` | car | a commented template with **invented** frames. Compiled only by the host tests, never into the firmware |
| `lib/radio_xp` | radio | the XP protocol: framing and parser (`xp_protocol.*`), the protocol object `kRadioXp` with the power, key, obstacle and version frames, the decoding of the radio frames and the protocol tables (`protocol_xp.*`), parking frame, key code table, the encoders of the car data for the radio screen (`xp_signals.*`), the key remap table, names of radio commands, `RadioLink` (the common session with XP, for the 0.6.0 reference and the tests) |
| `lib/radio_none` | radio | no radio: no UART, nothing is ever sent |
| `lib/radio_example` | radio | a commented template protocol with an **invented** framing and frames. Compiled only by the host tests, never into the firmware |
| `lib/app` | app | the glue: the `Decoder` object, plus the parts that need both sides (the key table in `xp_keys.*`). `xp_car.*` and `xp_settings.*` are the earlier PSA -> XP functions, kept for the frozen 0.6.0 reference and the host tests |

The `Decoder` (`lib/app/decoder.h`) holds all the logic of one running device. `src/main.cpp`
creates one `Decoder`, gives it the selected profile and protocol, and feeds it.

### The two structures

A car profile is one `const` structure, `vehicle::CarProfile` (`lib/core/car_profile.h`), compiled
into flash:

| Field | Meaning |
|---|---|
| `id` | short name, for example `"psa_aee2010"`, printed on the console |
| `canBitrate` | bus speed in bit/s. `src/can_bus.cpp` knows 50, 100, 125, 250, 500 and 1000 kbit/s; any other value means the controller is not started (safe state) |
| `physicalLayer` | high-speed or low-speed fault-tolerant, information only. The firmware does not switch transceivers. Both shipped profiles say `UNKNOWN` |
| `keys`, `knobs` | buttons (frame, byte, mask, value -> logical key) and rotary knobs (8-bit counter -> two keys). Row order = matching order |
| `txAllow` | the frames the transmit gate may send, each with a length and a content check written in C++. Empty for a listen-only profile |
| `privacyIds` | frames that identify the car (for example the VIN), masked before a recording is shared |
| `decodeParking`, `decodeBody`, `decodeLights` | functions that turn a frame into a neutral signal: parking sensors, ignition and reverse, lights. `nullptr` = this car does not provide it |
| `powerOutputs` | `true` = ignition is decoded well enough to drive the 12 V outputs in decoder mode |
| `keyRemapDefaults` | the starting entries of the radio key remap table (XP codes), restored by `map reset`. Only the reference profile has any |
| `decodeSteering`, `decodeTrip`, `decodeOutsideTemp`, `decodeEcoMode`, `decodeClimate`, `decodeDoors`, `decodeSettings` | the car data for the radio screen: functions that turn a frame into a neutral type (`SteeringAngle`, `TripData`, `OutsideTemp`, `EcoMode`, `ClimateState`, `DoorState`, `VehicleSettings` in `vehicle_signals.h`). Each type has a validity flag. Functions with an `io` argument get the state the `Decoder` keeps and change only what the frame says. `nullptr` = this car does not provide it |

A radio protocol is one `const` structure, `RadioProtocol` (`lib/core/radio_protocol.h`): `id`,
`uartBaud` (0 = no UART at all), `keyCode`, the table from a logical key to the code the radio
expects, and the encoders of the car data (`encodeSteering`, `encodeTrip`, `encodeOutsideTemp`,
`encodeEcoMode`, `encodeClimate`, `encodeCarState`, `encodeSettingsPresent`). An encoder gets a
neutral type and writes one radio frame; it never sees a CAN frame. `nullptr` = the radio does not
show this data. `encodeCarState` gets its previous frame back, so a field the car did not state
keeps its last value. The rest of the structure is the line and the session: `encodeFrame` (one
frame into bytes, with the protocol's header, length and checksum; `nullptr` = nothing is sent),
`rxParser` (the protocol's parser of the bytes from the radio), `sendAck` and `ackByte` (the byte
sent back after every valid radio frame), `txGapMs`, the frames for power, keys, parking sensors
(with `parkingRepeatMs`), obstacle and version, `decodeRadioFrame` (radio state for the console and
the panel, and the request "send everything"), `decodeCommand` (commands from the radio; `nullptr` =
none reach the CAN side), and two `const` tables: `latchKeys` (keys that toggle, XP: `A4`) and
`rateLimits` (state frames sent at most once per interval, XP: steering angle `29`, 150 ms). The
common session (`lib/core/radio_session.h`) and the `Decoder` use only these fields.

Selection happens in `src/config.h`:

| Build flag | Profile or protocol |
|---|---|
| `-DCAR_PROFILE=PSA_AEE2010` (default) | `lib/car_psa_aee2010` |
| `-DCAR_PROFILE=GENERIC` | `lib/car_generic` (used by `listen_generic_wifi`) |
| `-DCAR_PROFILE=CUSTOM` | your own profile, given with `CAR_PROFILE_CUSTOM_HEADER` and `CAR_PROFILE_CUSTOM_OBJECT`. Listen only |
| `-DRADIO_PROTOCOL=XP` (default) | `lib/radio_xp` |
| `-DRADIO_PROTOCOL=NONE` | `lib/radio_none` |
| `-DRADIO_PROTOCOL=CUSTOM` | your own protocol, given with `RADIO_PROTOCOL_CUSTOM_HEADER` and `RADIO_PROTOCOL_CUSTOM_OBJECT`. Listen only on CAN |

`-DCAR_PROFILE=EXAMPLE` and `-DRADIO_PROTOCOL=EXAMPLE` stop the build on purpose: the examples are
templates, not a car and a radio. How to write your own profile:
[Write a profile](../make-it-yours/write-a-profile.md); your own radio protocol:
[Map the radio](../make-it-yours/radio-mapping.md#radio-protocols-available).

## How one CAN frame travels

One pass of the main loop (`loop()` in `src/main.cpp`) does these steps in this order:

1. **Receive.** `canReceive()` empties the receive queue of the CAN controller (`src/can_bus.cpp`,
   queue of 128 frames). For each frame `handleFrame()` runs:
    - the web panel gets a copy for its live view (Wi-Fi builds only);
    - with `dump` switched on, the frame is printed on USB as `(time) can0 ID#DATA`;
    - `Decoder::onCanFrame()` gets the frame.
2. **Decode.** Inside `onCanFrame()` the profile turns the frame into neutral signals: buttons
   from the key and knob tables, parking sensors, ignition and reverse, lights, and the car data for
   the radio screen (steering angle, trip computer, outside temperature, economy mode, climate,
   doors, vehicle settings). For the car data `onCanFrameCarData()` hands each neutral value to the
   encoder of the radio protocol. It names no CAN ID and no radio frame.
3. **Console and panel work.** USB commands, delayed settings writes and web panel requests are
   handled, except while the CAN transmitter window is open (see
   [Transmitting on the CAN bus](../safety/transmit.md)).
4. **Tick.** `Decoder::tick()` decides, in this order: button events -> radio power (ACC,
   illumination, reverse) -> frames for the radio -> parking sound. It returns the result to
   `main.cpp`.
5. **Outputs.** `main.cpp` sets the 12 V outputs, but only in decoder mode, with a working CAN
   driver and with a profile that has `powerOutputs = true`. Otherwise all outputs are off.
6. **Radio.** `radioUartPoll()` (`src/radio_uart.cpp`) reads bytes from the radio through the
   protocol's parser, answers each complete frame with the protocol's acknowledgement (XP: `F3`),
   gives it to `Decoder::onRadioFrame()`, and sends the next frame from `Decoder::nextRadioTx()`,
   framed by the protocol (at least `txGapMs` between frames, XP: 10 ms). With radio `none` nothing
   is sent.
7. **Sound, LED, sleep.** The speaker plays what the parking beeper decided; the device goes to
   sleep after 30 s of a quiet bus when no computer is connected to USB.

A command from the radio goes the other way: `onRadioFrame()` -> the protocol's `decodeCommand`
(XP: radio command parser) -> for a vehicle setting (XP `80 nn vv`) the settings changer -> the CAN transmit gate -> the hardware port in
`src/can_tx/`. In every build without transmit the gate has no port and refuses. Details:
[Transmitting on the CAN bus](../safety/transmit.md).

## Layer rules

`tools/check_layering.py` checks the `#include` lines of every source file in `lib/` and `include/`
(any extension except documentation and `library.json`; comments are removed first). Run it with
`python -I tools/check_layering.py` in `firmware/`; exit code 0 means no violation.

| Layer | May include |
|---|---|
| `core` | only `core` |
| `car_*` | `core` and itself |
| `radio_*` | `core` and itself |
| `app` | everything in `lib/` |

The script also fails when:

- a radio library includes a car file (in particular `can_ids.h`), or a car library includes a
  radio file or `app`;
- a file in `lib/` includes a framework or SDK header (`Arduino.h`, `esp_*.h`, FreeRTOS ...);
  only C standard headers, files in `lib/` and `include/` are allowed;
- a file in `lib/` includes a file from `src/` or from outside the firmware;
- the same header name exists in two libraries (they share one include path);
- an include goes through a macro (the target cannot be checked);
- a header in `include/` includes anything but `core`, `include/` or a C standard header (it follows
  the `core` rules, so it cannot pass a car or radio header on);
- code names the other side (comments and strings removed, `#define` lines included): the word `xp`,
  `BOX_*` or `RADIO_*` in `lib/car_*`; the words `psa` or `vset`, `ID_*` or `CanFrame` in
  `lib/radio_*`; the words `xp`, `psa` or `vset` in `lib/core` and `include/` (one reviewed
  exception: the old-name aliases in `lib/core/compat/legacy_060.h`);
- the radio path of the `Decoder` (`lib/app/decoder.*`) names the XP framing, parser or frame
  numbers (`xp::encode`, `xp::Parser`, `xp::BOX_*`, `xp::RADIO_*`, `parseRadioCmd` ...): every frame
  to or from the radio goes through `RadioProtocol`.

It does not follow includes further than one step, and it does not check CAN IDs written as plain
numbers inside `lib/radio_*`. Keep them out by hand; review still matters.

## Design decisions

| Decision | Why |
|---|---|
| Everything is chosen at build time, profiles are `const` | the program cannot be talked into another car or another transmit list at run time. The only run-time table is the radio key remap table |
| No dynamic memory, no `String`, no `delay()` in the loop | the device runs for hours in a car; fixed buffers cannot fragment or run out |
| Logic without hardware headers | the same code runs on a PC against real recordings, so a change is checked before it reaches the car |
| One place that can transmit on CAN (`src/can_tx/can_tx_port.cpp`), compiled only in `decoder_tx_wifi` | the five other environments do not contain the transmit function at all (`tools/check_no_can_tx.sh` checks the built programs) |
| Disarmed = listen-only controller, armed = transmitter closed except for one frame | the device stays invisible to the car outside a few milliseconds of a send. See [Transmitting on the CAN bus](../safety/transmit.md) |
| A driver that fails to start leaves the device in a safe state (12 V outputs off, red LED), with no restart loop | the radio and the car must not get ACC pulses |
| The USB line format `can0 ID#DATA` and comment lines starting with `#` are fixed | tools read them; see [Versioning](versioning.md) |
| The XP stream to the radio is compared byte for byte with recordings of the original box | golden tests keep the behaviour unchanged while the code is reorganised ([Golden files](../testing/golden-files.md)) |

## What is not generic yet

The split into profiles is done in the careful way, so that the stream to the radio stays the
same. The split in this version:

- buttons, parking sensors, ignition, lights, the bus speed, the transmit allow list and the car
  data for the radio screen (steering angle, trip computer, outside temperature, economy mode,
  climate, doors, vehicle settings) go through the car profile and the radio protocol;
- the trip computer values and the steering angle keep the reference car's encoding: their meaning
  and scale are not verified, so the neutral types cannot give them units yet;
- changing a vehicle setting is not generic: the radio command `80`, frame `15B` / `1A9` and the
  check in frame `260` stay PSA code in the `Decoder` and in `settings_changer.*`, behind the
  transmit gate. The car state they read (`CarModel`, frames
  `260`, `361`, `0F6`) runs only with the PSA frame set;
- the radio side goes through the radio protocol (framing, parser, acknowledgement, every frame,
  the latch keys and the rate limits), but what the commands from the radio mean is not generic:
  the setting numbers of `decodeCommand` are the XP numbers of the vehicle settings table, so a
  `CUSTOM` radio protocol builds only without CAN transmit (the climate and sound commands only
  print a console line);
- the key event types and the key remap table keep their `xp::` names (`xp::KeyEncoder`,
  `xp::KeyCustomizer`), although they work with the codes of any protocol;
- the default key remaps (`21` -> `1F`, `9F` -> `9B`) are in the PSA profile (`keyRemapDefaults`) as XP
  codes: the one place where a car profile names radio codes;
- the settings change sequence (`settings_changer.*`) lives in the PSA profile library, not in
  `core`;
- the hardware code in `src/` (CAN, transmit port, radio UART, main loop) has no PC test. It is
  checked on the bench ([Bench test protocol](../testing/bench-test-protocol.md)).

Which button sends frame `0A2` (SRC, or the one between SRC and LIST) has two different answers in
the notes of the project and still has to be checked in the car.
