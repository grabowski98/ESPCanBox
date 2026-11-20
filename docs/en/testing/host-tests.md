# Host tests

Most of the firmware logic does not touch the hardware: decoding CAN frames, building the frames
for the radio, the parking beep rhythm, the transmit gate. This logic lives in `firmware/lib/` and
is compiled **on your PC** as well. The host tests feed it with real recordings from the car and
compare the whole output with stored references.

Nothing is flashed and no port is opened. You do not need the board or the car.

## What you need

- **Git Bash** on Windows (it comes with Git for Windows), or any shell on Linux and macOS.
- **uv** (the Python package runner). Install it as described in
  [Flash the firmware](../quick-start/flash.md#1-install-uv); on Windows:
  `winget install --id=astral-sh.uv -e`. The test script uses uv to get a portable C++ compiler
  (`ziglang` package), so you do not have to install a compiler. The first run downloads it,
  needs internet and takes longer; later runs use the `uv` cache.
- Or your own compiler: `CXX=g++` or `CXX=clang++` in front of the command.

## Run all tests

Open Git Bash (or another shell) in the top directory of the repository and type:

```bash
cd firmware
bash tools/run_host_tests.sh
```

This works everywhere. If you have `make` (Linux, macOS; Git Bash does not include it), the same
from the top directory:

```bash
make test
```

With your own compiler:

```bash
cd firmware
CXX=g++ bash tools/run_host_tests.sh
```

The script builds every test into `firmware/.build_host/`, runs it, and compares the output with
the reference. It prints one line per test:

```text
OK     conformance
OK     replay_log
...
All tests match the references.
```

The real output has more lines than this, among them `compiler: ...` at the start and a few
`OK     golden ...` lines. What counts is the last line: `All tests match the references.`

If a line is not `OK`, the script stops at the first problem with a non-zero exit code:

| Line | Meaning |
|---|---|
| `OK     name` | output identical with the reference |
| `DIFF   name: reference ..., result ...` | output differs; the first 40 lines of the difference follow |
| `FAIL   name: compilation ...` | the compiler printed an error **or a warning**. Zero warnings is the rule |
| `FAIL   name: no capture ...` | a recording from `firmware/test_data/captures/` is missing |

Another directory of recordings can be given with `LOGS=/path/to/captures`.

## What the tests check

A short map. The script itself (`firmware/tools/run_host_tests.sh`) has a comment above every test.

| Test | What it checks | Input |
|---|---|---|
| `conformance` | buttons, parking sensors and power timing against what a real CAN box sent to the radio at the same moment | two recordings with CAN and UART lines |
| `replay_log` | what the device speaker would play | recordings |
| `beeper_scale` | beep rate for every sensor zone, continuous tone, silence after 400 ms without frames | synthetic frames |
| `power_loss` | ACC and ignition when `0F6` frames are lost during a firmware write | recording |
| `custom_keys_doors` | button remapping and door states | recording |
| `car_state` | car state from CAN frames, short and long frames, pasted frames | two recordings |
| `xp_codec` | radio protocol frames: checksum, length limit, timeout, garbage | synthetic bytes |
| `settings_38` | vehicle settings shown to the radio (UART frames `38`, `3C`) against the CAN box | three recordings |
| `radio_commands`, `settings_15b` | radio commands and the bytes of frame `15B` (only computed, nothing sent) | vectors, random frames |
| `radio_link`, `loop_policy` | queue and timing of frames to the radio, deferred flash writes | synthetic |
| `can_tx_gate`, `can_tx_gate_default` | the transmit gate with a stub instead of hardware; built with `-DCAN_TX_EXPERIMENTAL=1` and with `=0` | synthetic |
| `settings_change`, `can_tx_arm`, `tx_path`, `tx_path_default` | the whole path radio command -> gate -> stub port, arming tied to the build | synthetic and one recording |
| `key_tables`, `radio_none`, `profile_tables`, `example_car`, `radio_example` | car profiles and radio protocols, including the `generic` profile, the template profile and the template radio protocol (`radio_example`: its own framing only, no XP frame on the radio line) | recordings and a synthetic log |
| `net_policy`, `net_rings`, `net_util` | when Wi-Fi runs, panel buffers, password and firmware marker | synthetic, one fixture file |
| `golden_stream` (three passes) | the full stream to the radio, the outputs and the console, byte for byte | five recordings and four variants, see [Golden files](golden-files.md) |

> [!NOTE]
> **What the host tests do not cover**
>
> The code in `firmware/src/` talks to the hardware: CAN driver, UART, 12 V outputs, sleep,
> Wi-Fi, the transmit port. It is only compiled, not tested on the PC. It is checked on the
> bench with the [bench test protocol](bench-test-protocol.md).

## Other checks

These need no board either:

| Command (from `firmware/`) | What it checks |
|---|---|
| `python -I tools/check_layering.py` | `lib/core` does not include car or radio code, and other layer rules |
| `bash tools/check_no_can_tx.sh` | after building all environments: `twai_transmit` exists only in `decoder_tx_wifi` and only in one source file. It uses the `nm` tool that PlatformIO installs with the ESP32-S3 compiler; to use another one, set `NM=<path to xtensa-esp32s3-elf-nm>` |
| `bash tools/check_tx_profile_lock.sh` | a transmit build does not compile with the `GENERIC`, `EXAMPLE` or `CUSTOM` profile (the `CUSTOM` case builds with the generic header and object, so it reaches the `#error` in `src/config.h`) |
| `python -I tools/check_i18n.py` | the panel dictionaries have the same keys |

`make check` in the repository root (after `make build`) runs the transmit check, the project
identity check, the private data scan, the excluded-files check, the image metadata check and the
constant provenance check.

## Add a test

The project rule: a new function = logic in `firmware/lib/` + a test on a recording.

1. Write the logic in the right library (`lib/core` for car-neutral code, `lib/car_<profile>` for
   one car, `lib/radio_<protocol>` for one radio). No `Arduino.h` there.
2. Write `firmware/test_host/<name>.cpp`. It prints what the logic does, one line per event, and
   returns 0. `test_host/replay_frames.h` helps to replay a recording frame by frame.
3. Add one line to `firmware/tools/run_host_tests.sh`, next to similar tests:

    ```bash
    test_one my_test "my_logic.cpp psa_frames.cpp" can_20261111_191045.log
    ```

    The second argument lists the library files to link; each name must exist in exactly one
    `lib/*/` directory. Without a recording name the test runs once without arguments.
4. Record the reference: `bash tools/run_host_tests.sh --record-golden`.
5. Read the new `test_host/expected/my_test.txt` line by line. Check that it is what the car did.

> [!WARNING]
> **Recording references overwrites all of them**
>
> `--record-golden` writes **every** reference again, not only the new one. Before you commit,
> look at the difference of `firmware/test_host/` in Git. Any change in a file you did not mean
> to change is a change of behaviour: explain it in the pull request or undo it.

Next: to test a new car profile on your own recordings, see
[Test your profile](../make-it-yours/test-your-profile.md). How to make recordings:
[Car recordings](captures.md).
