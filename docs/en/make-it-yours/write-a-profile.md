# Write a car profile

A **car profile** tells the firmware how to read one car: the bus speed, which bits are the
steering wheel buttons, where the ignition, reverse gear, lights and parking sensors are. It is
one constant structure in C++ (`vehicle::CarProfile`, defined in `firmware/lib/core/car_profile.h`),
compiled into the program. You choose the profile when you build the firmware; it cannot change
while the device runs.

This page takes you from your decoded frames to a profile that builds. The next page,
[Test your profile](test-your-profile.md), checks it on a recording on your PC.

Every step on this page was tried on a copy of the repository with an invented car. Where that
went wrong, the page says so and shows the message you will see.

> [!CAUTION]
> **Your own profile is listen-only on the car bus**
>
> A profile built with `-DCAR_PROFILE=CUSTOM` can never transmit on the car CAN bus. The
> transmitting environment `decoder_tx_wifi` refuses to build with it and stops with
> `CAN transmit is not allowed with CAR_PROFILE=CUSTOM (listen-only; add a named profile in src/config.h)`.
> Do not try to get around this. Transmission for a new car needs measured frames, content
> checks written in code and tests: see [Transmitting on the CAN bus](../safety/transmit.md).

## What a profile covers

Be clear about this before you start. A profile carries only part of what the device does on the
reference car (Peugeot 508 SW 2015, profile `psa_aee2010`).

| Function | Through your profile | Notes |
|---|---|---|
| Bus speed | yes | `canBitrate` |
| Steering wheel and panel buttons, rotary knobs | yes | tables `keys` and `knobs` |
| Ignition and engine state, reverse gear | yes | function `decodeBody`; drives ACC and the reverse output |
| Lights on | yes | function `decodeLights`; drives the illumination output |
| Parking sensors and the beep from the device speaker | yes | function `decodeParking` |
| Transmit allow list | **no**: the field stays empty (`nullptr, 0`) | a `CUSTOM` profile cannot transmit (see above) |
| Frames that carry the VIN | listed, informational | `privacyIds`; the firmware does not use it |
| Doors, boot and bonnet | yes | function `decodeDoors` -> `vehicle::DoorState` |
| Trip computer | yes, with a limit | function `decodeTrip` -> `vehicle::TripData`. The values keep the reference car's encoding (their meaning is not verified): fill them only if your car uses the same |
| Outside temperature, economy mode | yes | functions `decodeOutsideTemp`, `decodeEcoMode` |
| Climate control state | yes | function `decodeClimate` -> `vehicle::ClimateState` |
| Steering angle | yes, with a limit | function `decodeSteering`; scale not verified on the reference car |
| Vehicle settings (what the radio shows) | yes | function `decodeSettings` -> `vehicle::VehicleSettings` (items with neutral IDs) |
| Changing a vehicle setting from the radio | **no** | only the reference profile `psa_aee2010`, behind the transmit gate |

The car data for the radio screen (doors ... vehicle settings) are optional: leave a pointer
`nullptr` and the radio never gets that data. Each function fills a neutral type from
`firmware/lib/core/vehicle_signals.h`; the radio protocol turns it into its own frame. The section
"Car data for the radio screen" in step 3 lists them.

With the XP radio protocol, a profile of another car therefore gives the radio: the power state
(ACC), button presses, the parking sensor picture, the obstacle flag, and every car data function
the profile fills in. Changing vehicle settings from the radio stays with the reference car.

## Before you start

You need:

- the bit rate, the CAN wires and the bus type of your car ([Find your bus](find-your-bus.md));
- a recording with markers ([Record the bus](record.md));
- the frames and bits you decoded ([Decode frames](decode.md)). Write each one down with its
  status, **measured** or **unverified**, in a copy of `vehicles/_template/README.md`;
- the build tools from [Flash the firmware](../quick-start/flash.md): `uv` and, on Windows, Git Bash.

You do not need every signal. A profile with only the ignition is useful already: it can switch
the radio's ACC. Leave out everything you did not measure.

## Step 1: copy the template

The template `firmware/lib/car_example/` describes an invented car with comments on every field.
It is compiled only by the host tests and never into the firmware.

> [!NOTE]
> **Two invented cars**
>
> The template uses frames `100`, `200`, `300` and `400`. The examples on this page and on
> [Test your profile](test-your-profile.md) come from a **second** invented car, the "tutorial
> car" (frames `2C0`, `1F0`, `350`, `2C8`), made up when these pages were tried out. Its
> recording is not in the repository; the snippets only show what your own values look like.
> In your copy you replace the template values with the frames of your car.

Pick a short name for your profile, in lower case, for example `mine`. Then open Git Bash in the
`firmware` folder: in Explorer, open the folder, right-click and choose "Open Git Bash here" (on
Windows 11 under "Show more options"). Run:

```bash
cp -r lib/car_example lib/car_mine
cd lib/car_mine
mv profile_example.h profile_mine.h
mv profile_example.cpp profile_mine.cpp
cd ../..
```

File names must be unique across all of `firmware/lib/`: every library shares one include path,
and `tools/check_layering.py` reports a header name that exists twice.

## Step 2: rename inside the files

Open the three files in a text editor and rename:

| Where | From | To |
|---|---|---|
| `profile_mine.cpp`, first `#include` | `"profile_example.h"` | `"profile_mine.h"` |
| both files | `kCarExample` | `kCarMine` |
| both files | `namespace example` and `example::` | `namespace mine` and `mine::` |
| `profile_mine.cpp`, first field of the profile | `"example"` | `"mine"` |
| `library.json`, field `name` | `"car_example"` | `"car_mine"` |
| `library.json`, field `description` | the template text | one line about your car |

> [!WARNING]
> **Do not forget `library.json`**
>
> PlatformIO knows the library by the `name` in `library.json`, not by the folder name. The
> `listen` environments ignore a library called `car_example` (`lib_ignore` in
> `firmware/platformio.ini`). With the old name the build fails with
> `Warning! Could not find the package with 'car_mine' requirements` and then
> `fatal error: profile_mine.h: No such file or directory`.

## Step 3: fill in the profile

Work in this order:

1. In `profile_mine.h`, set the four frame IDs `ID_BODY`, `ID_KEYS`, `ID_PARKING` and
   `ID_LIGHTS` to your frames. An ID whose function you set to `nullptr` (point 4) is not read.
2. In `profile_mine.cpp`, rewrite the functions `decodeBody`, `decodeParking` and
   `decodeLights` (sections below).
3. In the same file, fill the `KEYS` and `KNOBS` tables (sections below).
4. At the end of the file, fill the profile constant (table below). Its fields have no names in
   the code, only their order, so keep the comment next to each line.

Want to check the copy and the renames before you change any value? Do the dry run on
[Test your profile](test-your-profile.md) first.

The fields of the profile constant, in order:

| Field | What to put there |
|---|---|
| `id` | short ASCII name, shown in the console (`# profile: car=mine radio=xp`) and in the panel |
| `canBitrate` | the measured bit rate in bit/s |
| `physicalLayer` | `vehicle::PhysicalLayer::UNKNOWN`, `HIGH_SPEED` or `LOW_SPEED_FT`. Informational only: the board decides the transceiver. Leave `UNKNOWN` until you have measured it |
| `keys`, `keyCount` | the button table (below); `nullptr, 0` for none |
| `knobs`, `knobCount` | the knob table (below); `nullptr, 0` for none |
| `txAllow`, `txAllowCount` | `nullptr, 0`. A `CUSTOM` profile never transmits |
| `privacyIds`, `privacyCount` | IDs of frames that carry the VIN in your car, if you know them; `nullptr, 0` otherwise |
| `decodeParking` | your parking sensor function, or `nullptr` |
| `decodeBody` | your ignition and reverse function, or `nullptr` |
| `decodeLights` | your lights function, or `nullptr` |
| `powerOutputs` | `true` only when `decodeBody` reads the ignition reliably; `false` keeps all 12 V outputs off |
| `keyRemapDefaults`, `keyRemapDefaultCount` | `nullptr, 0`: every button reaches the radio with its own code. Rows `{from, to}` (codes of the radio protocol you build with: XP codes with the default protocol, your own codes with `-DRADIO_PROTOCOL=CUSTOM`) only for remappings you want by default; see [Map the radio](radio-mapping.md#default-remappings-come-from-the-car-profile) |

A `nullptr` is always allowed: that signal is then never produced.

### Byte numbers in code start at 0

The documentation and the vehicle pages count CAN bytes from **1** (byte 1 = the first data
byte). In C++ the first byte is `f.data[0]`, and the `byteIndex` field of the tables is the same
index. So a button you wrote down as "byte 1, bit 2" is `byteIndex 0, mask 0x04`. Check this
twice: it is the easiest mistake to make.

### Bit rate

`src/can_bus.cpp` knows these speeds: 50000, 100000, 125000, 250000, 500000 and 1000000 bit/s.
Any other value compiles, but the CAN controller does not start: the console shows `# can=FAIL`,
the status LED turns red, and the 12 V outputs stay off. This is the safe state, not a crash.

### Ignition and reverse: `decodeBody`

The function gets every received frame. It returns `false` for any frame that is not its own, or
that is too short, and `true` after it has filled `out`. Always check the ID and the length
first; never read past the length.

The firmware does not use your car's ignition byte directly. You convert it to the car-neutral
`vehicle::Ignition` (`firmware/lib/core/vehicle_signals.h`):

| Value | Meaning |
|---|---|
| `OFF` | ignition off, or nothing decoded yet |
| `ON` | ignition on, engine not running |
| `ENGINE` | engine running |
| `TRANSITION` | anything else that is not off: cranking, switching off, a value you do not know |

Keep your car's raw value in `ignitionRaw`. It is only shown in the console and the panel.

Example from the invented car of this tutorial (frame `2C0`: byte 2 = ignition, byte 4 bit 6 =
reverse):

```cpp
bool decodeBody(const CanFrame& f, vehicle::BodyState& out) {
    if (f.id != ID_BODY || f.len < 4) return false;   // our frame, long enough?
    out.ignitionRaw = f.data[1];                       // byte 2
    switch (f.data[1]) {
        case 0: out.ignition = vehicle::Ignition::OFF; break;
        case 1: out.ignition = vehicle::Ignition::ON; break;
        case 2: out.ignition = vehicle::Ignition::ENGINE; break;
        default: out.ignition = vehicle::Ignition::TRANSITION; break;
    }
    out.reverseGear = (f.data[3] & 0x40) != 0;         // byte 4 bit 6
    return true;
}
```

How the generic logic uses it (`firmware/lib/core/power_policy.h`):

- ACC is on in every state except `OFF`, so cranking does not switch the radio off.
- ACC goes off 2.5 s after the ignition goes to `OFF`.
- The frame must keep coming. If it is missing for more than 1 s, the ignition counts as off
  ([how to measure the period](decode.md#how-often-a-frame-comes)).
  On the reference car the ignition frame `0F6` comes every 500 ms (measured: recording
  `can_20261111_191045.log`). If your ignition frame is slower than once per second, the 12 V
  outputs will drop.
- The reverse output is on only with the ignition on and a fresh frame.

### Lights: `decodeLights`

Same pattern: check the ID and length, set `lightsOn`, return `true`. The illumination output is
on only when the ignition is on **and** a lights frame arrived in the last second
([how to measure the period](decode.md#how-often-a-frame-comes)). A car that
sends its lights state only when it changes will lose the illumination output after one second.

### Parking sensors: `decodeParking`

Fill a `vehicle::ParkingState` with the same meaning as on the reference car:

| Field | Meaning |
|---|---|
| `rearStatus`, `frontStatus` | `4` (`PARK_ACTIVE`) = working. The beeper uses only a side with status 4. Other values: 1 fault, 2 off by the driver, 5 waiting, 6 out of service |
| `rearLeft` ... `frontRight` | zones 0 to 7: 0 = very close, 7 = far or no obstacle. Clamp your values to 0..7 |
| `soundRequest`, `soundFront` | the car asks for sound; from the front (`true`) or the rear |
| `intervalRaw` | `0x00` = continuous tone, `0x3F` = no beeps, values in between = closer and closer |

Start with `out = vehicle::ParkingState();` so that every field you do not fill has its
"nothing" value (zones 7, status 0).

What the device does with it (`firmware/lib/core/parking_beeper.h`):

- it beeps from the first zone below 7 on an active side, even if the car does not ask for sound;
  the tempo comes from the nearest zone;
- it plays the continuous tone **only** when `soundRequest` is set and `intervalRaw` is `0x00`.
  If your car does not report an interval, compute one from the zone, as the template does;
- it goes silent when no parking frame arrived for 400 ms. On the reference car the frame comes
  every 100 ms.

### Car data for the radio screen

Seven optional functions, all with the same rules: check the ID and the length first, fill the
neutral type, set its `valid` flag, return `true` (or the page / change bits). `firmware/lib/car_example/profile_example.cpp`
shows `decodeDoors` and `decodeTrip`.

- `decodeDoors` -> `DoorState`: `open` holds the bits `DOOR_DRIVER`, `DOOR_PASSENGER`,
  `DOOR_REAR_LEFT`, `DOOR_REAR_RIGHT`, `DOOR_BOOT`, `DOOR_BONNET`; `anyOpen` = the car's door
  warning. Return `true` only when the frame says something about the doors.
- `decodeTrip` -> `TripData`: return `TRIP_NOW`, `TRIP_1` or `TRIP_2` for the page the frame updated.
- `decodeOutsideTemp` -> `OutsideTemp`, in half degrees Celsius.
- `decodeEcoMode` -> `EcoMode`: the car is about to cut the accessories.
- `decodeClimate` -> `ClimateState`: temperatures in half degrees (0 = no value), fan 0..15,
  `AIR_*` flags.
- `decodeSteering` -> `SteeringAngle`: raw value in the reference car's encoding.
- `decodeSettings` -> `VehicleSettings`: per item `at(SettingId::...)` the `value`, `known` (your
  latest frame stated it) and `present` / `presentKnown` (the car has the item). Return
  `SETTINGS_VALUES` and / or `SETTINGS_PRESENT`.

Functions with an `io` argument (trip, climate, doors, settings) get the state the decoder keeps
between frames. Change only what your frame says, so that several frames can build one state.

### Buttons: the `keys` table

One row is one button that is "pressed while a bit pattern is present":

```cpp
{canId, byteIndex, mask, value, KeyId::...}
```

The button counts as pressed while `(data[byteIndex] & mask) == value` in frame `canId`. Rules
from the code (`firmware/lib/core/car_profile.h`, `firmware/lib/app/xp_keys.h`):

- the order of rows is the order of matching and the order of events for the radio;
- at most 32 rows are used;
- a frame shorter than `byteIndex + 1` is skipped for that row;
- a row whose pattern is present when nothing is pressed reports a button held all the time. With
  `value` = `0x00` this happens easily; give every row a pattern that only a press produces;
- press = one event at once, then every 200 ms "held" while the pattern stays, then "released".

Two ways a car can encode buttons, both seen in practice:

```cpp
// one bit per button (template car: byte 1 bit 0 = volume +, bit 1 = volume -)
{0x200, 0, 0x01, 0x01, KeyId::VOLUME_UP},
{0x200, 0, 0x02, 0x02, KeyId::VOLUME_DOWN},

// a button number in a group of bits (tutorial car: byte 1 bits 3-0 = 1, 2, 3, ...)
{0x1F0, 0, 0x0F, 0x01, KeyId::VOLUME_UP},
{0x1F0, 0, 0x0F, 0x02, KeyId::VOLUME_DOWN},
```

The reference profile uses the second way too: in frame `21F` byte 1, the mask `0C` gives volume +
(`08`), volume - (`04`) and both together, mute (`0C`) (measured: recordings
`can_20261111_190817.log` and `can_20261111_191045.log`, CAN and the CAN box line to the radio on
one clock).

The names (`KeyId`) are logical buttons. Which code the radio gets for each name is the job of the
radio protocol: see [Map the radio](radio-mapping.md).

### Rotary knobs: the `knobs` table

A knob is an 8-bit counter in one byte of a frame:

```cpp
{canId, byteIndex, onIncrease, onDecrease}
```

A counter step of +1 is one detent `onIncrease`, -1 is one detent `onDecrease`. At most 4
detents are reported per frame, and at most 4 knob rows are used. The first frame only sets the
starting value. If your knob counts down when you turn it "forward", swap the two names, as the
reference profile does for frame `21F`.

## Step 4: build it

Your profile is selected with three build flags, without editing anything in `src/`. The file
`firmware/platformio_override.sample.ini` already has an environment for a profile called
`mine`:

1. Copy `firmware/platformio_override.sample.ini` to `firmware/platformio_override.ini`. The new
   file is not tracked by Git; `platformio.ini` loads it automatically.
2. If your profile is not called `mine`, change the `listen_mycar` section so that it reads like
   this, with your name in place of `<name>` and `<Name>`:

    ```ini
    [env:listen_mycar]
    extends = env:listen_wifi
    lib_deps = car_<name>
    build_flags =
        ${env:listen_wifi.build_flags}
        -DCAR_PROFILE=CUSTOM
        '-DCAR_PROFILE_CUSTOM_HEADER="profile_<name>.h"'
        -DCAR_PROFILE_CUSTOM_OBJECT=kCar<Name>
    ```

    `lib_deps` is needed: PlatformIO does not follow an `#include` that comes through a macro.
    The `listen_mine` section in the same file is for your own board, not your car; leave it.
3. Build, from the `firmware` folder:

    ```bash
    uvx platformio run -e listen_mycar
    ```

    Expected: the build ends with `SUCCESS`, and the dependency list near the top shows
    `|-- car_mine @ 1.0.0`. Other messages: see [Common mistakes](#common-mistakes).
4. Flash it and open the console:

    ```bash
    uvx platformio run -e listen_mycar -t upload
    uvx platformio device monitor
    ```

    Expected: `# profile: car=mine radio=xp` at start-up. Port and bootloader mode:
    [Flash the firmware](../quick-start/flash.md).

`listen_mycar` is a listen-only build "beside the box": it decodes your car, beeps the parking
sensors from the device speaker and prints console lines, but it sends nothing to the radio and
keeps the 12 V outputs off. In the console, type `keys` to print every button event, `dump` to
print every CAN frame.

### Decoder mode with your profile

To drive the radio (UART frames, ACC, illumination and reverse outputs), add a second section to
your `platformio_override.ini`. This one was built successfully in the tutorial run:

```ini
[env:decoder_mycar]
extends = env:decoder_wifi
lib_deps = car_mine
build_flags =
    ${env:decoder_wifi.build_flags}
    -DCAR_PROFILE=CUSTOM
    '-DCAR_PROFILE_CUSTOM_HEADER="profile_mine.h"'
    -DCAR_PROFILE_CUSTOM_OBJECT=kCarMine
```

> [!CAUTION]
> **Decoder mode drives the radio and the 12 V outputs**
>
> Flash a decoder build only after the profile passed its test on the PC and on the bench, and
> only when the aftermarket CAN box is unplugged: two transmitters on one UART line and two
> sources of ACC means outputs working against each other. Read
> [Radio connection](../install-in-car/radio-connection.md) and
> [First drive](../install-in-car/first-drive.md) first. On the car CAN bus this build still
> only listens.

### What does not build, on purpose

| Attempt | Result |
|---|---|
| `decoder_tx_wifi` with `-DCAR_PROFILE=CUSTOM` | `#error "CAN transmit is not allowed with CAR_PROFILE=CUSTOM ..."` (tried in the tutorial run) |
| `decoder_tx_wifi` with `-DCAR_PROFILE=GENERIC` | `#error "CAN transmit is not allowed with CAR_PROFILE=GENERIC ..."` (checked by `tools/check_tx_profile_lock.sh`) |
| `-DCAR_PROFILE=EXAMPLE` in any environment | `#error "CAR_PROFILE=EXAMPLE is a template for the host tests only ..."` |
| `CUSTOM` without the header or object flag | `#error "CAR_PROFILE=CUSTOM needs ..."` |

## Do not start from the reference profile

You could copy `firmware/lib/car_psa_aee2010/` instead of the template. Know what happens then: if
your profile keeps the reference functions, it decodes the reference car's frame IDs (doors,
settings, trip computer, climate). If it also keeps `psa::decodeBodyState` for the ignition, the
firmware treats your car as a PSA AEE2010 car for the settings change and the transmit conditions
too (`firmware/lib/app/decoder.cpp`, `Decoder::begin`). That is only right for a car that has been
measured to use the same frames. Treat every frame of another
car as **unverified** until you have recorded it, even on a close relative of the reference car.

## Common mistakes

| Symptom | Cause |
|---|---|
| `fatal error: profile_mine.h: No such file or directory` | `name` in `library.json` not changed, or `lib_deps` missing |
| `# can=FAIL` at start-up, red LED | `canBitrate` is not one of the six supported speeds |
| a button reported as held all the time | its `mask` / `value` pattern is present at rest |
| buttons on the wrong bit | `byteIndex` counted from 1 instead of 0 |
| illumination output drops after 1 s | the lights frame is not repeated at least once per second |
| no beep, but zones change | the side's status is not `4` (`PARK_ACTIVE`) |

Next: [Test your profile](test-your-profile.md).
