# Screenshots

The pictures of the web panel in `docs/assets/screenshots/` are made by one script from the mock
device. Nobody takes them by hand, and none of them comes from a car. When you change the panel,
make them again and commit the changed files together with the change.

## Make all pictures

In the repository root:

```sh
uvx --with playwright==1.65.2 --with pillow==13.0.0 python -I tools/make_screenshots.py
```

What you need:

- [uv](https://docs.astral.sh/uv/). `uvx` fetches Playwright and Pillow into its own cache; nothing
  is added to the firmware build or to the project.
- A Chromium-family browser. On Windows the script uses Microsoft Edge, which every Windows 10 and
  11 has. On Linux add `--channel chrome` for Google Chrome, or let Playwright download its own
  Chromium once with `uvx --with playwright==1.65.2 playwright install chromium` and add
  `--channel chromium`.

The script starts `firmware/tools/mock_device.py` on a free port of `127.0.0.1` for every scene,
with a random password that it hands over on the standard input and never prints. It talks to no
device, no serial port and no network other than `127.0.0.1`. A full run takes about eight minutes.

The mock runs with `--screenshot`: the same build date and hash in every picture (`Jan 10 2026
09:00:00`; the "new" firmware files of the update wizard are built `Feb  3 2026`), the network name
`ESPCanBox-A1B2` and console lines without the "(mock)" marks. `--supply-v` gives the 12 V row a
value that fits the car state of the scene.

## Make some of them

| Option | What it does |
|---|---|
| `--list` | prints every scene with its languages, viewports and file name |
| `--only ID[,ID...]` | only these scenes; a trailing `*` matches a prefix, for example `--only "ota-*"` |
| `--lang en,pl` | only these languages |
| `--viewport phone` | only these viewports: `desktop` (1280 × 800), `tablet` (820 × 1180), `phone` (390 × 844) |
| `--keep-going` | goes on after a failed scene; the exit code is still 1 |
| `--firmware DIR` | another firmware directory with `tools/mock_device.py` |

## Add a scene

Scenes are listed in `tools/screenshots.json`; the comment at the top of the file explains every
field. A scene says which mock to start (build mode, CAN transmission, home network client, a car
state or a recording), which steps to take in the panel (open a tab, click, type, upload a file,
wait for an element) and what to crop (the visible window, the full page, one card or the box
around several cards). `expect_text` and `absent_text` say what must and must not be on the page,
so a scene that no longer shows its state ends with a warning instead of a wrong picture.

Rules:

- A scene shows only a state the panel really has with a real device. Car states are written as
  the frames the device decodes (ignition, lights, reverse, doors, parking sensors), not as changed
  page text.
- The id says what the picture shows, for example `dashboard-park-continuous`.
- English for every scene. Polish and phone versions only for the pictures that the README and the
  quick start use.

## What the script checks

After each scene the script looks at the visible text of the page and prints a warning (the run then
ends with exit code 1) when it finds a dictionary key instead of a text (a missing translation), an unfilled `{placeholder}`, the
mock password, or something that looks like a local path, a MAC address, a VIN or an e-mail
address; text in fields and in `title`, `aria-label`, `placeholder` and `alt` attributes is checked
too. Each PNG is rendered at device pixel ratio 2, scaled down, reduced to 256 colours and written
without metadata. The colours written in the panel sources (`app.css`, `index.html`, `app.js`) are
kept exactly in the palette, so a red zone stays red. `docs/assets/screenshots/manifest.json` lists every file with its scene,
language, viewport, size and SHA-256.

Numbers that change by themselves (uptime, frame counters, ages) differ from run to run, so a new
run changes many files. Commit them only when the panel changed.

Look at every changed picture before you commit it. `python -I tools/strip_exif.py --check .` and
`python -I tools/scan_private.py .` run in `tools/release_check.sh` as well.
