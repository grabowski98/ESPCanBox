# Hardware overview

The device is an ESP32-S3 board that sits between the car and an Android radio, in the place of
the aftermarket CAN box. This section describes the only hardware that exists so far: **prototype
v1**, a home-etched carrier board for ready-made modules. A board for a PCB factory is planned,
not designed (see [Future boards](future-boards.md)).

> [!WARNING]
> **Proof of concept, not a product**
>
> One prototype board was etched at home, assembled and used in the reference car (Peugeot 508
> SW). It is published so you can see a working layout and copy it at your own risk. There are
> no Gerber files and no production files. If you build it, check every step with a multimeter.

## What is on the board

The prototype is a carrier: the difficult parts are modules you plug into sockets.

| Block | Part on the prototype | What it does |
|---|---|---|
| Processor | ESP32-S3 dev board with ESP32-S3-WROOM-1-N16R8 (the author used a Waveshare ESP32-S3-DEV-KIT-N16R8), on two 1x22 sockets | runs the firmware, USB, Wi-Fi |
| CAN | Adafruit CAN Pal module (TJA1051T/3 transceiver), 1x7 socket | turns the CAN wire pair into logic signals for the ESP32 |
| Sound | MAX98357A I2S amplifier module, 1x7 socket | parking beeps from a small speaker |
| Power | polymer fuse, reverse-polarity diode, surge clamp (TVS), Traco TSR 1-2450 DC/DC module (12 V to 5 V) | runs the board from the permanent 12 V of the car |
| Battery voltage | 470k / 100k divider to an ADC pin | shows the supply voltage in the console and the panel |
| UART to the radio | two series resistors | the serial link to the radio (decoder mode) |
| 12 V outputs | three P-MOSFET high-side switches: ACC, illumination (ILL), reverse (REV) | switch the radio's ACC, ILL and REVERSE inputs (decoder mode) |

The 3.3 V for the CAN module comes from the regulator on the ESP32 dev board. There is no
separate 3.3 V regulator on the prototype PCB.

Board size 100 x 77 mm, two copper layers, 0.8 mm tracks and 0.4 mm clearance, made for toner
transfer. Details: [Prototype PCB](prototype-pcb.md).

## What is only in the schematic

The schematic describes a **fuller** board than the one that was etched. These parts are drawn but
are not on the prototype PCB:

- expansion connector and I2C connector;
- emergency button;
- fourth 12 V output;
- spare 3.3 V regulator;
- selection jumpers `JP_S` (SLNT source), `JP_UART` (swap radio TX/RX), `JP_3V3` (3.3 V source).

See [Schematic](schematic.md).

## Listen-only in hardware

Build the board as drawn: JP1 empty, no wire to GPIO6. Then the CAN module cannot transmit: its
SLNT input (silent mode) is pulled high by a 10 kOhm resistor (R3), and the transmit line goes
through the open jumper JP1. The author's board was later changed for transmit tests (JP1 closed,
a wire from SLNT to GPIO6); you do not need that change. On such a board only the firmware decides
whether the device transmits.

> [!CAUTION]
> **A modified board has no hardware transmit lock**
>
> With JP1 closed and the SLNT wire to GPIO6, nothing in hardware stops transmission. Leave JP1
> open and do not fit the SLNT wire unless you build the transmit firmware on purpose. Read
> [Transmitting on the CAN bus](../safety/transmit.md) first.

The firmware listens only by default: in every environment except `decoder_tx_wifi` the TWAI
controller runs in listen-only mode and GPIO6 stays high. More in
[Prototype PCB](prototype-pcb.md#transmit-path).

## Pins

The pin numbers of the prototype live in one file, `firmware/boards/board_proto_v1.h`. That file is
the only source: if this page and the file disagree, the file is right.

| Signal | GPIO | On the prototype PCB |
|---|---|---|
| CAN RX (from the module) | 4 | yes |
| CAN TX (to the module) | 5 | through jumper JP1 |
| CAN SLNT (silent mode) | 6 | **no**: only on the author's modified board, by an added wire |
| Battery voltage (ADC) | 7 | yes, 470k / 100k divider |
| I2C SDA / SCL | 8 / 9 | no (schematic only) |
| UART to the radio, device transmits | 10 | yes, pad J4 "radio RX" |
| UART from the radio, device receives | 11 | yes, pad J4 "radio TX" |
| 12 V output ACC | 12 | yes |
| 12 V output ILL | 13 | yes |
| 12 V output REV | 14 | yes |
| I2S BCLK / LRC / DIN | 15 / 16 / 17 | yes |
| Amplifier SD (off when low) | 18 | yes |
| Spare 12 V output | 21 | no (schematic only) |
| Emergency button (to ground) | 3 | no (schematic only) |
| Status LED (RGB on the dev board) | 38 | on the dev board |

GPIO 35-37 (memory of the N16R8 module) and 19/20 (USB) are never used. The build checks this.
How to describe your own board: [Configuration and pins](../reference/config-and-pins.md).

## Pages in this section

- [Bill of materials](bom.md): parts list of the prototype.
- [Schematic](schematic.md): sheets, file format, how to open it.
- [Prototype PCB](prototype-pcb.md): making the board, first power-up, what is bridged or empty.
- [Enclosure](enclosure.md): the 3D-printed case.
- [Finding and checking wires](wiring.md): how the board connects to a harness, and how to measure yours.
- [Future boards](future-boards.md): what is planned.

## Licence

Hardware files (KiCad, PDF, STL, `bom.csv`) are under CERN-OHL-S-2.0 (`LICENSE-hardware` in the
repository).

Next: [Bill of materials](bom.md).
