# Peugeot 508 SW (2015): reference car

The car the project was developed on. Firmware profile: `psa_aee2010`.

| Item | Value | Status |
|---|---|---|
| Model | Peugeot 508 SW, first generation, model year 2015, 1.6 e-HDi, manual gearbox | owner's documents |
| Electrical architecture | PSA AEE2010 | dealer diagnostic tool lists the modules BSI2010, COM2008P, ESP81; frame IDs and periods match PSA-RE for AEE2010 (66 of 70 IDs) |
| Factory radio | replaced by an aftermarket Android radio (FYT platform); factory: navigation radio, no amplifier, 8 speakers | owner |
| Equipment that matters here | front and rear parking sensors, head-up display, Stop and Start with ECO OFF button, blind spot monitoring | owner, dealer tool |
| Aftermarket CAN boxes tried | Simple Soft RP5-PA-001 ("XP" protocol), Raise RZ-PSA15 | owner |

Files:

- [wiring.md](wiring.md): Quadlock pins, CAN box plug, radio plugs, how to measure your own;
- [508.dbc](508.dbc): frames and signals with status comments (open in SavvyCAN or any DBC tool);
- [settings-map.md](settings-map.md): vehicle settings from the radio screen (`260`, `15B`, `1A9`), the
  rows of the radio settings, climate and sound screens, and why `364` does not control the climate.

Recordings used by the firmware tests: `firmware/test_data/captures/` (the VIN frames are
overwritten).

## Status words

- **measured**: seen on this car, in a recording (file named) or in the console of the device;
- **unverified**: from a public description, mostly PSA-RE
  (github.com/prototux/PSA-RE, `buses/AEE2010.full/LS.INFO-DIV`, commit 67f7581e, Apache-2.0),
  not confirmed here. PSA-RE covers a whole family of cars; three of its fields do not fit this
  car (`2E1`, `236` byte 8, `3E1` 4.5 never change here).

## The bus at the radio

| Fact | Status |
|---|---|
| CAN 125 kbit/s, 11-bit IDs | measured |
| about 218 frames/s with ignition on, 70 IDs, 58 periodic | measured (`can_20261110_190421.log`) |
| 66 of the 70 IDs have the name and period given by PSA-RE for INFO-DIV; unknown: `2E3`, `323`, `363` | measured against PSA-RE |
| CANH 0.65 V, CANL 4.5 V average to ground with ignition on: looks like low-speed fault-tolerant CAN | voltages measured; bus type unverified (oscilloscope needed) |
| a high-speed 3.3 V transceiver receives it without errors | measured |
| with the key out: silent. After switch-off: full traffic about 100 s, then frame `036` alone for 14 s, then silence until the car is opened | measured |
| frames `336`, `3B6`, `2B6` carry the VIN as ASCII | measured. **Mask them before sharing a recording** (`tools/sanitize_capture.py`) |

## Signals used by the firmware

All measured on this car unless the row says otherwise. Details and bit positions in
[508.dbc](508.dbc).

| Signal | Frame and field | How it was confirmed |
|---|---|---|
| ignition / engine | `0F6` byte 1: `80` off, `88` on, `8E` running (`90`, `91`, `89`, `8A` cranking; `86`, `84` switching off) | recordings with ignition and engine steps |
| reverse gear | `0F6` byte 8 bit 7 | reverse engaged 3 times in a recording |
| indicators | `0F6` byte 8 bits 1-0 | step in a recording |
| outside temperature | `0F6` byte 7: value / 2 - 40 °C | matches the box frame to the radio in 21 of 21 frames |
| lights on | `036` byte 4 bit 5 | lights on/off steps |
| economy mode (ignition on, engine off) | `036` byte 3 bit 7 | 10 of 10 frames |
| panel power knob pressed | `036` byte 7: `01` idle, `02` pressed | CAN + UART recording |
| parking sensors | `0E1` (state, sound request, beep interval, 6 zones 0-7) | many approaches front and rear; the box frame to the radio matches in 394 of 396 frames |
| sensors inhibited | `126` byte 1 = `18` | 4 presses of the screen button |
| steering wheel buttons | `21F` (volume, phone, LIST, scroll wheel, press), `0A2` (SRC, left thumb wheel) | 36 of 36 presses matched the box frames to the radio |
| centre panel buttons | `122` (NAV, MEDIA, TEL, SETUP, DRIVE, seek, zoom, screen off, mic mute, WEB, volume knob) | each button pressed in a recording; one button (code `22`) not identified |
| trip computer | `221` (current page, stalk button), `2A1` trip 1, `261` trip 2 | copied byte for byte by the box (9-11 of 9-11 frames); meaning of the values unverified |
| steering angle | `10B` bytes 1-2 | copied by the box, 155 of 155 frames; scale unverified |
| climate state | `350` + `2AD` (rear window heating) | 26 of 27 box frames matched; A/C, both temperatures, air distribution, recirculation, windscreen demist and rear window heating again with owner markers on the car's climate panel (`centralka_can_20261119_150048.log`, markers 12-28; details in [provenance.md](../../radios/xp-simple-soft/provenance.md#climate-panel-of-the-car-markers-12-28)) |
| door open message | `1A1` byte 2 = `DE`, byte 4 bits | each door in two recordings (2026-11-10 and `centralka_can_20261119_150048.log`) |
| Stop and Start state | `3E1` byte 4: `40` active, `80` off | ECO OFF button pressed (capture not published) |
| ASR | `217` byte 3 bit 6 = press, `227` byte 1 bit 4 = ASR off | ASR button pressed (capture not published) |
| personalisation | `260`, `15B` | see [settings-map.md](settings-map.md) |

Found by recording that a button sends **nothing** on this bus: two buttons added by the owner
next to the glove box. There is no parking sensor off switch in the car.

## Frames sent by an aftermarket CAN box

The Simple Soft box acted as the factory radio **and** the telematics unit on this bus. Seen in
recordings (frames that disappear without the box: unverified, no recording without the box with
the same steps):

| Frame | What changes | Status |
|---|---|---|
| `165` radio status | source (byte 3 bits 6-4), mute | measured |
| `1A5` volume | level from the radio volume command, 4-10 ms later | measured |
| `1E5` audio settings | single bytes change with balance / bass on the radio | measured; no amplifier in this car to react |
| `15B` personalisation request | after `80 nn vv` from the radio, accepted by the BSI | measured |
| `1A9` screen buttons | sensors off, blind spot | measured |
| `364` climate buttons | pulses after temperature changes on the radio; **car did not react** | measured |
| `2E3`, `363` | text / frequency for the cluster, change with the source | measured; layout unverified |
| `3E5`, `167`, `1A3`, `323` | constant in the recordings | measured |

The device in this project sends only what its transmit gate allows; see the firmware
documentation. The default builds never transmit.

## Known issue: parking tone keeps sounding

With the aftermarket box installed the continuous parking tone sometimes kept sounding after the
engine was switched off with an obstacle at the closest zone, for up to 10-15 minutes, and the
remote did not unlock after 1-2 hours of parking. What was measured:

- at the moment of switch-off the parking module always sends `0E1` **without** a sound request
  (`D8 00 3F FC FC FC C2 00`), also in the trials where the tone stayed. The stuck tone is held
  further in the car (the module under the steering wheel, driven by the BSI on another bus),
  not by a frame on this bus;
- the tone also got stuck once **without** any CAN box connected, after a quick switch-off during
  the continuous tone. It is not only caused by the box;
- a new approach to the closest zone, ended normally, clears a stuck tone;
- the remote problem disappeared with the box removed and did not appear with another box (Raise);
  the cause is not known.

## What is not known

- The bus type (needs an oscilloscope).
- Which frames disappear without an aftermarket box (recording without a box, same steps).
- Single `1A9` frames from the device, without the periodic telematics frames (measured 18.11.2026:
  device console, recording not published). Result: parking sensors off/on accepted both ways (state
  in `227` changed after about 240-290 ms); blind spot off accepted, on not accepted (one try, 1.2 s
  after the off); Start/Stop not accepted (two tries, frame content not from a measurement).
  Not known: why the blind spot "on" failed (to repeat with a longer gap, watching the mirror LEDs)
  and which `1A9` content, if any, switches Start/Stop.
- The visible effect of the accepted `15B` requests.
- Meaning of `2E3`, `323`, `363`, of the trip computer values and the steering angle scale.
- In `350`: byte 1 bit 2 and byte 6 bits 7-4 values other than 3 (recirculation); `2AD` byte 4. The fan
  level (byte 6 bits 3-0) was not confirmed with a panel press (only through the box frames).
