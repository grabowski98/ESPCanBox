# Test your profile

Before your profile goes near the car, run it on your PC. A **host test** feeds a recording, frame
by frame, through the same decoder code that runs on the device, and prints what the device
would do: console lines, frames for the radio, and every change of the 12 V outputs and the
beeper. You read that output once, check it against what you did in the car, and store it as the
reference. From then on the test tells you when a change breaks something.

Nothing is flashed and no port is opened. You do not need the board or the car.

This page continues the tutorial of [Write a car profile](write-a-profile.md) with the invented
profile `mine`. Every command was run on a copy of the repository. All commands run from the
`firmware` folder, in Git Bash.

> [!TIP]
> **Dry run first**
>
> Before you put your own frames into `car_mine`, you can check the copy and the renames. Do
> steps 2 to 4 with the template values still in `car_mine`, and with
> `run_file mine_car test_host/synthetic/example_car.log` in step 3. The output must end with
> `example profile works on every axis` and `### code=0`. Then change the profile to your car
> and put your own recording into the `run_file` line.

## What you need

- Git Bash on Windows, or a shell on Linux or macOS;
- `uv`: the test script gets a C++ compiler through it (`ziglang` package; the first run downloads
  it, so it needs internet once), or set your own with
  `CXX=g++`. Details: [Host tests](../testing/host-tests.md);
- your profile in `firmware/lib/car_<name>/`;
- a recording of your car with markers ([Record the bus](record.md)), cleaned with
  `python -I ../tools/sanitize_capture.py` if you will share it ([Share your car](contribute.md)).

## Step 1: put the recording in place

| Recording | Where |
|---|---|
| a real recording of your car | `firmware/test_data/captures/`, with one row in its `README.md` (what, how recorded, which test uses it) |
| frames you wrote by hand to try a case | `firmware/test_host/synthetic/`, with a comment at the top saying the frames are invented |

The format is the usual one, `(seconds) can0 ID#DATA`. Lines that are not CAN frames (comments,
UART lines) are skipped by the test. Hand-written files for profile tests go to
`test_host/synthetic/`, not to `test_data/synthetic/`: that one is for `can_diff.py`.

A hand-written file is a good start, because you know exactly what should happen. The one used in
the tutorial, `test_host/synthetic/mine_car.log`, began like this (the tutorial car of
[Write a profile](write-a-profile.md); this file is not in the repository, so write your own):

```text
# Invented frames for the "mine" profile (tutorial). Not a real car.
# 2C0 = ignition / reverse, 1F0 = buttons + knob, 350 = rear parking, 2C8 = lights
(0.000000) can0 2C0#00000000
(0.200000) can0 2C0#00010000
(0.400000) can0 2C0#00020000
(0.600000) can0 1F0#01000000
(0.700000) can0 1F0#01000000
```

## Step 2: copy the example test

The template profile has its own test, `firmware/test_host/example_car.cpp`. Copy it, from the
`firmware` folder:

```bash
cp test_host/example_car.cpp test_host/mine_car.cpp
```

Then change four things in `mine_car.cpp`:

1. `#include "profile_example.h"` becomes `#include "profile_mine.h"`;
2. every `kCarExample` becomes `kCarMine` (six times, in four lines; a search and replace is easiest);
3. the usage text `example_car <capture.log>` becomes `mine_car <capture.log>` (in the comment at the
   top and in the `fprintf` line);
4. the last check, the line that starts with `const bool ok =`. It requires that **every** output
   did something. Each part: `r.accMs` = ACC, `r.illMs` = lights, `r.revMs` = reverse,
   `r.toneMs` = beeper, `r.uartFrames` = frames for the radio, `d.haveLastKey()` = at least one
   button. Remove the parts your profile does not have. A profile without parking sensors fails
   on `r.toneMs > 0`, and the test then ends with exit code 1. Ready lines:
    - ignition and buttons only:
      `const bool ok = r.accMs > 0 && r.uartFrames > 0 && d.haveLastKey();`
    - ignition only: `const bool ok = r.accMs > 0;`

    The text `example profile works on every axis` may stay: it is only a label.

## Step 3: add the test to the test script

Open `firmware/tools/run_host_tests.sh` and find the block that ends with `finish example_car`.
Add your test right after it:

```bash
# My car profile
# shellcheck disable=SC2086
build mine_car $S38_LIBS profile_mine.cpp
rm -f "$OUT/mine_car.actual.txt"
run_file mine_car test_host/synthetic/mine_car.log
finish mine_car
```

`build` compiles your test together with the decoder and its libraries and your profile file.
`$S38_LIBS` is the list of decoder files the test needs; keep it as it is. `run_file` runs it on the file you give, with a path relative to `firmware/`. For a
recording in `test_data/captures/`, give that path.

## Step 4: first run, read the output, store the reference

Run all host tests (`cd firmware` is needed only if you are in the repository folder):

```bash
cd firmware
bash tools/run_host_tests.sh
```

Expected: all existing tests print `OK`. Yours stops the script, because it has no reference yet:

```text
OK     example_car
FAIL   mine_car: no reference test_host/expected/mine_car.txt
```

The output is now in `firmware/.build_host/mine_car.actual.txt`. **Read it** and compare it with
what you did in the car (or wrote in the file). This is the real test; the stored file only
repeats it later. An excerpt from the tutorial run:

```text
### mine_car test_host/synthetic/mine_car.log
profile mine: 500000 bit/s, 4 key rows, 1 knob(s), txAllow 0
     0 outputs acc=0 ill=0 rev=0 tone=0 continuous=0
console [car] acc=1
   200 radio 2E EA 02 01 0A 08
   200 outputs acc=1 ill=0 rev=0 tone=0 continuous=0
console [key] code=14 state=1
   600 radio 2E 20 02 14 01 C8
console [key] code=14 state=2
   800 radio 2E 20 02 14 02 C7
...
console [park] rear=4 front=0 sound=1 rear interval=0x00 zones rear 707 front 777
  1600 radio 2E 32 07 02 05 00 05 05 05 05 AB
  1600 outputs acc=1 ill=1 rev=1 tone=1 continuous=1
...
frames=21 uart_bytes=106 uart_frames=13 acc_ms=3101 ill_ms=1001 rev_ms=800 tone_ms=390 keys_dropped=0
example profile works on every axis
### code=0
```

How to read it:

| Line | Meaning |
|---|---|
| `profile mine: ...` | what your profile contains: speed, number of button rows and knobs, transmit list (always 0 for your own profile) |
| `console ...` | the line the device would print on its USB console |
| `200 radio 2E EA ...` | a frame for the radio at 200 ms (time from the recording clock). `EA` = power on, `20` = key, `32` = parking sensors, `3E` = obstacle flag. See [XP protocol](../reference/xp-protocol.md) |
| `outputs acc=1 ill=0 rev=0 tone=0 continuous=0` | a change of the 12 V outputs (ACC, illumination, reverse) or of the beeper |
| `[key] code=14 state=1` | button with radio code `14` (volume +): 1 pressed, 2 held, 0 released |
| last lines | totals in milliseconds, and your `ok` check |

Things worth checking in your own output:

- ACC comes on with your ignition-on frame and goes off 2.5 s after ignition off. In the excerpt it
  never goes off, because the replay ends 1 s after the last frame.
- The illumination output goes off 1 s after the last lights frame (in the tutorial run: lights
  frame at 1100 ms, `ill=0` at 2101 ms). If your car repeats its lights frame, this does not
  happen.
- Every button gives `state=1`, then `state=2` every 200 ms while held, then `state=0`.
- The tone starts with the first zone below 7 and becomes continuous only with interval `0x00`.
- A button shows a different code than you expect: see [Map the radio](radio-mapping.md). The
  default remappings come from the car profile; the example profile has none, so the example test
  shows SRC as `code=21 state=1`, sent unchanged.

> [!WARNING]
> **A stored `### code=1` passes as OK**
>
> The script compares the output with the stored file only. It does not stop on a stored
> `### code=1`. Check the last lines (point 1 below) before you copy, or the test will pass
> although it failed.

When the output is right, store it as the reference:

1. Check the last two lines of `.build_host/mine_car.actual.txt`. They must be
   `example profile works on every axis` and `### code=0`.
2. If you see `FAIL: an axis of the example profile did nothing` and `### code=1`, fix the
   `const bool ok =` line (step 2, point 4) and run the script again.
3. Copy the file and run the script again:

```bash
cp .build_host/mine_car.actual.txt test_host/expected/mine_car.txt
bash tools/run_host_tests.sh
```

Expected: `OK     mine_car` and `All tests match the references.`

> [!WARNING]
> **Do not use `--record-golden` for this**
>
> `bash tools/run_host_tests.sh --record-golden` records the references of **all** tests
> again, including the golden files of the reference car. Use it only when you have changed the
> behaviour on purpose and checked every difference. For one new test, copy its file as shown
> above.

## A faster loop for one test

The full script took 30 to 50 seconds in the tutorial run. While you work on the profile, you
can compile and run only your test. Run the full test script once first: it downloads the compiler into the `uv` cache,
and the command below uses that cache (`--offline`). From the `firmware` folder, in Git Bash:

```bash
LIBS="vehicle_settings.cpp radio_commands.cpp can_tx_gate.cpp settings_changer.cpp car_state.cpp decoder.cpp candump.cpp psa_frames.cpp xp_protocol.cpp xp_keys.cpp xp_keys_custom.cpp xp_parking.cpp xp_car.cpp psa_signals.cpp xp_signals.cpp radio_session.cpp power_policy.cpp parking_beeper.cpp xp_settings.cpp profile_psa_aee2010.cpp tx_allowlist.cpp protocol_xp.cpp profile_generic.cpp profile_mine.cpp vehicle_signals.cpp"
SRCS=""; for f in $LIBS; do SRCS="$SRCS $(ls lib/*/$f)"; done
INC=""; for d in lib/*/; do INC="$INC -I ${d%/}"; done
mkdir -p .build_host
uvx --offline --from ziglang python -I -m ziglang c++ -std=c++17 -O1 -Wall $INC -I include \
    test_host/mine_car.cpp $SRCS -o .build_host/mine_car.exe < /dev/null
./.build_host/mine_car.exe test_host/synthetic/mine_car.log
```

The file list is the same as `$S38_LIBS` in the test script, plus your profile and
`vehicle_signals.cpp`. If the list in the script changes, copy it from there. Run the full script
before you share anything.

## Also run the layering check

```bash
python -I tools/check_layering.py
```

It checks that your profile includes nothing from a radio protocol, from `src/` or from the
Arduino framework, and that no header name exists twice. In the tutorial run it listed
`car_mine` among the libraries and ended with `Layering OK.`

## What this test does not cover

- The code in `firmware/src/` (CAN driver, UART, 12 V outputs, sound) does not run on the PC. The
  first check of real hardware is the [bench test](../quick-start/bench-test.md).
- Timing on the real bus: the replay steps in 1 ms and loses no frames.
- Whether your decoding is right for the car. The test can only show what your profile makes of
  the frames. That is why you read the output against your markers before you store it.

Next: [Map the radio](radio-mapping.md).
