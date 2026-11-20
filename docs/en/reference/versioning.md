# Versioning

Other programs and people depend on a few fixed formats of this project: tools that read the USB
console, the web panel, the device mock, the firmware update over Wi-Fi. This page lists those
formats (the **contracts**), the version numbers that protect them, and what counts as a breaking
change.

## Version numbers

| Number | Where | Now | Meaning |
|---|---|---|---|
| firmware version | `FW_VERSION` in `firmware/src/version.h` | `2.0.0-beta.1` | [Semantic Versioning](https://semver.org/spec/v2.0.0.html) `MAJOR.MINOR.PATCH`, with an optional pre-release part (`-beta.N`); at most 15 characters, only digits, letters, `.` and `-` |
| panel API version | `FW_API_VERSION` in `firmware/src/version.h`, field `api` of `GET /api/hello` | `1` | a single integer; raised only on a change that breaks clients |
| build string | `__DATE__ " " __TIME__` of `firmware/src/version.cpp` | e.g. `Nov 18 2026 01:33:28` | tells builds apart; CAN transmit arming is tied to it |
| image hash | field `elf` of `/api/status` and `/api/ota/status` | 16 hex digits | the first 8 bytes of the SHA-256 of the ELF file; different for every built image |

The version number lives in `version.h` and not in `platformio.ini`: a change of `platformio.ini`
rebuilds everything, a change of `version.h` only `src/`.

`firmware/tools/build_time.py` recompiles `version.cpp` whenever something in `src/`, `lib/` or
`web/` changed, so the build string is the time of the last build with changed sources. Use the
image hash, not the build string, to tell two images of the same version apart.

> [!NOTE]
> **The first release is a beta**
>
> The firmware versions up to `1.0.0` are the author's own history on the reference car (the
> prototype, before the project had its name). The first public release is `2.0.0-beta.1`: a new
> MAJOR, because this tree changed contracts of the prototype (see `CHANGELOG.md`, format:
> [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)). A final `2.0.0` follows only after the
> full check on the bench and in the car.

The pre-release part is shown everywhere the version is: the console line
`[ver] <product> 2.0.0-beta.1 listen, built ...`, the version badge and the System tab of the panel,
the startup comment `# version: ...`, and the names of the release files
(`<environment>-2.0.0-beta.1.bin`, made by `.github/workflows/release.yml`, which also requires the
tag `v2.0.0-beta.1`). Nothing in the firmware, the panel or the mock compares two versions, so the
pre-release part needs no special handling.

## The contracts

### 1. USB console line format

Read by the panel (Console tab), by `firmware/tools/record.py` (it saves the console lines through
`/api/log`) and by anyone who logs the USB port.

- raw CAN frame: `(seconds.milliseconds) can0 ID#DATA`, seconds without leading zeros,
  milliseconds always 3 digits, ID 3 hex digits, data 0..16 hex digits;
- comment: a line that starts with `#`;
- tagged line: `[tag] ...`, for example `[stat]`, `[park]`, `[vset]`, `[cantx]`.

The console module (`firmware/src/console.h`) adds nothing to the text and changes nothing. No
`ESP_LOGx` macros are mixed into it: error lines of the ESP-IDF libraries are caught and printed as
`# idf: ...` comments.

The same `(s.mmm) can0 ID#DATA` format is used by the recordings in `firmware/test_data/captures/`,
by `/api/trace.log` and by the panel recording. Comment lines in those files: `# masked`,
`# lost N frames`, `# device restart`.

The texts of the tagged lines are pinned by tests: the stream references in
`firmware/test_host/golden/` contain the console lines the logic prints while it replays the
recordings (`*.console.txt`). A changed line shows up as a test failure; see
[Golden files](../testing/golden-files.md).

Command names are part of the contract too. Old (Polish) names stay as aliases:
`ust`, `wifi haslo`, `wifi nowehaslo` (see [USB console](console.md#old-command-names)). The command
`climate` and its alias `klima` were removed in 2.0.0 together with the climate trial.

### 2. Panel API

Described on [Panel API](panel-api.md). Rules:

- adding a field to an answer, a new error word or a new request is **compatible**: the API
  version stays. A client must ignore fields it does not know and show unknown words as they are;
- removing a field, renaming it, or giving it another meaning **breaks** clients: raise
  `FW_API_VERSION`;
- a field added later is marked on the API page with the version that brought it, and a client
  must treat a missing field as described there (usually as `false` or "no data").

`/api/hello` returns `api`. The panel only shows it on the login screen and checks the
product name; neither the panel nor the mock compares `api` with a known version yet.

Fields added so far, so a client can talk to older firmware:

| Firmware | Added |
|---|---|
| 0.7.0 | byte `canTx` in the firmware marker; `sys.canTx`; `mode_change` also for a `canTx` difference |
| 0.8.0 | `car.settings`; the panel checks `canTx` of a file before it sends it; `radio.txGated`, `radio.slotsFull` |
| 0.9.0 | `can.tx` (transmit gate) |
| 1.0.0 | `sys.clientAp`, `can.tx.armNext`, `car.settings.mask`, `car.settings.last.frame`; the environment `decoder_tx_wifi`; `cantx` and `vset on/off` checks in `/api/cmd` |
| 2.0.0-beta.1 | renamed: `other.centralka` of `GET /api/ota/status` is now `other.known` (a prototype client does not connect anyway: the product name changed). `api` stays `1`, the first API of this product |

### 3. Firmware marker (FwIdent)

A 72-byte structure in every image (`firmware/lib/core/fw_ident.h`, layout on
[Panel API](panel-api.md#the-device-marker-in-the-file)). The Wi-Fi update looks for it in an
uploaded file and refuses a file without it or with another product.

- The start string is byte `7F`, the 9 characters of `fwid_magic`, byte `7F`, byte `00`.
- The product is the `fw_product` value (at most 15 characters).
- Exactly one copy of the start string may exist in an image.
- New fields may only use the reserved byte or grow the structure at the end; the offsets of the
  existing fields stay.

### 4. Project identity

`project.json` is the only place for the project name and the values derived from it. Some of
them are **compatibility values**:

| Value | What depends on it |
|---|---|
| `fw_product`, `fwid_magic` | the Wi-Fi update: a device refuses an image with another product or magic, so after a change the first update must go over USB |
| `nvs_namespace` | the stored settings: another namespace starts with empty settings (new password, no home network, no remappings) |
| `web_session_key` | only a logged-in browser tab (the user logs in again) |
| `ap_ssid_prefix`, `mdns_host` | the name of the device network and the `.local` address |

Change them only with `tools/set_project_name.py` (it also rewrites
`firmware/include/project_identity.h` and the identity line of `firmware/web/app.js`). Its
`--dry-run` lists every other place where the old values still appear, documentation included.
The logo and the other graphics show the name as a drawing: the tool does not change them
([Logo and icon](../contributing/brand.md)).

### 5. Stored settings

The NVS keys listed on [Config and pins](config-and-pins.md#settings-stored-on-the-device). A new
version must read the keys of the older one or ignore them safely. An invalid stored value falls
back to the default (for example, an invalid key table gives the built-in remappings and the line
`# stored key swap table is invalid: the fixed entries apply`).

CAN transmit arming is stored as the build string of the armed program. It is valid only for that
exact build: every newly flashed program starts disarmed. This is on purpose and must not change.

### 6. Build environments

The environment names (`listen`, `decoder`, `listen_wifi`, `decoder_wifi`, `decoder_tx_wifi`,
`listen_generic_wifi`) appear in documentation, scripts and the file path
`.pio/build/<environment>/firmware.bin`. Rename one only in a major version, and keep the old name
in a comment of `platformio.ini`. The default environment must stay a listen-only one.

## What goes into which part of the version

| Change | Part |
|---|---|
| a contract above breaks (a console line that tools read changes form, an API field is removed, the marker layout changes, a stored key is read differently) | MAJOR (and for the API also `FW_API_VERSION`) |
| a new feature, command, API field or car profile that keeps the contracts | MINOR |
| a fix that keeps behaviour and formats | PATCH |

A change of what the device **sends** (to the radio, on the 12 V outputs or on CAN) is never a
patch: it needs a note in `CHANGELOG.md` and, for CAN, an entry on
[Transmitting on the CAN bus](../safety/transmit.md).
