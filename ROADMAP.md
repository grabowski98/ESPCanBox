# Roadmap

Ideas and planned work, roughly in order. Nothing here has a date: this is a hobby project. An
item moves to [CHANGELOG.md](CHANGELOG.md) when it is done. If you want to work on one, open an
issue first so the work is not done twice.

## Before the first public release (2.0.0-beta.1)

- **Bench test protocol** on a real board for this tree: flashing over USB, `ver`, `stat`,
  frames received, UART to the radio, 12 V outputs off in listen mode, `cantx` without arming,
  panel in two languages, update over Wi-Fi.
- **Continuous integration**: the workflow files exist (`.github/workflows/ci.yml`,
  `release.yml`, see [Continuous integration](docs/en/testing/ci.md)) but have not run on GitHub
  yet; the first run, including the host tests with `g++`, comes with the first push.
- **New board** (see [Future boards](docs/en/hardware/future-boards.md)); the first public
  release waits for it.
- **Documentation translations**: Polish in full; German, Spanish, Russian and Ukrainian for the
  start pages, quick start, safety and "make it yours", reviewed by native speakers.
- Content check for frame `15B` (the check covers only ID and length), with a test of the
  transmit path.
- **Licences of the binary images**: the LGPL-2.1 parts of the Arduino core and the ESP-IDF
  components in the build are checked one by one, and what a distributor of a binary image owes
  is written down, before any image is published ([NOTICE](NOTICE), section 1).

## Car and radio support

- **One interface for the rest of the car data.** The car data for the radio screen (steering
  angle, trip computer, outside temperature, economy mode, climate, doors, vehicle settings) now
  go through neutral types in `firmware/lib/core/vehicle_signals.h`: decoded by the car profile,
  encoded by the radio protocol, with the stream patterns identical byte for byte.

  **Work status:** reading done. Still open: (1) changing a vehicle setting from the radio
  (command `80` -> `15B` / `1A9` -> check in `260`) is PSA code behind the transmit gate; a
  generic form needs the change sequence and its car state behind the profile, which touches the
  transmit path and waits for a separate review; (2) the trip computer values and the steering
  angle keep the reference car's encoding until their meaning and scale are measured; (3) the
  meaning of the commands from the radio is still the reference pair's: `decodeCommand` gives XP
  setting numbers (the `vset` table), so a `CUSTOM` radio protocol builds only without CAN transmit. A neutral setting command (`vehicle::SettingId` instead of the XP
  number) waits for the review of the transmit path; until then a protocol without
  `commandsUseVsetNumbers` gets its commands reported as "not supported", never interpreted.
- **Panel and status for any car and radio**: the web panel always gets the PSA AEE2010 frame names
  and VIN frames, shows the radio source with XP source codes and labels the car state frame as
  "frame 38"; the outside temperature in the panel comes only from the PSA frame `0F6`. The radio
  protocol has no frames for reverse, lights or ignition over UART, and the car-neutral types have no
  speed, engine speed or handbrake; a key code `00` cannot be sent.
- **Neutral names for the key path**: `xp::KeyEncoder` (`firmware/lib/app/xp_keys.*`) and
  `xp::KeyCustomizer` (`firmware/lib/radio_xp/xp_keys_custom.*`) work with the codes of any protocol
  but keep XP names and live in the XP library; move them to `lib/core` under neutral names. The
  console preview of the radio frames (`src/radio_uart.cpp`) prints the XP frames `A6` and `C0` only
  on change; other protocols print every frame.
- **`TripData` with meaning and units.** The trip computer values (`TripPage` fields `a`, `b`, `c`
  and `now[3]` in `firmware/lib/core/vehicle_signals.h`) keep the reference car's raw encoding;
  named fields with units (speed, distance, consumption) wait until their meaning and scale are
  measured.
- **Other radio protocols** (for example Raise / RZC, HiWorld), written only from your own UART
  recordings, never from other people's code with an incompatible licence.
- Other PSA AEE2010 cars: recordings and profiles from owners.
- **The remaining unverified bits** of the frames to the radio (door positions in XP frame `38`, the
  bits of `21`, `38` and `3C` that never changed in a recording, listed in
  [radios/xp-simple-soft/provenance.md](radios/xp-simple-soft/provenance.md#logic-that-is-not-a-single-constant)):
  record them with photos of the radio screen, or drop them. On the CAN side: the fan level of `350`
  with a panel press, and the radio sound row `84 01` ("amplifier switch" on the screen, not
  clicked yet).

## Tools

- **Profile generator**: a car profile described in a TOML or JSON file, turned into the C++
  header at build time; it refuses a transmit list without a content check in code.
- **Profile overlay in NVS**: receive-side changes (key map, frame names) without rebuilding.
  Rule: it can never add a frame to the transmit list.
- **Profile editor in the web panel**, on top of the overlay.
- DBC import and export for SavvyCAN, and a view of changing bits in the panel.
- Browser installer (ESP Web Tools) for the listen-only images, after the first release built in
  CI.

## Hardware

- A board made by a PCB factory, with production files, replacing the home-made prototype.
- Hardware listen-only switch that the firmware cannot override, kept on every board.
- Ports for audio and sleep behind interfaces, for boards with other parts.

## Not planned

- Ready-made firmware images that transmit on the car bus.
- Anything that writes to the car's control units (diagnostics, coding).
- Using decompiled code or firmware of box or radio makers.
