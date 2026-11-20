# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html). What counts as a breaking change
(console line format, panel API, firmware marker) is described in
[docs/en/reference/versioning.md](docs/en/reference/versioning.md).

## [2.0.0] - 2026-11-20

First public release: `FW_VERSION` in `firmware/src/version.h` is `2.0.0`, shown as such by the
console (`ver`), the panel and the names of the release files. It was prepared as the release
candidate `2.0.0-rc.1`; every entry of that candidate is in this section. It is a new MAJOR against
the private prototype firmware 1.0.0 that ran in the reference car before it, because the contracts
listed below changed.

**Status: bench-tested and tried in one car.** This tree compiles in every environment and passes
the host tests (the radio and output patterns are identical byte for byte with firmware 1.0.0). It
was bench-tested on an ESP32-S3-WROOM-1-N16R8 board
([bench test protocol](docs/en/testing/bench-test-protocol.md)). In-car trial on 2026-11-20 in
one car (Peugeot 508 SW 2015 with one Android head unit using the XP protocol): bus reception
complete with no lost frames and no bus errors, radio link, ACC timing, steering-wheel and panel
keys, doors, climate display, reverse and parking-sensor beeps, vehicle settings through frame
`15B` (six rows, both directions), deep sleep and wake-up from the bus. The experimental `1A9`
button frames (parking sensors, blind spot) and the optional frame after a press were exercised
only in an experimental build that is not part of the release images. **Not exercised in the
car:** the transmit guards (3.5 s after ignition-on, cranking, reverse); they are covered by host
tests only.

### Breaking changes against the prototype firmware 1.0.0

- **Product name and firmware marker**: firmware product `espcanbox`, marker `ESPC-FWID`, Wi-Fi
  `ESPCanBox-XXXX`, mDNS `espcanbox.local`, NVS namespace `espcanbox`. The Wi-Fi update refuses the
  prototype image in both directions: flash over USB once; the settings start empty.
- **Environment names** in English: `listen`, `decoder`, `listen_wifi`, `decoder_wifi`,
  `decoder_tx_wifi` (plus the new `listen_generic_wifi`); the old names are only comments in
  `firmware/platformio.ini`.
- **Console tags**: `[ust]` -> `[vset]`, `[klima]` -> `[climate]`, `[dzwiek]` -> `[sound]`; texts of the
  lines in English. Names in the lines: radio climate button `8A 09` is "fan" (was "intensity level"),
  the sound rows are "surround", "fader", "balance", "bass", "treble", "sound selection",
  "volume / loudness" (were "sound no. N"); `80 04`, `80 08`, `80 0E` print "unknown setting", and
  `80 0D` (Start/Stop) ends with "no known frame".
- **Panel API**: field `other.known` of `GET /api/ota/status` (was `other.centralka`). `api` stays `1`.
- **Climate trial with frame `364` removed**: the console command `climate` (and its alias `klima`),
  the `364` row of the transmit allow list with its content check, the 200 ms / 25-per-10-s fast lane
  of the transmit gate and the `364` bits are gone. Radio climate commands (`8A`) only print a
  `[climate]` line in every build.
- **Transmit list**: the default `decoder_tx_wifi` sends only `15B`; `1A9` is on the list only with
  `-DCAN_TX_EXPERIMENTAL=1`, and only with its two recorded contents (parking sensors, blind spot).
  The Stop and Start content `00 7F FF 00 FF 00 90 00` is refused by every build.
- **Vehicle settings**: `80 04`, `80 08`, `80 0E` are no longer in the table (the radio of the
  reference car never sent them); `80 0D` is no longer a `1A9` button item. A stored item mask loses
  the bits `08` and `0D` (it is filtered when read).
- **Radio screen data**: car language codes other than Polish are no longer translated for the radio
  (the language field keeps its previous value); the radio frame `C5` is no longer recognised (it is
  counted as unknown; it was always rejected by the parser for its length byte).

### Added

- Panel tour with screenshots (`docs/en/reference/panel-tour.md`, Polish copy): 111 pictures of the
  web panel from the mock device (English and Polish, desktop, phone and tablet), also used on the
  quick start, install, safety and reference pages; a one-command generator
  (`tools/make_screenshots.py` with `tools/screenshots.json`, new mock options `--demo-radio`,
  `--screenshot`, `--supply-v`, `--password-stdin`); an animated README logo
  (`docs/assets/brand/logo-animated.svg` with GIF fallbacks, built by `tools/make_logo_animation.py`).
- Two recordings of the session of 2026-11-19 in `firmware/test_data/captures/`
  (`centralka_konsola_20261119_150048.txt`: the radio frames the device received, with owner markers
  before each action; `centralka_can_20261119_150048.log`: the CAN frames of the same session). They are
  the evidence for 38 protocol constants in `radios/xp-simple-soft/provenance.md`; the host tests do not
  read them.
- `vehicles/peugeot-508-sw-2015/settings-map.md`: the 26 rows of the radio settings screen with their
  command numbers, the buttons of the radio climate screen and the rows of the sound screen; the
  climate panel bits of `350` / `2AD` checked with owner markers (`provenance.md`, `508.dbc`).
- Web panel: **Remember on this device** on the login screen (off by default). Ticked, the password
  is kept in the browser's `localStorage`, so every tab opens the panel without asking, also after a
  browser restart; unticked, it stays in `sessionStorage` of one tab as before. **Log out** forgets
  it in both places, a rejected password (`401`) is forgotten too, and a password change in the
  panel updates the saved copy ([Security](docs/en/safety/security.md#wi-fi-and-the-web-panel)).
- Car profiles chosen at build time (`-DCAR_PROFILE`): `PSA_AEE2010` (reference car, default),
  `GENERIC` (any car: listen only, no keys, no 12 V outputs, empty transmit list) and `CUSTOM`
  for your own profile; `firmware/lib/car_example` as a template compiled only in the host tests.
- Radio protocols chosen at build time (`-DRADIO_PROTOCOL`): `XP` (default), `NONE` and `CUSTOM`
  for your own protocol (`RADIO_PROTOCOL_CUSTOM_HEADER`, `RADIO_PROTOCOL_CUSTOM_OBJECT`; listen only
  on CAN: a transmitting build stops with an error); `firmware/lib/radio_example` as a template with
  an invented framing, compiled only in the host tests.
- `RadioProtocol` carries the whole radio side: framing (`encodeFrame`), parser of the radio frames
  (`rxParser`), acknowledgement byte, gap between frames, the power, key, parking, obstacle and
  version frames, decoding of the radio frames and commands, and two `const` tables (keys with a
  latch, state frames with a rate limit). The queue and state slots moved to the common session
  `firmware/lib/core/radio_session.*` (`RadioLink` is now that session with XP). Host test
  `radio_example`: a foreign protocol gets only its own framing and no XP frame; the stream to the
  radio with XP is unchanged byte for byte and in time (golden files). `tools/check_layering.py`
  rule 9: the radio path of the `Decoder` names no XP framing, parser or frame number.
- Car data for the radio screen through the profile and the protocol: neutral types
  (`SteeringAngle`, `TripData`, `OutsideTemp`, `EcoMode`, `ClimateState`, `DoorState`,
  `VehicleSettings` in `firmware/lib/core/vehicle_signals.h`), `CarProfile::decode...` functions
  (PSA: `firmware/lib/car_psa_aee2010/psa_signals.*`) and `RadioProtocol::encode...` functions
  (XP: `firmware/lib/radio_xp/xp_signals.*`). The template profile shows doors and a trip page;
  the host test `radio_stub` shows a foreign radio protocol getting the neutral data. The stream
  to the radio is unchanged byte for byte. Changing vehicle settings from the radio stays PSA code.
- Environment `listen_generic_wifi`: unknown car, listen only, with the web panel.
- Board pin files in `firmware/boards/` (`-DBOARD=PROTO_V1`), with compile-time checks of the
  pin rules of the ESP32-S3 N16R8 module, and `board_template.h` for a new board.
- `project.json`: the only source of the project name and the compatibility values (firmware
  product, marker, Wi-Fi prefix, NVS namespace); `tools/set_project_name.py`.
- Anonymised recordings of the reference car in `firmware/test_data/captures/` (VIN frames
  overwritten) and a firmware marker fixture in `firmware/test_data/fixtures/`, so the host tests
  run on any computer.
- Tools for publishing: `tools/sanitize_capture.py`, `tools/scan_private.py`,
  `tools/strip_exif.py`, `tools/check_excluded.py`, `tools/check_provenance.py`,
  `tools/check_doc_links.py`; firmware checks `check_layering.py`, `check_board_macros.py`,
  `check_comment_only.py`, `check_tx_profile_lock.sh`.
- Web panel translations: dictionaries `firmware/web/i18n/en.json` (source) and `pl.json`, language
  picker on the login screen and in the System tab (default English, choice stored in the browser
  under `ui_lang`), `firmware/web/i18n/README.md` for translators and
  `docs/en/contributing/glossary.md`.
- Panel dictionaries `de.json`, `es.json`, `ru.json`, `uk.json`: machine translations, marked
  "needs native review".
- Vehicle data of the panel moved to `firmware/web/profiles/psa_aee2010.json` (frame names, car keys,
  radio functions, door bits, ignition values) as dictionary keys.
- `firmware/tools/check_i18n.py` (missing and orphan keys, placeholders, plural forms, text literals
  outside the dictionary) and `firmware/tools/embed_web.py --report` (page size and CSP check).
- Public descriptions: `vehicles/` (reference car, template, supported list), `radios/xp-simple-soft/`
  (protocol from recordings, source of every protocol constant), `hardware/prototype-v1/`
  (KiCad, PDF, BOM, enclosure; proof of concept), `extras/sniffer/`.
- Documentation site skeleton (`mkdocs.yml`, `docs/en/`), README in English and Polish, licence
  files, `NOTICE`, `SECURITY.md`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `ROADMAP.md`, issue
  and pull request templates.
- Documentation translations, machine translated and marked "needs review" with a banner on every
  page: Polish (`docs/pl/`, every page), German, Spanish, Russian and Ukrainian (`docs/de/`,
  `docs/es/`, `docs/ru/`, `docs/uk/`: overview, safety, quick start and "Make it yours", 13 pages
  each; other pages fall back to English). The site builds all six languages with a language
  switcher and translated menu sections. READMEs in the same five languages, linked from every
  README.
- `tools/check_translations.py` (a step of `tools/release_check.sh`): every translated page has an
  English page and the banner, the same number of headings, code blocks and table rows, and code
  blocks identical to English.
- Logo and icon (author grabowski98, CC BY 4.0): files in `docs/assets/brand/` (light, dark and
  transparent logo, icon, favicon, README banners, social preview, intro animation), silkscreen
  versions in `hardware/brand/`, rules in `docs/en/contributing/brand.md`. The READMEs and the
  overview open with the SVG logo at a whole dot pitch; the documentation site has the icon, the
  favicon and the logo colours in its header and links, and follows the light or dark setting of
  the system.
- Web panel: the project icon in the header next to the name (inline SVG in the text colour) and a
  tab icon embedded as a `data:` address; the page stays within its size budget.
- Screenshots of the web panel in `docs/assets/screenshots/`, taken from the mock device with a
  recording (English, dark scheme, desktop and phone width): in the READMEs, the overview, First
  boot and the Web panel guide. On the site every screenshot is a link to the full-size file.

- Build flag `-DCAN_TX_1A9_RELEASE=1` (default `0`, only together with `-DCAN_TX_EXPERIMENTAL=1`):
  after a `1A9` button press acknowledged on the bus, one release frame `1A9 20 7F FF 00 FF 00 50 00`
  through the same gate (all interlocks apply), when the gate pause after the press ends (about
  500 ms). The content is the aftermarket box's periodic `1A9` with ignition on (every ~204 ms in
  `can_20261110_190421.log`); the box sent no separate release, a press replaced one periodic frame
  and the next periodic frame followed it. Whether a press without such a following frame causes a
  later toggle is a hypothesis. `00 7F FF 00 FF 00 10 00` (seen only with ignition `80` and once at
  box start-up) is refused with either value of the flag. A busy gate is retried for at most 1 s; a press
  without an acknowledgement gets no release; the press report is not delayed. Console lines
  `[vset] 1A9 ... release sent (acked after N ms, M ms after the press)` and
  `[vset] 1A9 ... release not sent (<reason>)`. With the flag `0` the allow list refuses this content.
  Host test `button_release` (both values), `profile_tables` (third variant).
- Overnight record in RTC memory (survives deep sleep, lost on power loss): last stable parking
  sensors state from `126`, wake-ups, "state changed while asleep", `1A9` presses and releases. New
  `stat` line `[diag] wakes=N park-flips-asleep=N 1A9 press=N release=N` and, once after a wake-up
  with a different state, `[park] sensors state changed while asleep: was ON, now OFF`. Host test
  `park_diag`.

### Changed

- Vehicle setting `80 05` (parking sensors) reads the car state from `126` byte 1 (`18` = off, `00` =
  on) instead of the `227` LED bit, for the "already in this state" and the "accepted" decisions. Both
  button items (`05`, `0C`) are refused (no state, nothing sent) during the first 3.5 s after the
  ignition comes on (the `227` lamp test lasts 3 s; for about 95 ms after ignition on `227` shows the
  value from before). What the radio is told is unchanged (still `227`).
- A waiting vehicle setting command pushed out of its slot by a command for another item now ends
  with a report and the line `[vset] radio: 80 NN VV ...: REFUSED (previous send in progress)`
  (before: dropped without a trace). A newer command for the same item still replaces it silently.
- The device network accepts 4 devices at once instead of 2 (`NET_AP_MAX_STATIONS`): in the car the radio
  could not join while two other devices were connected.
- Code review of 19.11.2026 (fixes; the radio stream and the outputs of the reference pair are
  unchanged byte for byte):
  - after a driver error at startup (CAN, I2S, UART) the 12 V outputs stay off, the amplifier
    asleep and the LED red on every loop pass (before: outputs followed only the CAN result and the
    first loop pass turned the red LED off);
  - the power frame to the radio is never lost in a full queue; a held key without fresh CAN
    frames is released after 2.5 s; the frame timeout of the radio parser is no longer hit by a
    stalled loop pass; a frame the protocol cannot frame is counted (`radio`: `too long to frame`);
  - a USB console line longer than the buffer is dropped whole with a `[con]` line instead of
    being executed cut off, and the buffer holds `settings import` with the longest export;
  - web panel: a total time limit for sending one response (slow readers are disconnected); a
    login lockout cannot come back after the millisecond counter wraps; the last wrong password is
    wiped after a correct login and after 60 s; an upload or rollback that changes the operating
    mode or `canTx` only from the device's own network (`403 ap_only`); the switch to a new image
    is skipped when reverse or sensors appeared during the image check;
  - `RadioProtocol` has `stateKeyBytes` (state frames with a page byte) and
    `commandsUseVsetNumbers` (only XP: other protocols' commands are not interpreted);
    `CarProfile::keyRemapProtocol` (the PSA default remaps are used only with XP);
  - `tools/check_layering.py` also reads `#include_next`, `#import`, digraphs, line continuations
    and digit separators, refuses to include non-header files, checks the XP namespace in any letter
    case and every Decoder file;
  - the panel mock (`firmware/tools/mock_device.py`) follows the device's login lockout and `wifi`
    command rules.

- **Project named ESPCanBox.** The identity values changed (firmware product `espcanbox`, marker
  `ESPC-FWID`, Wi-Fi `ESPCanBox-XXXX`, mDNS `espcanbox.local`, NVS namespace `espcanbox`), so a
  device running the prototype firmware must be flashed over USB once (the Wi-Fi update rejects
  the other image in both directions) and starts with empty settings. Panel API: field
  `other.known` of `GET /api/ota/status` (was `other.centralka`); downloaded logs are named `espcanbox_can*.log`.
- **Protocol constants**: every constant has an own measurement (`tools/check_provenance.py`: 109
  constants, 108 measured, 1 design, none "needs owner decision"). 38 constants were measured on
  2026-11-19; 33 without a measurement were removed (see "Breaking changes").
- **Transmitting:** the transmit gate reads its allow list from the car profile. The default
  `decoder_tx_wifi` allows only frame `15B` (checked by ID and length only). Frame `1A9` is compiled
  only with `-DCAN_TX_EXPERIMENTAL=1` (default `0`).
  A transmit environment does not compile with the `GENERIC`, example or `CUSTOM` profile.
- Code, comments, file names and environment names in English: `listen`, `decoder`,
  `listen_wifi`, `decoder_wifi`, `decoder_tx_wifi` (the old names are kept as comments in
  `firmware/platformio.ini`).
- USB console and panel API codes in English. Console tags renamed: `[ust]` -> `[vset]`,
  `[klima]` -> `[climate]`, `[dzwiek]` -> `[sound]`; the other tags are unchanged. The old Polish
  commands still work as aliases. Scripts that read the old lines must be updated.
- Libraries split into `lib/core`, `lib/car_psa_aee2010`, `lib/car_generic`, `lib/radio_xp`,
  `lib/radio_none` and `lib/app`.
- With the radio protocol `NONE` the car data for the radio screen are not built at all (before:
  built and dropped), so the counter of dropped radio frames is smaller and the panel field
  `car.settings.state38` stays all zeros. No shipped environment combines `NONE` with the
  reference profile.
- With the radio protocol `NONE` the power, key, parking and obstacle frames are not built either
  (before: built as XP frames and dropped), so the counter of dropped radio frames stays `0`
  (host test `radio_none`, column `suppressed`).
- Panel page size with all six languages (en, pl, de, es, ru, uk): 301 214 B raw,
  **81 855 B gzip**, only 65 B under the budget of 81 920 B (before the translations about 31 KB).
  Measured with `python -I tools/embed_web.py --report`.
- Comment lines of a recording saved by the panel are in English, the same as `/api/trace.log`
  (`# masked`, `# lost N frames`, `# device restart`).
- Licences are final: Apache-2.0 for the code, CC BY 4.0 for the documentation, CERN-OHL-S-2.0 for
  the hardware files (which file falls under which: `NOTICE`, README section "Licences"). Release
  archives carry the licence texts, the third-party notices and licence texts of the parts linked
  into the images (`LICENSES/THIRD-PARTY.md`, `LICENSES/third-party/`) and the source archive of
  the LGPL-2.1 library, the Arduino core for ESP32 3.3.12 (`source-arduino-esp32-3.3.12.tar.gz`).

### Fixed

- After a CAN driver start error the device samples the CAN RX pin on every main loop pass instead of
  relying on received frames (a LOW sample = a dominant bit = bus activity): it stays awake in the
  safe state while the car bus is active and deep-sleeps after the usual 30 s of silence, with the
  usual wake-up on that pin. Before, it either repeated the start in a sleep/wake loop while the bus
  was active, or (earlier today) never slept and drained the battery while parked
  (`sleepBusActivity`, host test `loop_policy`).
- The confirmation write of an update (otadata) is no longer done while the CAN transmitter window is
  open, the same as the rest of the network handling.
- The radio screen latch key (moon) is cleared when ACC comes on: a press while ACC was off no longer
  makes the first press after the ignition do nothing. It is also cleared when the radio reports it
  has started (`81 01`): a rebooted radio comes back with the screen on (host test `custom_keys_doors`).
- The USB console reads at most 256 bytes and 2 complete lines per main loop pass (like the radio
  UART budget); a pasted or streamed capture no longer holds the loop (CAN receive, radio UART,
  watchdog) until the whole burst is read.
- Blind spot (`80 0C`): no state while reverse is engaged and for 500 ms after it is released. Bit
  `227` 3.0 clears about 40 ms after reverse is engaged and returns about 40 ms after it is released
  (`can_20261111_151209.log`, all 20 reverse cycles), so in reverse the radio command could toggle the
  setting the wrong way or be taken as ACCEPTED. Now refused with NO_STATE, nothing sent; the data for
  the radio (frame `38`) are unchanged (host test `settings_change`).
- The `[diag]` 1A9 press and release counters count only frames handed to the CAN controller, not a
  send the gate ended as failed before the controller took the frame (`CanTxGate::handedOver`, host
  test `button_release`).
- The installation guide no longer expects `bad checksum=0` from the radio link: some head units
  send a malformed `C5` frame that counts as one bad checksum and three garbage bytes.

### Known limitations

- Frame `15B` has no content check yet.
- Settings sent with `15B` were accepted by the body control unit (seen in frame `260`) for six
  items on the reference car; the visible effect in the car was not checked.
- Climate control from the radio with frame `364` does not work, so the firmware has no climate
  transmit path; the radio shows the climate state only.
- Deep sleep and wake-up from the bus were tried in one car only (2026-11-20).
- The transmit rate limit of 10 frames per minute can refuse quick repeated changes from the
  radio's settings screen.
- The `refused` transmit counter also counts internal retries.
