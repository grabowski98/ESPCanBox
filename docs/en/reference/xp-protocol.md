# XP radio protocol

The "XP" protocol is the serial link between an aftermarket CAN box and an Android radio. This
page describes it as it was **recorded** on the reference car: a Simple Soft box (RP5-PA-001) and
a radio on the FYT platform, CAN profile "XP Simple Soft -> Peugeot 508 -> Type 2" in the radio.

> [!NOTE]
> **Not affiliated**
>
> This project is not affiliated with Simple Soft, FYT, Junsun or Peugeot. The names are used
> only to say what was tested.

## Where the facts come from

Everything on this page is **measured**, unless a line says otherwise. Both UART lines (box to
radio and radio to box) and the car CAN bus were recorded on one clock with the recorder in
`extras/sniffer`. The two recordings are in `firmware/test_data/captures/`:

- `can_20261111_190817.log`: steering wheel buttons, trip computer button, doors, boot;
- `can_20261111_191045.log`: doors, lights, engine, reverse, parking sensors, steering, climate,
  radio screens, vehicle settings from the radio, centre panel buttons.

The longer, binding version of this page is in the repository: `radios/xp-simple-soft/protocol.md`.
The source of every protocol constant in the firmware is listed in
`radios/xp-simple-soft/provenance.md`. Commands that were never seen on the wire are only listed in
[Commands the firmware only names](#commands-the-firmware-only-names).

How the device uses the protocol:

- in the `listen` and `listen_wifi` environments the device only **listens** to the radio -> box
  line. It sends nothing to the radio;
- in the `decoder` environments the device **replaces** the box and sends the box -> radio frames
  itself. Unplug the box first: two transmitters on one UART line short each other.

## Byte numbering

UART frames are counted with `DATA[n]`: `DATA[0]` is the first data byte after the header and
the length. CAN bytes on this page are counted from 1 (byte 1 = the first data byte), as on every
CAN page of this site. See the [glossary](../contributing/glossary.md).

## Transport

| Fact | Status |
|---|---|
| UART 38400 baud, 8 data bits, no parity, 1 stop bit (8N1) | measured: every recorded frame has a valid checksum at this speed |
| 3.3 V logic levels | measured |
| box -> radio and radio -> box on two separate wires | measured; pins in `vehicles/peugeot-508-sw-2015/wiring.md` |
| frame: `2E CMD LEN DATA[LEN] CHK` | measured |
| `CHK = ~(CMD + LEN + sum of DATA) & 0xFF` | measured |
| one acknowledge byte after every frame: the radio sends `FF`, the box sends `F3` (`F0` once at start-up) | measured |
| commands `01`-`7F` go from the box to the radio, `80`-`FF` from the radio to the box; exception: `EA` from the box | measured |
| at power-up the box prints ASCII text (version, build date) on its line, then frames start | measured |

Checksum example: `2E 29 02 1C FF B9`. Add `29 + 02 + 1C + FF = 146`, keep the low byte `46`,
invert it: `B9`.

A receiver must skip every byte that is outside a frame (the start-up text, the acknowledge
bytes).

Timing of the box (measured): 5-25 ms from a CAN change to the frame for the radio; 4-10 ms from a
radio command to the CAN frame.

## Box -> radio

| CMD | Meaning | Content as recorded |
|---|---|---|
| `20` | key | `DATA[0]` = key code (table below). `DATA[1]` = `1` pressed, `2` held (repeats), `0` released. Wheel detents send only `1` |
| `21` | climate state | 9 bytes. `DATA[2]`, `DATA[3]` = left / right temperature in 0.5 °C steps (`2D` = 22.5 °C; CAN `350` value + `23`). `DATA[1]` bits 3-0 = fan level. Mode bits in `DATA[0]`, `DATA[1]`, `DATA[8]` change only with actions on the car panel. Example: `C4 E0 2D 2D 00 81 40 00 E0`. 26 of 27 frames match CAN `350` + `2AD` |
| `29` | steering angle | `DATA[0]` = CAN `10B` byte 2, `DATA[1]` = byte 1 (155 of 155 frames). Scale: unverified |
| `32` | parking sensors | `DATA[0]` = state (`01`, `02`; `00` with `DATA[1..6]` = `FF` when inactive; `03` + `FF` seen too). `DATA[1..3]` rear left / centre / right, `DATA[4..6]` front left / centre / right; `05` far, `00` nearest. About once a second plus on change |
| `33` | trip computer, current page | `DATA[0..5]` = CAN `221` bytes 2-7; `DATA[6..8]` = `00` in all frames |
| `34` | trip 1 | from CAN `2A1`: bytes 4, 5, `00`, 1, 2, 3 |
| `35` | trip 2 | from CAN `261`, same layout as `34` |
| `36` | outside temperature | bit 7 = minus, bits 6-0 = °C. From CAN `0F6` byte 7 (value / 2 - 40, halves rounded up; 21 of 21 frames) |
| `38` | doors and vehicle settings | 6 bytes, see [Peugeot 508 SW](vehicles/peugeot-508.md#xp-frame-38). Sent on change and after each radio `81 01` |
| `3C` | which setting rows to show | 2 bytes; one value recorded: `9E 80` |
| `3B` | stored speeds (limiter, cruise control) | `FC 28 3B 4F 63 82 01` = 40, 59, 79, 99, 130 km/h; the same values are in CAN `1E8` |
| `3D` | speed list | `32 46 50 5A 6E 82` = 50 ... 130 km/h, sent twice; same as CAN `2A7` / `267` |
| `3E` | obstacle | `80` while the sensors are active and a zone is below 7, else `00` (19 of 19 frames) |
| `44` | economy mode | `01` with ignition on and engine off, `00` with the engine running; from CAN `036` byte 3 bit 7 |
| `71` | protocol version | `0A 05 16 10 20 56 02 19 06`; the radio shows it as "XP_PSA_2016_10_20 V2256" |
| `30` | box version text | ASCII |
| `EA` | power | `01 0A` within 30 ms after CAN `0F6` byte 1 leaves `80` (ignition on); `00 0A` 2.5 s after it returns to `80` |
| `01`, `37`, `39` | not decoded | `01`: `00 00 00`, `00 00 10` after engine start. `37`, `39`: change with some car messages in CAN `1A1` |

The parking zones of CAN `0E1` (0 nearest ... 7 far) become radio values like this: 0->0, 1->1,
2->1, 3->2, 4->2, 5->3, 6->3, 7->5 (394 of 396 frames match).

After the ignition is switched on, the box sends this group four times, about 1 s apart:
`38`, `EA 01 0A`, `71`, `01`, `3B`, `3C`, `30`, `3E 00`, `36`, `33`, `34`, `35`, `21`, `3D`,
`44 01`.

### Key codes

Command `20`, `DATA[0]`. All rows measured (36 of 36 presses matched the box frames).

| Code | Button | CAN source |
|---|---|---|
| `14` | wheel: volume + | `21F` byte 1 = `08` |
| `15` | wheel: volume - | `21F` byte 1 = `04` |
| `16` | wheel: both volume buttons (mute); also the panel power knob pressed | `21F` byte 1 = `0C`; `036` byte 7 = `02` |
| `17` / `18` | wheel: next / previous | `21F` byte 2 counter down / up |
| `21` | wheel: SRC | `0A2` byte 2 = `04` |
| `50` | wheel: LIST | `21F` byte 1 = `01` |
| `07` | wheel: scroll wheel pressed | `21F` byte 3 = `80` |
| `30` | wheel: phone (held: repeats `30 02`) | `21F` byte 1 = `02` |
| `23` | wiper stalk: trip computer | `221` byte 1 bit 0 |
| `22` | a panel button, not identified | `122` byte 1 = `02` |
| `81` / `82` | panel volume knob right / left | `122` byte 7 counter up / down |
| `9E` | panel: NAV | `122` byte 2 = `80` |
| `A0` | panel: MEDIA | `122` byte 2 = `20` |
| `97` | panel: seek left | `122` byte 2 = `01` |
| `98` | panel: seek right | `122` byte 3 = `80` |
| `9C` | panel: zoom + | `122` byte 3 = `40` |
| `9D` | panel: zoom - | `122` byte 3 = `20` |
| `9F` | panel: TEL | `122` byte 2 = `08` |
| `99` | panel: SETUP | `122` byte 2 = `02` |
| `9B` | panel: DRIVE | `122` byte 2 = `10` |
| `A4` | panel: screen off (moon) | `122` byte 2 = `40` |
| `A1` | panel: microphone mute | `122` byte 2 = `04` |
| `A3` | panel: WEB | `122` byte 1 = `01` |

The box did **not** pass the left thumb wheel (`0A2` byte 4, counter; press in byte 5) or its press to the radio
(measured). The box sent the wheel detents about 220 ms after the CAN frame (measured). Another
box (Raise) showed no such delay (the author's observation, not recorded).

> [!NOTE]
> **Defaults of this firmware differ a little**
>
> In decoder mode the firmware sends the measured codes above, with a few remaps that are a
> choice of this project for the reference car (for example `21` -> `1F`, `9F` -> `9B`, set in its car
> profile). You can change the key table
> from the console. See [Console](console.md).

## Radio -> box

| CMD | Meaning | Content as recorded | Effect on CAN (box) |
|---|---|---|---|
| `A6` | date and time | year - 2000, month, day, hour, minute; every second | |
| `81` | radio state | `01` on, `00` switching off; 4 times about 1 s apart | the box answers each `81 01` with `38` + `3C` |
| `90` | request a frame | `90 71 00` with each `81` (version); `90 21 ...` on entering the climate screen; `90 33 00 00 00` on entering the trip computer screen | |
| `C4` | volume | 0-36, `80` = mute | `1A5` = level, 4-10 ms later; mute: `165` byte 1 = `E0` |
| `C0` | source / playback | 8 bytes: `01 01 ...` radio, `0C 30 ...` others, `08 13 ...` media player with track data | `165` byte 3: `10` radio, `60` player, `40` others |
| `CA` | profile variant | `00`, at start, 4 times | |
| `CB` | text for the car display | type (`02`, `04`) + 32 bytes, empty in the recordings | |
| `80` | vehicle setting | number, value | one `15B` about 6 ms later, see [Peugeot 508 SW](vehicles/peugeot-508.md#personalisation-with-15b-and-260) |
| `82` | trip computer command | `82 00 00 00 40 00 00` = "clear" button on the trip screen; `82 00 00 00 00 00 00` on entering the screen | |
| `8A` | climate screen command | `8A nn vv`, one frame per button press: `01` AUTO, `02` A/C, `03` MAX A/C, `04` / `05` left / right temperature (`01` plus, `02` minus), `06` face, `07` windscreen, `08` feet, `09` fan, `0A` fan speed (`01` plus, `02` minus), `0B` MONO, `0C` temperature unit (from the settings screen); recorded with owner markers 2026-11-19 | none: the car did not react, the firmware sends nothing |
| `84` | sound screen command | `84 nn vv`: `06` treble, `05` bass, `03` fader, `04` balance, `09` sound selection, `02` surround, `0A` volume / loudness; value `00` / `01`, meaning not determined; recorded with owner markers 2026-11-19 | none |

The radio sends the **target value** of a setting, computed from its own picture of the car
(from frame `38`). If `38` is wrong, the radio keeps sending the same value.

## Commands the firmware only names

The radio commands `84` (rows of the radio sound screen) and `8A` (buttons of the radio climate
screen) were recorded on 2026-11-19 with an owner marker before each press
(`firmware/test_data/captures/centralka_konsola_20261119_150048.txt`; the numbers and names are
in `radios/xp-simple-soft/provenance.md`). The firmware prints them with their names and sends
nothing to the car. `C2` (tuner) is recognised and ignored. Frame `C5` is not recognised: it was
seen only with a length byte that does not match its data, and its meaning was never measured.

## Not known

- Which panel button sends code `22`.
- The tuner and phone commands in detail, and the value byte of `84`.
- The rule for the remaining bits of `38`, and the meaning of `01`, `37`, `39`.
- Which CAN frames disappear when the box is removed: one short recording without a box
  (`firmware/test_data/captures/centralka_can_20261117_184411.log`, about 10 s of frames) has none
  of the frames sent by the box, but it does not repeat the same steps.
