# Peugeot 508 SW: vehicle settings from the radio screen

How the "vehicle settings" screen of an aftermarket radio (XP protocol, see
[radios/xp-simple-soft](../../radios/xp-simple-soft/protocol.md)) maps to the CAN frames of the
car. Byte numbers on CAN start at 1, bits 7..0 ("5.7" = byte 5 bit 7). UART bytes: DATA[0] =
first data byte.

Status words used here:

- **measured**: seen on this car (recording or console log of the device);
- **unverified**: public description (PSA-RE, Apache-2.0, commit 67f7581e), not confirmed here;
- **unverified (box firmware analysis)**: a bit of the XP frames to the radio that never changed in
  a recording; it only agrees with a constant value. Such bits decide only what the radio screen
  shows, never what is sent to the car. Every protocol **constant** has an own measurement since
  2026-11-19; constants without one (the `364` bits, the language codes other than Polish, the
  Stop and Start content of `1A9` and others) were removed from the firmware
  (see [radios/xp-simple-soft/provenance.md](../../radios/xp-simple-soft/provenance.md)).

## Frames

| Frame | Sender, period | Role |
|---|---|---|
| `260` | BSI, 500 ms, 7 bytes | personalisation **status** |
| `15B` | radio / telematics, on change, 8 bytes | personalisation **request** to the BSI |
| `361` | BSI, 500 ms, 6 bytes | which functions the car offers (the radio hides rows of absent functions) |
| `227` | BSI, 500 ms, 5 bytes | button LEDs |
| `3E1` | about 1 s | Stop and Start state |
| `1A9` | telematics, 200 ms, 8 bytes | screen buttons (one press = one frame with one bit) |

## Changing a setting (measured)

Radio sends `80 nn vv` -> the box (or this device) sends **one** `15B` 6-7 ms later -> the BSI
shows the new state in the next periodic `260` (0.15-0.55 s later) -> 10 ms later the box sends
the XP frame `38` with the new state to the radio. No repetitions were seen.

`15B` was built as: byte 1 = `00`, bytes 2-5 = bytes 2-5 of the latest `260` with one field
changed, byte 6 = `260` byte 6 with bits 3, 1, 0 cleared, bytes 7-8 = `00`. Bit 6.3 of `15B` is
a tyre pressure calibration **request** in PSA-RE (unverified) and is always sent as 0.

The BSI accepts a single `15B` from this device without the periodic frames that a telematics
unit normally sends (measured).

| Radio command | Setting | `260` / `15B` field | Status |
|---|---|---|---|
| `80 01 v` | parking assist | 5.7 | measured: aftermarket box recording (4 commands, 12 frames, all accepted) and this device |
| `80 02 v` | rear wiper in reverse | 5.5 | measured: accepted by the BSI, both ways |
| `80 06 v` | follow-me-home lighting, v = 0 off, 1-3 = 15/30/60 s | 4.7 on, 4.6-4.5 = v - 1 | measured: accepted (v = 3 and back) |
| `80 07 v` | welcome lighting, as above | 4.3 on, 4.2-4.1 = v - 1 | measured: accepted (v = 3 and back) |
| `80 09 v` | sound set, v = 1-4 | 2.1-2.0 = v - 1 (2.2 must already be 1) | measured: accepted with byte 1 = `00` |
| `80 0F v` | door unlocking, 1 = all doors, 0 = driver only | 3.6 inverted (1 = driver only) | measured: accepted (v = 0 and back) |

Measured examples (`260` before = `A4 1C 8F BB C5 50 00`):

| Command | `15B` sent |
|---|---|
| `80 01 00` | `00 1C 8F BB 45 50 00 00` |
| `80 01 01` | `00 1C 8F BB C5 50 00 00` |
| `80 02 01` | `00 1C 8F BB E5 50 00 00` |
| `80 06 03` | `00 1C 8F DB C5 50 00 00` |
| `80 07 03` | `00 1C 8F BD C5 50 00 00` |
| `80 09 02` | `00 1D 8F BB C5 50 00 00` |
| `80 0F 00` | `00 1C CF BB C5 50 00 00` |

What was measured is the BSI **accepting** the request (the new value in `260`). The visible
effect in the car (wiper, lights, sounds, remote) was not checked separately.

## Settings that are buttons, not `15B` fields

Two rows of the radio screen do not change a `15B` field. In the car they behave like a press
of a screen button: one `1A9` frame with one bit, which **toggles** the state. The state must be
known from the bus before sending, and nothing is sent when the car already has the wanted state.

| Radio command | Setting | `1A9` content | State read from | Status |
|---|---|---|---|---|
| `80 05 v` | parking sensors on/off | `00 7F FF 04 FF 00 10 00` | `227` 1.6 (LED "sensors off") | measured: aftermarket box, 4 presses, 4 reactions |
| `80 0C v` | blind spot monitoring | `00 7F FF 20 FF 00 10 00` | `227` 3.0 | measured: aftermarket box, 4 presses, 4 reactions |

All `1A9` presses were recorded while the aftermarket box was also sending `1A9` every 204 ms.
A single `1A9` from the device without the periodic frames (measured 18.11.2026): accepted for the
parking sensors (both ways) and blind spot off; blind spot on not accepted (one attempt; details in
[README.md](README.md#what-is-not-known)).

Stop and Start (`80 0D`, row "Start/Stop silnika") is **not** a button item in the firmware: no `1A9`
content for it was ever recorded on this car. A trial on 18.11.2026 with `00 7F FF 00 FF 00 90 00`
(bit 7.7, taken from public descriptions) was not accepted, and the content was removed from the
firmware on 19.11.2026. Its state (`3E1` byte 4: `40` active, `80` switched off by the ECO OFF
button, measured) is still read for the radio screen.

## Settings with no known frame

Boot unlocking (`80 03`), fuel units (`80 0A`), language (`80 0B`), Stop and Start (`80 0D`), tyre
pressure calibration (`80 10`), headlamp delay (`80 12`), traction control (`80 14`), driver fatigue
detection (`80 15`), instrument colour (`80 16`), automatic lights (`80 18`), automatic locking
(`80 19`), adaptive headlamps (`80 1A`), collision risk and the other rows `80 1B`-`80 1F`: the
command numbers are measured (next section), **the CAN frame is not.** The firmware refuses them.
Tyre pressure calibration must never be sent.

## The rows of the radio settings screen (measured 19.11.2026)

The owner went through the radio screen "Ustawienia funkcji" (function settings) row by row from
the top, with the device console recording every radio frame (`centralka_konsola_20261119_150048.txt`
in `firmware/test_data/captures`, 1223-1263 s, after owner marker 64), and photographed the screen.
The 26 different command numbers, in the order of their first occurrence, match the 26 rows one to
one (row 1 was clicked second); row 10 also has its own marker (65). The radio did not send `80 04`,
`80 08` or `80 0E`: the firmware treats these numbers as unknown.

| Row | Text on the screen (Polish) | Meaning | Command |
|---|---|---|---|
| 1 | Ustawienia języka | language | `80 0B` |
| 2 | Asystent parkowania | parking assist | `80 01` |
| 3 | Automatyczna tylna wycieraczka przy cofaniu | rear wiper in reverse | `80 02` |
| 4 | Oświetlenie „prowadź mnie do… (cut on the screen) | follow-me-home lighting | `80 06` |
| 5 | Oświetlenie powitalne | welcome lighting | `80 07` |
| 6 | Ustawienia dźwięku | sound set | `80 09` |
| 7 | Jednostki zużycia paliwa | fuel units | `80 0A` |
| 8 | Wykrywanie martwego pola | blind spot detection | `80 0C` |
| 9 | Otwarcie drzwi | door unlocking | `80 0F` |
| 10 | Kalibracja czujnika ciśnienia w oponach | tyre pressure calibration | `80 10` |
| 11 | Start/Stop silnika | engine Stop and Start | `80 0D` |
| 12 | Przełącznik radaru | parking sensors on/off | `80 05` |
| 13 | System kontroli trakcji | traction control | `80 14` |
| 14 | System wykrywania zmęczenia | driver fatigue detection | `80 15` |
| 15 | Kolor wyświetlacza wskaźnikó… (cut) | instrument display colour | `80 16` |
| 16 | Opóźnienie wyłączenia reflekto… (cut) | headlamp delay | `80 12` |
| 17 | Jednostka temperatury | temperature unit | `8A 0C` |
| 18 | Automatyczne światła | automatic lights | `80 18` |
| 19 | Automatyczne ryglowanie podczas jazdy | automatic locking while driving | `80 19` |
| 20 | Adaptacyjne reflektory | adaptive headlamps | `80 1A` |
| 21 | Collision risk Activation | collision risk alert on/off | `80 1B` |
| 22 | Collision risk Sensitivity | collision risk sensitivity | `80 1C` |
| 23 | Speed recommendation | speed recommendation | `80 1D` |
| 24 | Directional headlamps | directional headlamps | `80 1E` |
| 25 | Prevention of door mirror folding | door mirror folding | `80 1F` |
| 26 | Odblokowanie bagażnika | boot unlocking | `80 03` |

Some of these functions are not fitted to the reference car; they did not work with the Raise
and Simple Soft boxes either (owner's note).

## Climate control from the radio screen (`8A n v`)

The radio sends `8A n v` for each button of its climate screen. Recorded 19.11.2026 with owner
markers before each press (`centralka_konsola_20261119_150048.txt`, markers 33-46, each frame
0.7-1.2 s after its marker); `8A 0C` came from the settings screen (row 17):

| Command | Button | Values seen |
|---|---|---|
| `8A 01` | AUTO | `01` |
| `8A 02` | A/C | `01` |
| `8A 03` | MAX A/C | `01` |
| `8A 04` | left temperature | `01` plus, `02` minus |
| `8A 05` | right temperature | `01` plus, `02` minus |
| `8A 06` | air to face | `00` |
| `8A 07` | air to windscreen | `00` |
| `8A 08` | air to feet | `00` |
| `8A 09` | fan (screen label "wiatrak") | `02` |
| `8A 0A` | fan speed | `01` plus, `02` minus |
| `8A 0B` | MONO | `00` |
| `8A 0C` | temperature unit | `01` |

The "off / on" button of the radio climate screen sent no frame (marker 47). The car did not react
to any of these (`350` and `2AD` unchanged 930-1035 s); nothing was sent on CAN. The firmware only
prints the command with its name.

### Frame `364` does not control the climate

A box sends frame `364` with one button bit, about 200 ms later the idle frame `00 00 1E 00`.

| Recorded (aftermarket box, can_20261110_190421.log) | Meaning |
|---|---|
| `80 00 1E 00` x4 | left temperature + |
| `40 00 1E 00` x3 | left temperature - |
| `20 00 1E 00` x4 | right temperature + |
| `04 00 1E 00` x1 | fan - |
| `08 00 1E 00` x2 | fan + |

**The car did not react** (frame `350` unchanged; the same box with a newer firmware did control
the climate one day earlier, not recorded). Single `364` frames sent by this device on 18.11.2026
(4 and 6 bytes) were acknowledged on the bus and changed nothing either. The firmware has no `364`
path since 19.11.2026 (the trial, its console command and its allow-list row were removed).

## Sound screen of the radio (`84 n v`)

Recorded 19.11.2026 with owner markers 49-61 (`centralka_konsola_20261119_150048.txt`): `84 06`
treble, `84 05` bass, `84 03` fader, `84 04` balance, `84 09` sound selection, `84 02` surround,
`84 0A` volume / loudness. The value byte was `00` after a marker with the row name and `01` after a
marker "+"; its meaning is not determined. The first row of the screen ("Przełącznik wzmacniacza",
amplifier switch) was not clicked; its number is not known. The firmware only prints the command.

## XP frame `38` (vehicle state for the radio)

The radio shows the state of its settings screen from XP frame `38` (6 data bytes) and hides
rows according to XP frame `3C`.

| `38` | Content | Source on CAN | Status |
|---|---|---|---|
| DATA[0] bits 7, 6, 5, 4, 3 | doors: front (two), rear (two), boot | `1A1` door message (byte 2 = `DE`) | CAN side measured; bit positions in `38`: unverified (box firmware analysis) (no recording of a door change in `38`) |
| DATA[0] bit 2 | set with ignition on | | measured |
| DATA[2] bit 4 | parking assist | `260` 5.7 | measured (changed together, 2 times) |
| DATA[4] bit 7 | "blind spot" | `227` 3.0 | measured (3 changes) |
| DATA[4] bit 6 | Stop and Start | `3E1` byte 4 bits 7-6: `10` -> 1, `01` -> 0 | measured (1 change) |
| DATA[1] bit 7 | rear wiper in reverse | `260` 5.5 | unverified (box firmware analysis) |
| DATA[1] bit 2 | fatigue detection | `260` 7.4 | unverified (box firmware analysis) |
| DATA[1] bit 1 | instrument colour | `217` 8.1 | unverified (box firmware analysis) |
| DATA[2] bits 7-5 | ambient level | `260` 3.0, 2.5-2.3 | unverified (box firmware analysis) |
| DATA[2] bit 3 | "radar" | `227` 1.6 | unverified (box firmware analysis) |
| DATA[3] bits 7-6, 5-4 | follow-me-home, welcome lighting | `260` byte 4 | unverified (box firmware analysis) |
| DATA[3] bit 3 | daytime lights | `260` 3.1 | unverified (box firmware analysis) |
| DATA[3] bits 2-1 | sound set | `260` 2.1-2.0 | unverified (box firmware analysis) |
| DATA[4] bit 5, bit 4 | welcome function, door unlocking | `260` 3.3, 3.6 inverted | unverified (box firmware analysis) |
| DATA[4] bits 3-0 | language | `260` 1.6-1.2 through a table | Polish (`260` code 9 -> radio 8) measured; other codes are not translated (the field keeps its previous value) |
| DATA[5] bits 7-6 | fuel units | `260` 1.1, 1.0, 2.7 | unverified (box firmware analysis) |

The complete rule reproduces all 38 frames `38` recorded from the aftermarket box with 0
differences (test `settings_38` on the published captures). For the rows marked "unverified"
this only shows agreement with a value that did not change during the recordings.

XP frame `3C` (rows to show) is built from `361`; one value was recorded (`361` = `A0 66 C8 0A 00 00`
-> `3C` = `9E 80`, 9 of 9 frames). Bit assignment: unverified (box
firmware analysis). On this car the radio hides daytime lights, ambient light level and welcome function.
