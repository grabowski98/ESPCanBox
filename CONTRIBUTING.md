# Contributing

Thank you for wanting to help. This is a hobby project with one maintainer: questions, recordings
and pull requests are welcome, but there is no promise of an answer, a review or a release within
any time. Please be patient and keep things small.

By taking part you agree to the [Code of Conduct](CODE_OF_CONDUCT.md).

## Ground rules

- **No guessing about cars.** Every fact about a car, a radio or a CAN box has a status:
  **measured** (how, and which recording) or **unverified** (with the source). Rules:
  `docs/STYLE.md`.
- **Listen first.** A new car starts with the `generic` profile, which only listens. Transmit
  support for a new frame needs a measurement, a content check in code and a test; nothing may
  extend the transmit list at run time. See [docs/en/safety/transmit.md](docs/en/safety/transmit.md).
- **Only your own work.** Constants and frame descriptions must come from your own recordings or
  from a source whose licence allows it (credited in `NOTICE`). Do not send decompiled code,
  firmware or apps of box or radio makers, tables taken from them, datasheets, or code copied from
  projects under an incompatible licence (for example GPL code into this Apache-2.0 code).
- **Nothing private.** No VIN, serial numbers, MAC addresses, e-mail addresses, passwords, Wi-Fi
  names, invoices or raw logs of your radio.
- By sending a contribution you agree that it is published under the licences of this
  repository: code Apache-2.0, documentation CC BY 4.0, hardware CERN-OHL-S-2.0.

## Before you share a recording

Recordings of the bus contain the VIN (on PSA AEE2010 cars in frames `336`, `3B6` and `2B6`) and
show when and how you drove. Clean every file first. Type this in the top directory of the
repository (not in `firmware/`):

```
python -I tools/sanitize_capture.py my_drive.log my_drive_clean.log
python -I tools/sanitize_capture.py --check my_drive_clean.log
```

Expected: the first command writes `my_drive_clean.log`, the second prints `0 problem(s)`. If
your car carries the VIN in other frames, add them: `--ids 336,3B6,2B6,ABC`. Look at the file
yourself as well: the comment lines (`# ...`) are copied as they are.

## Add a car

1. Read [Make it yours](docs/en/make-it-yours/find-your-bus.md). Build `listen_generic_wifi`
   and record your bus, one action at a time, with a `#` marker before each action.
2. Decode the frames you need and write them down with their status.
3. Copy `vehicles/_template/README.md` to `vehicles/<make>-<model>-<year>/` and fill it in.
4. If you write a firmware profile, start from `firmware/lib/car_example` and add a test on your
   (cleaned) recording, see [Test your profile](docs/en/make-it-yours/test-your-profile.md).
5. Open an issue with the **New vehicle** template, or a pull request.

A description of a car without a firmware profile is a useful contribution on its own.

## Translate

- Web panel: `firmware/web/i18n/<lang>.json`, rules in `firmware/web/i18n/README.md`.
- Documentation: `docs/<lang>/`, same file names as `docs/en/`; rules in `docs/STYLE.md` and
  [docs/en/contributing/translations.md](docs/en/contributing/translations.md).
- Use the words of [the glossary](docs/en/contributing/glossary.md). If you change a term, change
  the glossary and the dictionaries in the same pull request.
- Polish, German, Spanish, Russian and Ukrainian are machine translations marked "needs review". A native
  speaker who checks them helps a lot; use the **Translation** issue template.

**A new panel text needs only English.** When you add a key, add it to `en.json` only. The other
languages fall back to English until a translator adds the key; `check_i18n.py` reports this as
a warning, not an error. Do not machine-translate new keys into other files in the same pull
request.

## Fix a bug or improve the code

- Code rules: logic without hardware goes into `firmware/lib/` (no `Arduino.h`) and gets a host
  test; hardware code stays in `firmware/src/`; pins only in `firmware/boards/`; no `String`,
  `malloc` or `new` in the main loop; anything that can send something has an explicit mode,
  off by default.
- Do not change the USB console line format (`can0 ID#DATA`, `#` comments, `[tag]` prefixes),
  console commands or API codes without a note in `CHANGELOG.md`: tools read them
  ([Versioning](docs/en/reference/versioning.md)).
- Test patterns (`firmware/test_host/expected`, `golden`, `golden_060`) are recorded again only
  when the change is meant to change the output, and the pull request explains every difference.

## Checks to run

From `firmware/` (Git Bash on Windows):

```
bash tools/run_host_tests.sh
uvx platformio run
bash tools/check_no_can_tx.sh
python -I tools/check_layering.py
python -I tools/check_i18n.py
```

From the repository root:

```
python -I tools/scan_private.py .
python -I tools/check_doc_links.py
python -I tools/check_translations.py
uvx --from mkdocs-material==9.8.1 --with mkdocs-static-i18n==1.3.1 mkdocs build --strict
```

You need uv and, on Windows, Git Bash: see `docs/en/quick-start/flash.md`. The first
`uvx platformio run` downloads the ESP32 platform and its compiler, a large download that takes a
while; the first host test run downloads a compiler too.

Expected: the host tests end with `All tests match the references.`, PlatformIO with `SUCCESS`,
and every check ends without errors (exit code 0).

`uvx platformio run` without `-e` builds only the default environment; build the environments
your change touches with `-e <name>`. Changes in `firmware/src/` have no host test: say in the
pull request whether you tried them on a board.

### Screenshots of the web panel and the animated logo

Changed the panel? Make the pictures again with one command and commit the changed PNG files:
`uvx --with playwright==1.65.2 --with pillow==13.0.0 python -I tools/make_screenshots.py`
(Edge or Chrome, no device needed). Details: [Screenshots](docs/en/contributing/screenshots.md).

Redrew the logo? Build the animated README logo again with
`uvx --with pillow==13.0.0 python -I tools/make_logo_animation.py --gif`;
`python -I tools/make_logo_animation.py --check` tells whether it still matches
([Brand](docs/en/contributing/brand.md)).

## Scope of support

- Supported: the reference car and radio listed in `vehicles/SUPPORTED.md`, the prototype board,
  and the build environments in `firmware/platformio.ini`.
- Not supported: installing it in your car for you, other radios or boxes without your own
  recordings, ready-made firmware images that transmit (none are published).
- The maintainer may close issues that cannot be reproduced or that ask for guesses about a car.
