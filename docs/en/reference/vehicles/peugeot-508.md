# Peugeot 508 SW (2015)

The reference car of this project: the car the firmware was developed and tested on. Firmware
profile: `psa_aee2010` (build flag `-DCAR_PROFILE=PSA_AEE2010`, the default).

> [!NOTE]
> **Not affiliated**
>
> This project is not affiliated with Peugeot, Stellantis, Simple Soft, Raise, Junsun or FYT.
> The names are used only to say what was tested.

The repository keeps the detailed, binding files of this car in
`vehicles/peugeot-508-sw-2015/`:

- `README.md`: the signal list with status;
- `508.dbc`: frames and signals with a status comment on each (open it in SavvyCAN or any DBC
  tool);
- `settings-map.md`: vehicle settings from the radio screen;
- `wiring.md`: Quadlock pins, CAN box plug, radio plugs, and how to measure your own.

## Status words

- **measured**: seen on this car, in a recording (file named) or in the console of the device.
  Recordings used by the tests are in `firmware/test_data/captures/`; their VIN frames are
  overwritten.
- **unverified**: from a public description, mostly PSA-RE (github.com/prototux/PSA-RE,
  `buses/AEE2010.full/LS.INFO-DIV`, commit 67f7581e, Apache-2.0), not confirmed on this car.
  PSA-RE describes a whole family of cars. Three of its fields do not fit this car: `2E1`,
  `236` byte 8 and `3E1` 4.5 never change here.

Byte and bit numbering: CAN bytes from 1 (byte 1 = the first data byte), bits 7..0, short form
`byte.bit` (`5.7` = byte 5 bit 7). UART bytes: `DATA[0]` = the first data byte. See the
[glossary](../../contributing/glossary.md).

## The car

| Item | Value | Status |
|---|---|---|
| Model | Peugeot 508 SW, warranty start 31 March 2015, 1.6 e-HDi, manual gearbox | car documents (VIN document, diagnostic tool) |
| Electrical architecture | PSA AEE2010 | dealer diagnostic tool lists the modules BSI2010, COM2008P, ESP81; 66 of the 70 frame IDs at the radio match PSA-RE for AEE2010 by name and period |
| Factory radio | replaced by an aftermarket Android radio (FYT platform). Factory: navigation radio, no amplifier, 8 speakers | the author |
| Equipment that matters here | front and rear parking sensors, head-up display, Stop and Start with an ECO OFF button, blind spot monitoring | the author, dealer tool |
| Aftermarket CAN boxes tried | Simple Soft RP5-PA-001 ("XP" protocol), Raise RZ-PSA15 | the author |

The BSI is the body control unit of the car. It receives the personalisation requests described
below.

## The bus at the radio

| Fact | Status |
|---|---|
| CAN 125 kbit/s, 11-bit IDs | measured: reception without errors at this speed (`can_20261110_190421.log`) |
| about 218 frames/s with ignition on, 70 IDs, 58 of them periodic | measured (`can_20261110_190421.log`) |
| 66 of the 70 IDs have the name and period given by PSA-RE for INFO-DIV; unknown: `2E3`, `323`, `363` | measured against PSA-RE |
| Quadlock pin 10 = CANH, pin 13 = CANL | measured: continuity from the CAN box transceiver to the Quadlock, and error-free reception (17 169 frames) with this assignment |
| CANH 0.65 V, CANL 4.5 V average to ground, ignition on, multimeter DC | measured; the same with and without the device connected |
| bus type | **unverified.** The voltages look like low-speed fault-tolerant CAN (ISO 11898-3), not high-speed CAN (both lines near 2.5 V at rest). An oscilloscope is needed to confirm |
| a high-speed transceiver (3.3 V) still receives it without errors | measured |
| key out: no frames | measured |
| after switch-off: full traffic for about 100 s, then frame `036` alone for 14 s, then silence until the car is opened | measured |
| frames `336`, `3B6`, `2B6` carry the VIN as ASCII | measured. **Mask them before you share a recording** (`tools/sanitize_capture.py`) |

How to connect to this bus: [CAN connection](../../install-in-car/can-connection.md).

## Signals used by the firmware

All rows are measured on this car unless the row says otherwise.

| Signal | Frame and field | How it was confirmed |
|---|---|---|
| ignition / engine | `0F6` byte 1: `80` off, `88` on, `8E` running (`90`, `91`, `89`, `8A` cranking; `86`, `84` switching off) | recordings with ignition and engine steps |
| reverse gear | `0F6` 8.7 | reverse engaged 3 times in a recording |
| indicators | `0F6` 8.1-8.0 | step in a recording |
| outside temperature | `0F6` byte 7: value / 2 - 40 °C | matches the box frame to the radio in 21 of 21 frames |
| lights on | `036` 4.5 | lights on / off steps |
| economy mode (ignition on, engine off) | `036` 3.7 | 10 of 10 frames |
| panel power knob pressed | `036` byte 7: `01` idle, `02` pressed | CAN + UART recording |
| parking sensors | `0E1`, see [below](#parking-sensors-0e1) | many approaches front and rear; the box frame to the radio matches in 394 of 396 frames |
| sensors inhibited | `126` byte 1 = `18` | 4 presses of the screen button |
| steering wheel buttons | `21F` (volume, phone, LIST, scroll wheel and its press), `0A2` (SRC, left thumb wheel) | 36 of 36 presses matched the box frames to the radio |
| centre panel buttons | `122` (NAV, MEDIA, TEL, SETUP, DRIVE, seek, zoom, screen off, mic mute, WEB, volume knob) | each button pressed in a recording; one button (code `22`) not identified |
| trip computer | `221` (current page, stalk button), `2A1` trip 1, `261` trip 2 | copied byte for byte by the box (all 9-11 frames each); meaning of the values unverified |
| steering angle | `10B` bytes 1-2 | copied by the box, 155 of 155 frames; scale unverified |
| climate state | `350` + `2AD` (rear window heating) | 26 of 27 box frames matched |
| door open message | `1A1` byte 2 = `DE`, door bits in byte 4 | each door in a recording |
| Stop and Start state | `3E1` byte 4 | see [ECO OFF and ASR buttons](#eco-off-and-asr-buttons) |
| ASR | `217` 3.6 press, `227` 1.4 state | see [ECO OFF and ASR buttons](#eco-off-and-asr-buttons) |
| personalisation | `260` status, `15B` request | see [below](#personalisation-with-15b-and-260) |

The full bit positions are in `vehicles/peugeot-508-sw-2015/508.dbc`.

The physical climate panel of the car sends no button frames on this bus: only the state frame
`350` changes (measured).

### Parking sensors 0E1

Frame `0E1` comes from the parking sensor module every 100 ms (measured:
`can_20261110_190421.log`, `can_20261111_191045.log`).

| Field | Content | Status |
|---|---|---|
| rear state, front state | 4 active, 5 waiting, 2 switched off by the driver, 6 after ignition off | measured |
| sound request | 1 while the car beeps | measured |
| front / rear | 1 = obstacle in front, 0 = behind | measured (CAN + UART recording) |
| beep interval | `3F` no sound, `39` ... `0B` intermittent beeps, `00` continuous tone. After `0B` the next value is `00`. Unit not determined | measured |
| zones of six sensors (rear L/C/R, front L/C/R) | 0 nearest ... 7 far | measured |
| display request | 1 together with an obstacle | measured |

The front sensors start to request sound at interval `18` (measured with this device in the car).

## Personalisation with 15B and 260

The radio "vehicle settings" screen changes personalisation items in the BSI.

| Frame | Sender, period | Role |
|---|---|---|
| `260` | BSI, 500 ms, 7 bytes | personalisation **status** |
| `15B` | radio / telematics, on change, 8 bytes | personalisation **request** to the BSI |
| `361` | BSI, 500 ms, 6 bytes | which functions the car has (the radio hides rows of absent functions) |

How one change goes (measured with the aftermarket box and with this device): the radio sends
`80 nn vv` -> one `15B` follows about 6 ms later -> the BSI shows the new state in the next `260`,
0.15-0.55 s later. No repeated `15B` was seen.

`15B` is built like this: byte 1 = `00`; bytes 2-5 = bytes 2-5 of the latest `260` with one field
changed; byte 6 = `260` byte 6 with bits 3, 1 and 0 cleared; bytes 7-8 = `00`. Bit 6.3 is a tyre
pressure calibration request in PSA-RE (unverified) and is always sent as 0.

**The BSI accepts a single `15B` from this device**, without the periodic frames a telematics
unit normally sends (measured: console of an earlier transmit build of this firmware in the car;
log not published). Each item below was accepted in both directions:

| Radio command | Item | `260` / `15B` field | `15B` sent (before: `260` = `A4 1C 8F BB C5 50 00`) | Status |
|---|---|---|---|---|
| `80 01 v` | parking assist | 5.7 | `80 01 00`: `00 1C 8F BB 45 50 00 00`; `80 01 01`: `00 1C 8F BB C5 50 00 00` | measured: accepted after 555 ms and 367 ms; also in the aftermarket box recording (4 commands, all accepted) |
| `80 02 v` | rear wiper in reverse | 5.5 | `80 02 01`: `00 1C 8F BB E5 50 00 00` | measured: accepted |
| `80 06 v` | follow-me-home lighting, v = 0 off, 1-3 = time | 4.7 on, 4.6-4.5 = v - 1 | `80 06 03`: `00 1C 8F DB C5 50 00 00` | measured: accepted |
| `80 07 v` | welcome lighting, as above | 4.3 on, 4.2-4.1 = v - 1 | `80 07 03`: `00 1C 8F BD C5 50 00 00` | measured: accepted |
| `80 09 v` | sound set, v = 1-4 | 2.1-2.0 = v - 1 (2.2 must already be 1) | `80 09 02`: `00 1D 8F BB C5 50 00 00` | measured: accepted, byte 1 = `00` is enough |
| `80 0F v` | door unlocking, 1 = all doors, 0 = driver only | 3.6 inverted | `80 0F 00`: `00 1C CF BB C5 50 00 00` | measured: accepted |

The BSI acceptance took 140-572 ms in these trials. After the trials `260` returned to
`1C 8F BB C5 50` in bytes 2-6.

> [!WARNING]
> **Accepted is not the same as working**
>
> What was measured is the BSI **accepting** each request: the new value appears in `260`. The
> visible effect in the car (wiper, lights, sounds, unlocking) was not checked.

The other rows of the radio screen (boot unlocking, fuel units, language, Stop and Start, tyre
pressure calibration, headlamp delay, traction control, fatigue detection, instrument colour,
automatic lights, automatic locking, adaptive headlamps and the rows `80 1B`-`80 1F`) have **no
measured frame**. The firmware refuses them. Tyre pressure calibration must never be sent. The
command number of every row was recorded on 2026-11-19 by going through the screen row by row
(table in `vehicles/peugeot-508-sw-2015/settings-map.md`); the radio did not send `80 04`, `80 08`
or `80 0E`, so the firmware treats these numbers as unknown.

## Screen buttons with 1A9

Two rows of the radio screen do not change a `15B` field. In the car they act like a press of a
screen button: one `1A9` frame with one bit, which **toggles** the state. So the state must be
read from the bus before a press, and nothing is sent when the car already has the wanted state.

| Radio command | Item | `1A9` content | State read from | Recorded from the aftermarket box | Sent by this device (single frame) |
|---|---|---|---|---|---|
| `80 05 v` | parking sensors on / off | `00 7F FF 04 FF 00 10 00` | `126` 1.4-1.3 (`18` = off, `00` = on); until 2.0.0-beta.1 `227` 1.6 (LED "sensors off") | measured: 4 presses, 4 reactions (`can_20261110_190421.log`) | measured: **accepted both ways**; `227` changed after 286 ms and 236 ms, `0E1` state went 6 -> 2 -> 6 |
| `80 0C v` | blind spot monitoring | `00 7F FF 20 FF 00 10 00` | `227` 3.0 | measured: 4 presses, 4 reactions (`can_20261110_190421.log`) | measured: switching **off accepted** (76 ms); switching on 1.2 s later **not accepted** (no change in `227` for 2.5 s). One trial, cause not known |

Stop and Start (`80 0D`) is not a button item: a single `1A9` `00 7F FF 00 FF 00 90 00` (bit 7.7,
from public descriptions, never recorded on this car) was **not accepted** (two trials, `3E1`
unchanged), and the firmware no longer has this content: it refuses `80 0D` (no known frame).

The aftermarket box sent `1A9` every 204 ms in the background. The device sends only the single
press frame; a build with `-DCAN_TX_1A9_RELEASE=1` adds one release frame `20 7F FF 00 FF 00 50 00`
after an acknowledged press. That is the box's periodic `1A9` with ignition on (4189 frames in
`can_20261110_190421.log`): the box sent no separate release, a press replaced one periodic frame
(e.g. 1207.859 s) and the next periodic frame followed it (1208.066 s). `00 7F FF 00 FF 00 10 00`
was seen only with ignition `80` and once at box start-up; it is never sent. Whether a press without
a following byte-4 = `00` frame causes a later toggle is a hypothesis, not measured.

The parking sensors state comes from `126` byte 1 (sound and visual inhibition): it stayed `18`
through ignition off and on, while `227` 1.6 showed the value from before the ignition for about
95 ms after ignition on and the `227` lamp test lasts 3.0 s (`centralka_can_20261119_150048.log`
487.68 s). The firmware refuses both button items during the first 3.5 s after ignition on. The device results come from the console of an earlier transmit build of this
firmware in the car (log not published).

Bit `227` 3.0 toggles with the blind spot button and also clears in reverse and after engine off;
its exact meaning on this car is not determined. In reverse it clears about 40 ms after the gear is
engaged and returns about 40 ms after it is released (`can_20261111_151209.log`, all 20 reverse cycles),
so the firmware treats the blind spot state as unknown (`80 0C` refused, nothing sent) while reverse is
engaged and for 500 ms after it is released.

> [!CAUTION]
> **Not in the default transmit build**
>
> The firmware sends `1A9` only in a build with `-DCAN_TX_EXPERIMENTAL=1`. The default transmit
> build refuses it. See [Transmitting on the CAN bus](../../safety/transmit.md).

## ECO OFF and ASR buttons

Measured with this device in listen-only mode, engine running, 36 s recording (not published).

| Button | What changes on the bus | Status |
|---|---|---|
| ECO OFF (Stop and Start off) | `3E1` byte 4: `40` = Stop and Start active, `80` = switched off with the button. At the same time `1A1` shows `02 21 A4` (switched off) and `02 20 A4` (switched on again) | measured |
| ASR (traction control off) | one pulse in `217` 3.6 per press (`F0 80 40 ...`, about 130-190 ms). State in `227` 1.4: `10 00 11 00 00` while ASR is off. Two `1A1` messages appear with the changes: `82 45 A4` and `82 44 A4` | measured |

In the same recording `128` 6.4 changed 8 times, at the time of the ECO OFF presses. The cause is
not known.

Two buttons added by the author on the centre console (not factory parts) send **nothing** on this
bus: no frame changed while they were pressed, apart from counters and temperature (measured).
There is no parking sensor off switch in this car.

## Climate control with 364 does not work

The radio climate screen sends `8A n v`. A CAN box would then send frame `364` with one button bit
and, about 200 ms later, the idle frame `00 00 1E 00`. **None of this changed the climate control
of this car.**

| Trial | Frames on the bus | Result | Status |
|---|---|---|---|
| aftermarket box (Simple Soft, older firmware), presses on the radio screen | 4-byte `364`: `80 00 1E 00` x4 (left temperature +), `40 00 1E 00` x3 (left -), `20 00 1E 00` x4 (right +), `08 00 1E 00` x2 (fan +), `04 00 1E 00` x1 (fan -), idle `00 00 1E 00` every 1004 ms, each press followed by the idle frame after 202 ms | `350` unchanged | measured (`can_20261110_190421.log`) |
| this device, 4-byte frame | `80 00 1E 00` (4 times) and `40 00 1E 00` (once), each with the idle frame 0.5 s later; all acknowledged on the bus after 4-7 ms | temperature unchanged | measured (console, not published) |
| this device, 6-byte frame | `80`, `40`, `04` in byte 1, four presses with release, acknowledged after 4-7 ms | no reaction | measured (console, not published) |

In the box trial the periodic radio / telematics frames (`1A9`, `167`, `165`) were on the bus as
well, so they alone are not enough. The author saw the same box with a newer firmware control the
climate one day earlier; that was not recorded.

PSA-RE describes `364` with 6 bytes, a 1000 ms period plus on change, and an idle content
`00 00 1E 78 E0 00` (unverified: PSA-RE `LS.INFO-DIV/364.yml`). That idle form has not been tested
as a periodic frame on this car. The other bit names of PSA-RE (AUTO, A/C, MAX, air distribution,
MONO and others) are unverified.

**Conclusion: single `364` frames from the device do not control the climate.** The firmware has no
`364` path since 2026-11-19 (the trial was removed). The numbers of the radio climate screen buttons
(`8A 01`-`8A 0C`) were recorded with owner markers on that day; the firmware only prints them.

## XP frame 38

The radio draws its settings screen from XP frame `38` (6 data bytes, box -> radio) and hides rows
according to XP frame `3C`. See also the [XP radio protocol](../xp-protocol.md).

| `38` | Content | Source on CAN | Status |
|---|---|---|---|
| `DATA[0]` bits 7, 6, 5, 4, 3 | doors: front (two), rear (two), boot | `1A1` door message | CAN side measured; bit positions in `38` unverified (no door change recorded in `38`) |
| `DATA[0]` bit 2 | set with ignition on | | measured |
| `DATA[2]` bit 4 | parking assist | `260` 5.7 | measured (changed together, 2 times) |
| `DATA[4]` bit 7 | blind spot | `227` 3.0 | measured (3 changes) |
| `DATA[4]` bit 6 | Stop and Start | `3E1` byte 4 bits 7-6 | measured (1 change) |
| `DATA[4]` bits 3-0 | language | `260` 1.6-1.2 through a table | only Polish (`260` code 9 -> radio 8) measured; other codes are not translated (the field keeps its value) |
| other bits | rear wiper, fatigue detection, instrument colour, ambient light, lighting times, daytime lights, sound set, welcome function, door unlocking, fuel units | `260`, `217`, `227` | unverified |

The complete rule in the firmware reproduces all 38 frames `38` recorded from the box, with 0
differences (host test `settings_38`). For the unverified bits this only shows agreement with a
value that did not change during the recordings.

`3C` is built from `361`. One value was recorded: `361` = `A0 66 C8 0A 00 00` gives `3C` =
`9E 80` (9 of 9 frames). The bit assignment is unverified.

## Frames sent by an aftermarket CAN box

The Simple Soft box acted as the factory radio **and** as the telematics unit on this bus. Seen in
recordings:

| Frame | What changes | Status |
|---|---|---|
| `165` radio status | source (byte 3 bits 6-4), mute | measured |
| `1A5` volume | level from the radio volume command, 4-10 ms later | measured |
| `1E5` audio settings | single bytes change with balance / bass on the radio | measured; no amplifier in this car to react |
| `15B` personalisation request | after `80 nn vv` from the radio, accepted by the BSI | measured |
| `1A9` screen buttons | sensors off, blind spot | measured |
| `364` climate buttons | pulses after temperature changes on the radio; **the car did not react** | measured |
| `2E3`, `363` | text / frequency for the cluster, change with the source | measured; layout unverified |
| `3E5`, `167`, `1A3`, `323` | constant in the recordings | measured |

One short recording without a box exists (`firmware/test_data/captures/centralka_can_20261117_184411.log`,
device in decoder mode, about 10 s of frames). None of the frames in the table appears in it,
though `167` (every 104 ms) and `1A9` (every 204 ms) would show up dozens of times in 10 s if
another unit sent them (measured). This partly confirms that the box sends them. The recording is
short and does not repeat the same steps, so what the car misses without them is not known.

## Known issue: parking tone keeps sounding

With an aftermarket box installed, the continuous parking tone sometimes kept sounding after the
engine was switched off with an obstacle in the closest zone. At switch-off the parking module
always sent `0E1` **without** a sound request (measured), so the stuck tone is held further in the
car, not by a frame on this bus. The tone once got stuck with no box connected. The full story:
[The parking tone bug](../../case-studies/parking-tone-bug.md).

## What is not known

- The bus type: needs an oscilloscope.
- What the car misses without the frames of an aftermarket box (only one short recording without a box exists).
- The visible effect of the accepted `15B` requests.
- Why switching blind spot monitoring on with a single `1A9` was not accepted (one trial).
- The `1A9` content for Stop and Start, and whether the BSI accepts it at all.
- Whether `364` with the 6-byte idle form sent periodically changes anything.
- Meaning of `2E3`, `323`, `363`, of the trip computer values and of the steering angle scale.
- Which panel button sends XP code `22`.
