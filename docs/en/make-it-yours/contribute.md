# Share your car

You measured a car that is not in `vehicles/SUPPORTED.md`. Your notes help the next person with
the same car, even without a firmware profile. This page says what to share, how to clean it
first, and where each file goes. The general rules for contributions are in `CONTRIBUTING.md` at
the root of the repository.

This is a hobby project with one maintainer. There is no promise of an answer or a review within
any time.

## What you can share

| Contribution | Where it goes |
|---|---|
| A description of the car and its bus | `vehicles/<make>-<model>-<year>/README.md`, from `vehicles/_template/README.md` |
| Frames and signals | `vehicles/<make>-<model>-<year>/<car>.dbc`, each comment starting with its status |
| Connector views and pin tables | `vehicles/<make>-<model>-<year>/wiring.md`, with the side you looked from |
| Recordings used by a test | `firmware/test_data/captures/`, with one row in its `README.md` |
| A firmware profile | `firmware/lib/car_<name>/` ([Write a car profile](write-a-profile.md)) |
| Its host test | `firmware/test_host/<name>_car.cpp`, the reference in `firmware/test_host/expected/`, one block in `firmware/tools/run_host_tests.sh` ([Test your profile](test-your-profile.md)) |
| What your radio does with each key code | your vehicle page, section "Radio side" ([Map the radio](radio-mapping.md)) |
| A row for the supported list | `vehicles/SUPPORTED.md`: vehicle, profile, status, what works, link to your page |

A pull request with only the vehicle page is welcome. So is an issue with the "New vehicle"
template, if you do not use Git.

## The rules for facts

- Every fact has a status: **measured** (how: multimeter, recording, console of the device; for a
  recording, the file and the step) or **unverified** (the source: a public database, a forum, a
  seller's drawing).
- A conclusion is not a measurement. "The voltages look like low-speed fault-tolerant CAN" is a
  conclusion; say what would confirm it.
- Write only what you saw. "Not measured" is a good entry.
- Only your own work: your recordings, photos and words. Do not send decompiled code, firmware or
  apps of box or radio makers, tables taken from them, datasheets, or code copied from projects
  under an incompatible licence. Link a public source instead of copying it.

## Clean your recordings first

A recording of the bus can contain the VIN of your car, and it shows when and how you drove. Clean
every file before it leaves your computer.

### 1. Find the frames that carry the VIN

On the reference car the VIN is in frames `336`, `3B6` and `2B6` (measured: recordings of the
reference car). Those three IDs are the defaults of the cleaning tool and of `record.py`. **On
another make the VIN may be in other frames, or not on this bus at all.** Find out before you
share:

1. Take your VIN from the registration document. Do not type it into any file you will share.
2. The VIN is 17 letters and digits. Turn the first three characters into hex ASCII codes: for
   example `V` = `56`, `F` = `46`, `3` = `33`.
3. Search your recording for those bytes in a text editor, and then for the hex codes of the
   other characters. Any frame whose data spells a part of the VIN is a VIN frame. Stored as text,
   17 characters do not fit into one 8-byte frame, so expect more than one. If nothing matches,
   the car may not send the VIN as text on this bus; say so on your vehicle page.
4. Write the IDs down. If you find none, write "VIN frames: none found" on your vehicle page.

### 2. Overwrite them

From the repository root:

```bash
python -I tools/sanitize_capture.py my_drive.log my_drive_clean.log --ids 336,3B6,2B6,ABC
python -I tools/sanitize_capture.py --check my_drive_clean.log --ids 336,3B6,2B6,ABC
```

Replace `ABC` with your VIN frame IDs (keep the three defaults; they do no harm). The tool
overwrites the data of those frames with zeros of the same length: line count, times, IDs and
lengths stay as they were, so tests still work. It also replaces a header line that names your
serial port with `# capture`. `--check` must report `0 problem(s)`.

### 3. Read the comments yourself

The tool copies comment lines (`# ...`) as they are. Your marker texts are in them. Open the file
and look for names, places, addresses, network names or anything else private.

### 4. Scan everything you add

```bash
python -I tools/scan_private.py .
```

It looks for VINs, VIN frames with data, e-mail addresses, Windows user paths, MAC addresses and
device network names with a real suffix. It must end with no errors.

> [!WARNING]
> **The scanner knows only the reference car's VIN format**
>
> `scan_private.py` recognises a VIN of the PSA group (`VF3` + 14 characters) and the PSA VIN
> frames. It does not know the VIN of another make or your own VIN frames. Steps 1 to 3 are
> still your job.

## Before you open a pull request

The pull request template has a checklist. For a new car, these are the parts that matter:

| Check | Command | Expected |
|---|---|---|
| Recordings cleaned | `python -I tools/sanitize_capture.py --check <files> --ids ...` | `0 problem(s)` |
| No private data | `python -I tools/scan_private.py .` | no errors |
| Host tests (from `firmware/`) | `bash tools/run_host_tests.sh` | `All tests match the references.` |
| Layering (from `firmware/`) | `python -I tools/check_layering.py` | `Layering OK.` |
| Your build (from `firmware/`) | `uvx platformio run -e listen_mycar` | `SUCCESS` |
| Documentation links, if you changed `docs/` | `python -I tools/check_doc_links.py` | no dead links |

Your `platformio_override.ini` stays on your computer: it is not tracked. Say in the pull request
which build environment you used for your car, and whether you tried the firmware on a board and
in the car.

Do not change existing test references (`firmware/test_host/expected/`, `golden/`,
`golden_060/`) in a pull request that adds a car. Adding your own reference file is fine.

## Transmission is not part of a car contribution

A profile built with `-DCAR_PROFILE=CUSTOM` cannot transmit, and the firmware has no way to add a
transmit frame at run time. A frame that the device may send needs its own measurement, a content
check written in C++, a host test, and a named profile in `firmware/src/config.h`. That is a
separate discussion, not part of adding a car: read
[Transmitting on the CAN bus](../safety/transmit.md) and open an issue first.

## Licences

By sending a contribution you agree that it is published under the licences of this repository:
code Apache-2.0, documentation CC BY 4.0, hardware CERN-OHL-S-2.0. These licences are provisional
until the first public release.

This project is not affiliated with any car maker, radio maker or CAN box maker. Name them only to
say what you tested.
