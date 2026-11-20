# Car recordings

A recording (capture) is a text file with every CAN frame the device saw, in order, with time.
The host tests replay recordings from the reference car. You make your own the same way.

## Format

One event per line, in the style of `candump -L` (Linux CAN tools):

```text
(13.497421) can0 0F6#805C1E23225D5C20     CAN frame: time in s, interface, ID#DATA
(12.346900) uartA FD04021117              bytes seen on one UART line between a CAN box and the radio
# STEP 3 19:06:41.149 press volume up     comment: a marker
```

- `ID` is three hex digits for a standard frame, eight for an extended one. `DATA` is the data
  bytes in hex, without spaces.
- Lines starting with `#` are comments. The frame parser (`firmware/lib/core/candump.cpp`) skips
  them, so markers never break a test.
- `uartA` / `uartB` lines exist only in recordings made with the UART recorder (see below).

The same `can0 ID#DATA` line is what the device prints on USB after the `dump` command. Do not
change this format: tools read it.

## The recordings in the repository

All in `firmware/test_data/captures/`, all from the reference car (Peugeot 508 SW, PSA AEE2010,
the bus at the radio, 125 kbit/s), all recorded listen-only.

| File | Recorded with | Scenario |
|---|---|---|
| `can_20261110_190421.log` | sniffer (CAN only), an aftermarket CAN box connected | long session: ignition, engine, steering wheel buttons, lights, doors, climate from the car panel and from the radio, parking sensors front and rear, parking tone trials, sleep |
| `can_20261111_190551.log` | recorder (CAN + both UART lines) | ignition off -> on, volume up three times |
| `can_20261111_190817.log` | recorder (CAN + UART) | steering wheel buttons, trip computer button, doors, boot |
| `can_20261111_191045.log` | recorder (CAN + UART) | doors, lights, engine, reverse, parking sensors, steering, climate, radio screens, vehicle settings from the radio, centre panel buttons, engine off |
| fifth file (see the README of the directory) | this firmware in decoder mode, no CAN box | about 5 minutes of CAN only, sensors inactive, one gap of lost frames |

The step markers in these files are in Polish (`# KROK` = step), as they were recorded.

## How the recordings were anonymised

On this car, frames `336`, `3B6` and `2B6` carry the VIN (the vehicle identification number) as
text. Before the files were added:

- the data of these three frames was overwritten with zeros of the same length; time, ID and
  length stay;
- a header line naming the serial port of the recording PC was replaced with `# capture`.

Nothing else changed. The host test results are identical for the original and the anonymised
files.

> [!WARNING]
> **Mask the VIN before you share a recording**
>
> Run the sanitiser on every file before you attach it to an issue or a pull request:
>
> ```bash
> python -I tools/sanitize_capture.py my_car.log my_car_clean.log
> python -I tools/sanitize_capture.py --check my_car_clean.log
> ```
>
> `--check` exits with code 1 if a private frame or a port header is left. On another car the
> VIN may sit in other frames: find them first and pass them with `--ids 3B6,2B6,...`.

## Make a recording

Three ways, all listen-only:

| Way | Tool | Notes |
|---|---|---|
| USB console of the device | type `dump` in a serial terminal, save the output | the device prints every frame as `can0 ID#DATA`; `dump` again stops it |
| Wi-Fi panel of the device | `python -I firmware/tools/record.py` on a PC joined to the device network | writes CAN frames and console lines into two files, outside the repository; asks for the device password without echo; writes VIN frames as `# masked` and gaps as `# lost N frames` |
| separate sniffer or recorder | `extras/sniffer/` and its `record_serial.py` | the recorder variant also logs the UART lines between a CAN box and the radio |

`record.py` and `record_serial.py` both let you add markers while you record; the marker goes
into the file as a `#` line. In `record.py` you type a text and press Enter: the line is
`# MARKER n ... text`, n counted from 1. In `record_serial.py` an empty Enter writes the next step
of the scenario file (`# STEP n HH:MM:SS.mmm text`), and a digit `1` to `9` + Enter writes
`# MARK d HH:MM:SS.mmm` (d = the digit typed). Details: [Record](../make-it-yours/record.md).

> [!NOTE]
> **Recording inside the browser panel loses frames**
>
> The CAN view of the panel records only while its tab is visible. When the author switched
> to another tab during a recording, tens of thousands of frames were lost (measured: one
> panel recording). Use `record.py` for anything longer than a quick look.

## Make a recording that is easy to decode

- **One action per step.** Write the steps first (see `extras/sniffer/scenarios/example_buttons.txt`).
- **Repeat each action three times**, with a pause of a few seconds. A bit that changes three
  times at the markers is your signal; a bit that changes all the time is a counter.
- **Put a marker before each action.**
- Start with a few seconds of nothing, so you see the idle state.
- Write down the car state: ignition, engine, doors, what the radio showed.

How to find the signal in the recording: [Decode](../make-it-yours/decode.md).
How to turn it into a test: [Test your profile](../make-it-yours/test-your-profile.md).
