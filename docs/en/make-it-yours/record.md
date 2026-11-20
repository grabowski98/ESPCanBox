# Record the bus

A recording (a capture) is a text file with every CAN frame the device saw, in order, with time.
You make it while you do things in the car: press a button, open a door, engage reverse. Later
you look for the bits that changed at those moments ([Decode](decode.md)).

You need the facts from [Find your bus](find-your-bus.md) first: the wires, the kind of bus and
the bit rate.

> [!CAUTION]
> **Record with a listen-only build**
>
> Use `listen_generic_wifi` or the separate sniffer in `extras/sniffer/`. Both only listen. Do not
> record an unknown car with a build that can transmit.

## The format

One event per line, the same format everywhere in this project:

```
(416.094276) can0 532#0800000000000000      CAN frame: time in s, interface, ID#DATA
(12.346900) uartA FD04021117                UART bytes (only the sniffer's recorder variant)
# STEP 3 19:06:41.149 Volume + three times   comment line: a marker
# stat rx=30529 ...                          comment line: statistics
```

Lines that start with `#` are comments. Some comments are **markers**: lines you add while you
record, at the moment you start an action. Markers are what makes a recording easy to decode.
Each tool writes its own kind:

| Tool | Marker line |
|---|---|
| `record.py` | `# MARKER n (time) computer clock HH:MM:SS: text` |
| `record_serial.py` (sniffer) | `# STEP n HH:MM:SS.mmm text` (Enter), `# MARK d HH:MM:SS.mmm` (a digit `1` to `9` + Enter) |

More about the format: [Car recordings](../testing/captures.md).

## Choose a tool

- Use `firmware/tools/record.py` if you have the device with `listen_generic_wifi` and a laptop
  on its Wi-Fi. The file goes to a `logs` folder next to the repository folder.
- Use `extras/sniffer/record_serial.py` if you have the separate sniffer board on USB. The file
  goes to the folder given with `--out`.
- Use `dump` in the USB console only for a quick look: it writes no markers.

## Plan the steps first

Write the steps down before you sit in the car. A good plan:

- **Start with a few seconds of nothing**, so the recording shows the car at rest.
- **One action per step.** "Volume up" and "volume down" are two steps.
- **Repeat each action three times**, with a pause of 2 to 3 seconds. A bit that changes three
  times is your signal. A bit that changes all the time is a counter or noise.
- **Set the marker just before the action**, not after it.
- **Note the car state** at the start of each step: ignition, engine, doors, what the radio shows.

An example plan in English is `extras/sniffer/scenarios/example_buttons.txt`. It starts like this:

```
START: ignition off, key out, radio off, wait 10 s
Insert the key, ignition on (engine off), wait until the radio has started, then 10 s
Steering wheel: volume + three times, every 2 s
Steering wheel: volume - three times, every 2 s
```

The recordings of the reference car in `firmware/test_data/captures/` were made with plans like
this one. Their step lines are in Polish (`# KROK n` = "step n") and were kept as recorded.

## Record over Wi-Fi with `record.py`

1. Build and flash `listen_generic_wifi` with the bit rate you found: from the `firmware` folder,
   `uvx platformio run -e listen_generic_wifi -t upload` ([Flash the firmware](../quick-start/flash.md)).
2. Connect the device to the car ([CAN connection](../install-in-car/can-connection.md)).
3. Join the device's Wi-Fi network with a laptop ([First boot](../quick-start/first-boot.md)).
   Use a laptop on battery, or powered from the car.
4. From the repository folder, run:

    ```
    python -I firmware/tools/record.py
    ```

    The default address is `10.50.8.1`. Another address goes as the first argument.
5. Type the device password when asked. It is the same as the Wi-Fi key; `wifi showpass` on the
   USB console shows it ([First boot](../quick-start/first-boot.md)). It is not shown on screen
   and not saved.
6. Wait for `[ok] connected, recording`. The script prints the names of two files: one with the CAN
   frames, one with the console lines of the device.
7. For each step: type a short text (for example `volume up 3x`), press Enter, then do the action.
8. Type `q` and Enter to stop.

`record.py` only reads from the device. It sends no commands and changes nothing. Frames `336`,
`3B6` and `2B6` are written as `# masked`, because on the reference car they carry the VIN. On your
car the VIN may be in other frames: find them before you share anything ([Share your car](contribute.md)).

> [!WARNING]
> **`record.py` always hides `336`, `3B6` and `2B6`**
>
> On another car these three IDs may carry ordinary signals, and the script has no option to
> keep them. If you cannot find a signal, make one recording with the sniffer or with `dump`:
> both keep every frame, so clean that file before you share it.

If the Wi-Fi link drops, the script keeps trying. Frames that the device could not hand over are
written as `# lost N frames`. The device keeps about 9 s of frames in memory, so short drops lose
nothing.

> [!NOTE]
> **Do not record in the browser panel**
>
> The CAN view of the web panel records only while its browser tab is visible. When the author
> switched to another tab during a panel recording, 26 593 frames were lost (measured: the
> `# zgubiono 26593 ramek` line, Polish for "lost 26593 frames", in
> `centralka_can_20261117_184411.log`). Use `record.py`.

## Record over USB with the sniffer

The sniffer is a small separate program for an ESP32-S3 board with a CAN transceiver module and no
transmit wire at all. Build, wiring and the recording command are in `extras/sniffer/README.md`. In
short:

```
uv run --with pyserial python -I extras/sniffer/record_serial.py COM7 --scenario extras/sniffer/scenarios/example_buttons.txt
```

Run it from the repository folder. Replace `COM7` with your port: `uvx platformio device list`
shows it. The script shows the next step of the scenario. Press Enter when you start it.

## How markers get their time

A comment line has no time of its own. The decode tool ([Decode](decode.md)) gives each marker the
time of the first CAN frame after it. A marker from `record.py` carries the device time in
brackets, and that time is used instead.

This matters when the bus is asleep. In the reference recording `can_20261110_190421.log` the bus
was silent during the first two steps ("start, wait 10 s" and "ignition on"). Both markers got the
same time, the moment the bus woke up:

```
    1  line     21     397.922 s  # KROK 1 19:04:37.265 START: zaplon OFF, kluczyk wyjety, drzwi zamkniete, czekaj 10 s (magistrala w spoczynku)
    2  line     39     397.922 s  # KROK 2 19:04:54.247 Wloz kluczyk i wlacz zaplon (bez odpalania), czekaj 10 s
```

So for "ignition on", look at the first frames of the recording, not at a change around the
marker.

## After the recording

- Check the numbers the script prints at the end: frames, markers, lost frames.
- Open the file in a text editor and read the comment lines. Your marker texts are copied into
  the file unchanged: do not type anything private into them.
- Keep the original file. Work on a copy.
- Before you share a recording with anyone, remove the VIN and check the file
  ([Share your car](contribute.md)).

Next: [Decode frames](decode.md).
