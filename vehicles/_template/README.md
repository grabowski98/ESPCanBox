# <Make> <Model> (<year>): vehicle page template

Copy this directory to `vehicles/<make>-<model>-<year>/` and replace everything in angle brackets.
Delete rows you did not measure instead of guessing.

## Rules

- Every fact has a status: **measured** (how, and which recording) or **unverified** (the source:
  a public database, a forum, a seller's drawing).
- Recordings: listen-only (sniffer or the `generic` profile), one action per step, each action
  repeated three times with a pause. Before sharing, run `tools/sanitize_capture.py` (on PSA cars
  frames `336`, `3B6`, `2B6` carry the VIN; on other makes find the VIN frames first) and
  `tools/scan_private.py`. Read the comment lines too.
- No personal data: no VIN, registration number, serial numbers, names, home location.
- Do not copy code or tables from projects with an incompatible licence. Link them as a source.

## Car

| Item | Value | Status |
|---|---|---|
| Model, year, engine, gearbox | | documents |
| Electrical architecture / platform | | |
| Factory radio and what replaced it | | |
| Relevant equipment (parking sensors, Stop and Start, ...) | | |

## Bus at the radio

| Fact | Value | Status |
|---|---|---|
| Connector and pins of CANH / CANL | | measured: continuity + reception |
| Bit rate | | measured: reception without errors |
| Physical layer (high-speed or low-speed fault-tolerant) | CANH = ... V, CANL = ... V to ground | multimeter / oscilloscope |
| Terminator on the radio side | | resistance CANH-CANL unpowered |
| Bus silent with key out after ... s | | recording |
| Frames carrying the VIN | | recording |

## Signals

| Signal | Frame, byte.bit, values | Status | How confirmed (recording, step) |
|---|---|---|---|
| ignition / engine | | | |
| reverse | | | |
| lights | | | |
| parking sensors | | | |
| steering wheel buttons | | | |
| doors | | | |

## Radio side

| Item | Value | Status |
|---|---|---|
| CAN box protocol the radio expects | | |
| UART lines (which wire transmits), level, bit rate | | |
| ACC / ILL / REV wires | | |

## Firmware profile

- Start from `firmware/lib/car_example/` (invented frames, comments on every field): copy it to
  `firmware/lib/car_<name>/`, rename the files and the `kCar...` object, and change `"name"` in its
  `library.json` to `car_<name>` (`lib_deps` finds the library by that name).
- Build it without editing `src/`: in `firmware/platformio_override.ini` (copy of
  `platformio_override.sample.ini`) set `-DCAR_PROFILE=CUSTOM`,
  `-DCAR_PROFILE_CUSTOM_HEADER="profile_<name>.h"`, `-DCAR_PROFILE_CUSTOM_OBJECT=kCar<Name>` and
  `lib_deps = car_<name>`. A `CUSTOM` profile is listen only (`src/config.h` stops a build with
  transmission).
- Ignition goes into the neutral `vehicle::Ignition` (`firmware/lib/core/vehicle_signals.h`), not into
  the byte encoding of another car.

## Files

- `<car>.dbc`: frames and signals, each comment starting with its status;
- `wiring.md`: connector views and pin tables, with the side you looked from;
- recordings used by tests go to `firmware/test_data/captures/` with a line in its README.

## Not known yet

- <list what you could not measure>
