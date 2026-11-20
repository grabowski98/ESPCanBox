# Decode frames

You have a recording with markers ([Record the bus](record.md)). Now you look for the bits that
changed when you did something in the car. This page does it step by step on real recordings of
the reference car, with the tool `firmware/tools/can_diff.py`.

The tool only reads files. It needs Python 3 and nothing else. All commands below run from the
repository folder.

The examples use recordings that are in the repository. Your own recording from `record.py` is in
the `logs` folder next to the repository folder, in a file whose name ends with
`_can_YYYYMMDD_HHMMSS.log`. Give its path instead, for example
`python -I firmware/tools/can_diff.py ../logs/<file name> --list`.

## Bytes and bits

A CAN frame has an ID (three hex digits, for example `0F6`) and up to 8 data bytes. This project
numbers them like this:

- bytes from **1**: byte 1 is the first data byte (`data[0]` in the C++ code);
- bits **7..0** inside a byte, bit 0 is the least significant;
- short form `byte.bit`: `8.7` = byte 8, bit 7.

Example: the frame `0F6#884D1E2182585828` has byte 1 = `88`, byte 2 = `4D`, ... byte 8 = `28`.

To see bits, write each hex digit as four binary digits:

| Hex | Binary (bits 7..0) |
|---|---|
| `24` | `0010 0100` |
| `A4` | `1010 0100` |

`24` and `A4` differ only in the leftmost bit, so the change is bit 7.

## What the tool does

For each marker, `can_diff.py` compares two time windows:

- **before**: the seconds just before the marker (3 s by default, `--before`);
- **after**: the seconds just after it (5 s by default, `--after`).

It lists every bit that was steady in "before" and changed in "after". Two filters remove most of
the noise:

- **counters**: a byte or half-byte that steps by the same amount in nearly every frame of the
  whole recording (rolling counters, timers). The tool prints them once at the top and never
  reports them.
- **background**: a bit that was already changing in the "before" window is ignored for that
  marker (the "ignored: N background bits" in the output).

It also lists IDs that appear only after the marker ("new IDs") and IDs that stop.

First check that the tool works on your computer. The synthetic file is an invented car with known
answers:

```
python -I firmware/tools/can_diff.py firmware/test_data/synthetic/marker_demo.log --selftest
```

```
selftest: 6 markers, 3216 frames, 0 errors
```

Useful options:

| Option | What it does |
|---|---|
| `--list` | lists the markers with their numbers and times, then stops |
| `--marker N` | only marker N (can be repeated) |
| `--match TEXT` | only markers whose text contains TEXT |
| `--after S`, `--before S` | window lengths in seconds |
| `--id 0F6,21F` | only these IDs |
| `--summary` | for the selected markers: how many of them each bit changed at |
| `--export OUT` | writes only the CAN lines to OUT, for other programs |

Markers are the comment lines that start with `# STEP`, `# MARK`, `# MARKER` or `# KROK` (the
Polish word used in the reference recordings). Another pattern goes with `--marker-regex`.

## Find the markers

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --list
```

```
firmware/test_data/captures/can_20261110_190421.log: 186700 frames, 70 IDs, 73 markers, 0 broken lines skipped
counters (never reported): 0E6:8H 10B:5L 236:5 3F6:3H
    1  line     21     397.922 s  # KROK 1 19:04:37.265 START: zaplon OFF, kluczyk wyjety, drzwi zamkniete, czekaj 10 s (magistrala w spoczynku)
    2  line     39     397.922 s  # KROK 2 19:04:54.247 Wloz kluczyk i wlacz zaplon (bez odpalania), czekaj 10 s
    3  line   4078     416.094 s  # KROK 3 19:05:19.255 Odpal silnik, 10 s bez ruchu
...
```

The counters line reads: in `0E6` the high half-byte (`H`) of byte 8 counts, in `10B` the low
half-byte (`L`) of byte 5, in `236` the whole byte 5, in `3F6` the high half-byte of byte 3. The
tool will never report them.

## Example 1: reverse gear (`0F6`)

Step 17 of this recording says, in Polish, "reverse: engage 3 s, disengage, 3 times (no
obstacle)". The step lasted about 27 s, so use a long "after" window:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 17 --after 25
```

Without `--id` the tool lists 13 IDs: `0A1 0B6 0E1 0E6 0F6 10B 128 161 227 236 2AD 3E7 3F6`.
The parking sensor frame `0E1` changes too, and slow values drift during 25 s. Look for the bit
that changes **as often as you did the action**. Here it is in `0F6`:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 17 --after 25 --id 0F6
```

```
==============================================================================
marker 17, line 43732, at 597.094 s
  # KROK 17 19:08:20.249 Wsteczny: wlacz 3 s, wylacz, 3 razy (bez przeszkody)
  window: 3.0 s before, 25.0 s after; ignored: 0 background bits, counters in 0 IDs
  0F6
    byte 2: 51 52
      bits: 2.1 0->1 x1 @+23.87s, 2.0 1->0 x1 @+23.87s
    byte 8: 24 A4 24 A4 24 A4 24 A4
      bits: 8.7 0->1 x7 @+8.67s
```

How to read it:

- `byte 8: 24 A4 24 ...` is the sequence of values the byte took, without repeats;
- `8.7 0->1 x7 @+8.67s`: bit 8.7 was 0 before the marker, changed 7 times, first at 8.67 s after
  the marker.

Bit 8.7 went to 1 and back again and again, in step with the action. Byte 2 changed once, near the
end: that is not the action.

The plan said three times; the bit went to 1 four times in the window. The recording does not say
why. Trust what the file shows, check it against your notes, and confirm a signal on a second
recording.

Result: reverse gear = `0F6` 8.7 (measured: `can_20261110_190421.log`, step 17; the same bit is in
`vehicles/peugeot-508-sw-2015/508.dbc`).

## Example 2: ignition and engine (`0F6` byte 1)

Some signals are not one bit but a whole byte with a few values. Step 3 says "start the engine,
10 s without moving":

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261110_190421.log --marker 3 --after 10 --id 0F6
```

```
==============================================================================
marker 3, line 4078, at 416.094 s
  # KROK 3 19:05:19.255 Odpal silnik, 10 s bez ruchu
  window: 3.0 s before, 10.0 s after; ignored: 0 background bits, counters in 0 IDs
  0F6
    byte 1: 88 90 91 89 8A 8E
      bits: 1.4 0->1 x2 @+3.30s, 1.3 1->0 x2 @+3.30s, 1.2 0->1 x1 @+5.02s, 1.1 0->1 x1 @+4.03s, 1.0 0->1 x2 @+3.46s
    byte 8: 20 24 64
      bits: 8.6 0->1 x1 @+6.12s, 8.2 0->1 x1 @+4.07s
```

Here the bit list is confusing, but the **byte values** tell the story: `88` before, then
`90 91 89 8A` while the starter turns, then `8E` with the engine running. With more recordings of
ignition off, on and engine start, the reference car gives `80` off, `88` on, `90`/`91`/`89`/`8A`
cranking, `8E` running, `86`/`84` switching off (measured: recordings with ignition and engine
steps, `508.dbc`).

Byte 8 also changed (`20 24 64`). What these two bits mean is not known. Do not give a meaning to a
bit just because it changed at the same time.

For the first switch-on there is no "before": the bus was asleep, and every ID is "new" at the
marker (see [Record](record.md#how-markers-get-their-time)). Then read the first frames directly.
The first `0F6` frame of this recording is:

```
(398.212947) can0 0F6#884D1E2182585828
```

Byte 1 = `88`: ignition on.

## Example 3: a steering wheel button (`21F`)

A clean case. Step 3 of `can_20261111_190817.log` says "steering wheel: volume up three times,
every 2 s". No `--id` this time:

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261111_190817.log --marker 3 --after 4.5
```

```
firmware/test_data/captures/can_20261111_190817.log: 30040 frames, 62 IDs, 14 markers, 0 broken lines skipped
counters (never reported): 0E6:8H,8L 10B:5L 236:5 3F6:3H
==============================================================================
marker 3, line 6809, at 223.999 s
  # KROK 3 19:08:54.163 Kierownica: glosnosc + trzy razy, co 2 s
  window: 3.0 s before, 4.5 s after; ignored: 10 background bits, counters in 4 IDs
  21F
    byte 1: 00 08 00 08 00
      bits: 1.3 0->1 x4 @+0.78s
  new IDs (absent before, present after): 1A5(4)
```

One bit: `21F` 1.3 (byte 1 = `08` while the button is held). The window of 4.5 s holds two of the
three presses: 4 changes. The next marker comes 4.5 s later, so a longer window would mix in the
next step.

The new ID `1A5` is not from the button. In this recording the aftermarket CAN box was connected,
and it sends `1A5` (the volume level) a few milliseconds after the radio changes the volume
(measured: [Peugeot 508 SW](../reference/vehicles/peugeot-508.md)). Other devices on the bus
react to your action too. Record once without them if you can.

Result: volume up = `21F` 1.3 (measured: `can_20261111_190817.log` and `can_20261111_191045.log`
with the UART line to the radio).

## Example 4: parking sensors (`0E1`)

A frame full of fields. Step 18 of `can_20261111_191045.log` says "rear sensors: reverse engaged,
move an object towards the rear bumper until the continuous tone, take it away, disengage
reverse". This file starts at step 13, so step 18 is marker 6 (check with `--list`):

```
python -I firmware/tools/can_diff.py firmware/test_data/captures/can_20261111_191045.log --marker 6 --after 25 --id 0E1
```

```
==============================================================================
marker 6, line 18775, at 421.532 s
  # KROK 18 19:12:11.696 Czujniki TYL: wsteczny wlaczony, zblizaj przedmiot do tylnego zderzaka az do tonu ciaglego, zabierz, wylacz wsteczny
  window: 3.0 s before, 25.0 s after; ignored: 2 background bits, counters in 0 IDs
  0E1
    byte 1: B0 B4 90 B0
      bits: 1.2 0->1 x2 @+2.53s
    byte 2: 00 D0 00 D0 50 D0 00
      bits: 2.7 0->1 x6 @+15.31s, 2.6 0->1 x4 @+15.31s, 2.4 0->1 x4 @+15.31s
    byte 3: 3F 30 2B 3F 2E 28 25 22 ...
      bits: 3.5 1->0 x2 @+16.86s, 3.4 1->0 x6 @+15.51s, 3.3 1->0 x10 @+15.31s, 3.2 1->0 x10 @+15.31s, 3.1 1->0 x14 @+15.31s, 3.0 1->0 x12 @+15.31s
    byte 4: FC F8 F4 F8 F4 F0 EC E8 ...
      bits: 4.7 1->0 x2 @+17.37s, 4.6 1->0 x2 @+17.75s, 4.5 1->0 x4 @+17.37s, 4.4 1->0 x2 @+16.86s, 4.3 1->0 x6 @+15.51s, 4.2 1->0 x10 @+15.31s
    byte 5: FC 5C 3C 1C FC
      bits: 5.7 1->0 x2 @+17.56s, 5.6 1->0 x2 @+17.82s, 5.5 1->0 x4 @+17.56s
```

When many neighbouring bits change together, they are one field. Split the bytes into groups of
bits that move together:

| Field | Where | What the values do here | Meaning on the reference car |
|---|---|---|---|
| rear state, front state | byte 1, bits 7-5 and 4-2 | `90` = `100 100 00`: rear 4, front 4 | 4 active, 5 waiting (measured) |
| sound request | 2.4 | 1 while the car beeps | measured |
| sides | 2.7-2.6 | `D0`: both sides | measured |
| beep interval | byte 3, bits 5-0 | `3F`, then falling values as the object comes closer | `3F` no sound, `39` ... `0B` beeps, `00` continuous tone (measured) |
| zones of the sensors | bytes 4-6, 3 bits each | values fall as the object comes closer | 0 nearest ... 7 far (measured) |

Meanings: `vehicles/peugeot-508-sw-2015/508.dbc` and
[Peugeot 508 SW](../reference/vehicles/peugeot-508.md#parking-sensors-0e1).

Bit 1.5 also changed (`B4` to `90`), but it is not in the list: it was already moving in the
"before" window, which overlaps the end of the reverse step. The background filter can hide a real
bit. If you expect a bit and do not see it, move the window: with `--before 1` this run shows
`1.5 1->0 x2` too. The byte values (`B0 B4 90 B0`) are always shown in full.

## Repeat the action, count the hits

The best proof is a bit that changes at **every** marker of the same action and at no other. With
one marker per repetition, `--match` and `--summary` count it for you. On the synthetic file (an
invented car, three door openings and three presses of a button):

```
python -I firmware/tools/can_diff.py firmware/test_data/synthetic/marker_demo.log --match door --summary
```

```
firmware/test_data/synthetic/marker_demo.log: 3216 frames, 5 IDs, 6 markers, 4 broken lines skipped
counters (never reported): 100:2,8L
==============================================================================
summary of 3 markers: 1 3 5
  3/3  300 2.5
```

`3/3  300 2.5` = in frame `300`, bit 2.5 (byte 2, bit 5) changed at all three door markers. With `--match volume` the answer is
`3/3  310 1.3`. The 4 broken lines are on purpose: the tool skips lines that look like frames but
are damaged (odd number of hex digits, more than 8 bytes, a 4-digit ID).

## How often a frame comes {#how-often-a-frame-comes}

Some signals need a frame that repeats often enough (see the
[Porting checklist](porting-checklist.md)). The tool does not show the period. Read it from the
file:

1. Open the recording in a text editor.
2. Search for `can0 0F6#` (put your ID in place of `0F6`).
3. Subtract the times of two lines that follow each other. In `can_20261111_191045.log`:
   336.199989 - 335.699989 = 0.5 s.
4. Check a few pairs. A frame sent only on change has long, irregular gaps.

In Git Bash, `grep "can0 0F6#" <file> | head -5` prints the first five lines of one ID.

## From bits to values

A field becomes a number with a **scale** and an **offset**:

```
value = raw × scale + offset
```

Example from the reference car: outside temperature = `0F6` byte 7, scale 0.5, offset -40, so
`value / 2 - 40` °C (measured: matches the temperature the CAN box sent to the radio in 21 of 21
frames, `can_20261111_191045.log`). In the frame `0F6#884D1E2182585828` above, byte 7 = `58` = 88,
and 88 / 2 - 40 = 4 °C.

To find a scale, record the value at two known points (for example the temperature shown on the
dashboard, or the radio volume) and compare.

### Write it into a DBC file

A DBC file is a text file that lists frames and signals. Many CAN tools read it. The reference car's
file is `vehicles/peugeot-508-sw-2015/508.dbc`. One line per signal:

```
BO_ 246 BODY_STATE: 8 CAR
 SG_ reverse : 63|1@0+ (1,0) [0|1] "" DEVICE
```

- `246` is the ID in decimal (`0F6` hex = 246).
- `63|1@0+`: start bit 63, length 1 bit, `@0` = big-endian byte order (also called Motorola; pick this in SavvyCAN or cantools), `+` = unsigned.
  With `@0`, start bit = (byte - 1) × 8 + bit of the highest bit of the field: byte 8 bit 7 =
  7 × 8 + 7 = 63. The beep interval, byte 3 bits 5-0, is `21|6@0+`.
- `(1,0)` = scale and offset; outside temperature has `(0.5,-40)`.

Give every signal a comment that starts with its status (`measured: ...` with the recording, or
`unverified: ...` with the source), as in `508.dbc`. Then turn the signals into a car profile:
[Write a car profile](write-a-profile.md).

## SavvyCAN and other tools

SavvyCAN is a free desktop program for CAN analysis that reads DBC files and log files in several
formats. This project has **no live link** to it: the firmware has no GVRET or similar protocol.
You work on recorded files. Whether your SavvyCAN version opens this project's recording format
directly was not checked; if it rejects the comment or `uartA` lines, export the CAN lines first:

```
python -I firmware/tools/can_diff.py my_recording.log --export frames_only.log
```

## Limits of the tool

- A marker has no time of its own: it gets the time of the next CAN frame. On a sleeping bus that is
  the moment the bus wakes up.
- A signal that changes slowly (temperature, fuel) may not change inside the window.
- A bit that was already moving before the marker is hidden (see example 4).
- The counter filter looks for steady steps. A counter that jumps, or a checksum, is not
  recognised and shows up as a busy bit in every window.
- Only 11-bit IDs (three hex digits) are read.

What is not known after decoding: what the car does with a value you did not see change. A signal
you found is measured for the actions you recorded, nothing more.

Next: [Write a car profile](write-a-profile.md).
