# Inside an aftermarket CAN box

An Android radio talks to the car through a small "CAN box": it listens to the car bus and sends the
radio a simpler serial protocol. Two boxes were opened during this project: a **Simple Soft
RP5-PA-001** and a **Raise RZ-PSA15**. This page describes only what was seen on the boards and
measured with a multimeter or a CAN recording. It does not describe their firmware.

The project is not affiliated with Simple Soft or Raise. Photos of the boards were taken by the
author; they are not published in this documentation.

## Why open a box at all

The Simple Soft box was the main suspect of two faults on the reference car (see
[The stuck parking tone](parking-tone-bug.md)). To make sense of the faults, and later to replace
the box, the author needed to know three things: what kind of CAN transceiver it has, whether it
terminates the bus, and which pin of its connector does what.

## How the boards were read

- **Video, frame by frame.** The boards were filmed in macro, and part markings were read from
  still frames. Some markings were confirmed with a magnifier.
- **Multimeter.** Continuity from connector pins to parts, resistance between the CAN lines with
  the box unpowered.
- **Recordings.** A listen-only sniffer on the bus with the box and the radio working.

A marking read from the board is a fact about the board. What that part *is* comes from the
maker's datasheet for that marking. Where a marking was not found in any datasheet, the table says
so.

## Simple Soft RP5-PA-001

### Parts

| Board position | Marking read | What it is | Status |
|---|---|---|---|
| U1 | ARTERY AT32F415CBT7, LQFP48 | microcontroller (Cortex-M4) with one CAN controller | read; type from the maker's datasheet |
| B1 | ZGC8.000 | 8 MHz crystal | read |
| near U10 | CSC1040, SO-8 | **high-speed** CAN transceiver, pinout like the TJA1040 (pin 8 = standby) | read; type from the maker's datasheet |
| U10 | BY25D40ES family, SO-8 | 4 Mbit SPI flash memory | read; type from the maker's datasheet |
| U15 | LM4890S | 1 W audio amplifier, probably for the external speaker of the kit | read; use is a conclusion |
| — | 78D05 (TO-252) | 5 V regulator | read |
| U7, U9 | 6118A, 6203A | regulators; output voltages **not measured** | read; type unverified |
| U3, U4, U5, U12 | 3407 (SOT-23) | P-channel MOSFETs | read; type unverified |
| F1 | JK 110 | polymer fuse | read |
| D10 | SS14 | Schottky diode | read |
| B2 | empty pad marked "+" | place for a buzzer, **not fitted** | read |

The 5 V to 3.3 V supply works (measured: multimeter).

### Measurements

| What | Result | Status |
|---|---|---|
| Resistance CANH-CANL at the transceiver, box unpowered | 48 kΩ: **no termination resistor** | measured |
| Pads next to the CAN lines | two pairs of empty pads: a termination that was not fitted | read from photos; agrees with 48 kΩ |
| Transceiver standby pin (pin 8) | connected to pin 2 of the microcontroller | measured: continuity |
| Series element on each CAN line between transceiver and connector | about 9 Ω, part not identified | measured |
| 2x8 connector, CAN | top row position 2 = CANH, bottom row position 2 = CANL | measured: continuity from transceiver pins 7 and 6 |
| 2x8 connector, power | bottom row position 1 = +12 V (through F1), top row position 1 = GND | measured: continuity |
| 5-hole header J3 | GND, two microcontroller pins, VBAT, supply: **not a debug (SWD) header** | measured: continuity |
| 6-hole header J2 | GND, microcontroller pins, VDD, one wire to the connector | measured: continuity |

"Top row" and "bottom row" mean: looking into the socket from the plug side, board at the bottom;
the top row is further from the board.

### Read from the photos, not measured

- Three P-MOSFETs (U3, U4, U5), each driven by a small transistor: most likely three switched 12 V
  outputs. The fourth P-MOSFET (U12) sits at the power input after the fuse and the diode.
- Two identical paths (560 Ω, a capacitor, 1 kΩ) near the connector: most likely the two serial
  data lines to the radio. Which connector pins they reach was not confirmed.

### What the box does on the bus

A sniffer at the radio showed that this box does not only listen. It sent frames as if it were the
factory radio **and** the telematics unit: radio status, volume, audio settings, vehicle setting
requests, screen buttons, climate buttons (measured: recordings with the box connected; details on
the vehicle page `vehicles/peugeot-508-sw-2015/README.md`). When the car went to sleep the box went
silent with it and sent nothing during parking (measured: recording during parking).

The serial line to the radio: 38400 baud, 8N1, 3.3 V levels (measured: recorder on both wires).

### One modification and its result

To test the idea that the standby pin caused the faults, pin 8 of the transceiver was cut from its
pad and tied to ground, so the transceiver always stays in normal mode. The tone still got stuck,
only for a shorter time; the remote still failed. See
[attempt 2 of the parking tone story](parking-tone-bug.md#attempt-2-the-standby-pin).

## Raise RZ-PSA15

This box was read from one video only, then measured through its harness. It was not opened
further.

### Parts (read from the video)

| What | Marking read | Status |
|---|---|---|
| Microcontroller, LQFP48 | custom marking ("Pocket Link N12"), type **not known** | read |
| Crystal | 24.000 MHz | read |
| CAN transceiver, SO-8 | P1040, the same 1040 family as in the Simple Soft box | read |
| Termination | no 120 Ω resistor visible near the transceiver | read |
| Buzzer | **fitted** on the board | read |
| 12 V outputs | three identical switch stages and a fourth one | read |
| Near the connector | two 560 Ω resistors and diodes (probably the serial lines), a 47 k / 18 k divider (probably 12 V sensing) | read; use is a conclusion |
| Connector | 2x8, the same type as on the Simple Soft box | read |

The connector pads are covered by silkscreen and the housing, so the pinout could not be read
from the board.

### Measurements (unpowered, through its harness at the car's Quadlock plug)

| What | Result | Status |
|---|---|---|
| CANH-CANL (Quadlock pins 10 and 13) | 59 kΩ: **no termination** | measured |
| +12 V to ground (pins 12 and 16) | 2.63 MΩ: no short | measured |
| Harness ground (pin 16) to board ground | continuity | measured |

### In the car

The Raise box was plugged into the Simple Soft harness; the positions matched. With the radio
profile set to Raise (measured: in the car, by the author):

- the radio started; steering wheel buttons and volume worked, with no delay on the volume
  buttons (the Simple Soft box sent volume steps about 220 ms late);
- parking sensors showed on the screen and the box buzzer beeped, together with the car's own beeps;
- the buzzer has only about three steps, and the continuous tone starts earlier than in the
  factory setup;
- climate: no control and no temperature on the screen;
- the buttons above the climate panel were mixed up;
- the remote key worked after every parking, overnight included.

## What this meant for the device

- Neither box terminates the bus, so the device must not either. It connects in parallel to the
  existing bus without a 120 Ω resistor.
- The Simple Soft box uses a high-speed CAN transceiver (its part number and pinout were read
  from the chip). The Raise transceiver is marked `P1040`, which looks like the same 1040 family,
  so it is probably high-speed too (unverified: marking only). The bus voltages look like low-speed
  fault-tolerant CAN. Both boxes work; so does the high-speed transceiver of this device. Why it works
  is not proven: an oscilloscope measurement is still missing.
- The 2x8 connector pinout measured on the Simple Soft box (GND, +12 V, CANH, CANL) is what the
  prototype uses to plug into the existing harness, through a socket taken from that box.
- The Simple Soft box acts as the factory radio and the telematics unit on the bus. A device that
  replaces it and does **not** transmit leaves those frames missing. What the car does without
  them is only partly known.
