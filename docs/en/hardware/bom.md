# Bill of materials

This is the parts list of **prototype v1**, taken from the prototype PCB. The machine-readable copy
is `hardware/prototype-v1/bom.csv`. Reference designators (R1, Q2, ...) match the PCB and the
assembly maps.

The list names typical parts and alternatives so you can use what you can get. It does not
recommend shops. Part numbers marked "not tested" were not tried on the board.

> [!WARNING]
> **Some parts are mandatory before the car**
>
> D2 (surge clamp) and a C1 rated 35 V or more must be fitted before you connect the board to a
> car. On the bench you may leave D2 out. All checks before the car:
> [Installation checklist](../install-in-car/checklist.md).

## Modules

| Ref. | Qty | Part | Notes |
|---|---|---|---|
| M1 + M2 (the two sockets of one dev board) | 1 | ESP32-S3 dev board with ESP32-S3-WROOM-1-N16R8, row spacing 22.86 mm (the author used a Waveshare ESP32-S3-DEV-KIT-N16R8) | plugged into two 1x22 female headers. The dev board's own 3.3 V regulator also supplies the CAN module |
| M3 | 1 | Adafruit CAN Pal (TJA1051T/3) | 1x7 socket. Pin order from the bottom: 1 CANL, 2 CANH, 3 SLNT, 4 TX, 5 RX, 6 GND, 7 VCC (3.3 V). **Terminator switch OFF** |
| M4 | 1 | MAX98357A I2S amplifier module | 1x7 socket. Pin order from the top: 1 LRC, 2 BCLK, 3 DIN, 4 GAIN, 5 SD, 6 GND, 7 VIN (5 V). Speaker 4-8 ohm, 2-3 W on the module terminal |
| U1 | 1 | Traco TSR 1-2450 (input 6.5-36 V, output 5 V / 1 A) | any 5 V / 1 A switching module with the 7805 pinout should fit (not tested) |

Check the pin order printed on **your** module before you etch the board. Clones can differ.

## Power and protection

| Ref. | Qty | Value | Typical part | Function | Notes |
|---|---|---|---|---|---|
| F1 | 1 | PTC 1.1 A / 30 V | polymer resettable fuse, 1812 | input fuse | rated 30 V or more |
| F2, F3, F4 | 3 | PTC 0.5 A / 30 V | polymer resettable fuse, 1812 | fuses of the 12 V outputs | rated 30 V or more |
| D1, D3 | 2 | SS34 | SS34 (alternatives SS36, B340A, SS24) | D1 reverse polarity; D3 separates the converter from USB 5 V | |
| D2 | 1 | SMBJ18A | SMBJ18A (alternatives SMAJ18A, P6SMB18A) | input surge clamp, after F1 | **mandatory before the car**; cathode band to +12 V |
| C1 | 1 | 100 uF / 35 V | electrolytic, low ESR, 105 °C, 8x10 mm | input reservoir | 35 V or more in the car; 10 mm tall, the board needs 12 mm standoffs |
| C2 | 1 | 100 nF / 50 V | ceramic X7R, 0805 | input decoupling | 25 V or more |
| C4 | 1 | 470 uF / 16 V | electrolytic, 8x10 mm | 5 V reservoir (amplifier peaks and Wi-Fi) | |
| C3, C5, C6, C10 | 4 | 10 uF | tantalum 10 uF 10 V, 3528 (or ceramic) | decoupling 5 V / 3.3 V | |
| C7, C8 | 2 | 100 nF | ceramic, 0805 | decoupling / ADC filter | |

## 12 V outputs (ACC, ILL, REV)

| Ref. | Qty | Value | Typical part | Function | Notes |
|---|---|---|---|---|---|
| Q2, Q4, Q6 | 3 | AO3407 | AO3407A (-30 V, gate +/-20 V), SOT-23 | high-side switches (P-MOSFET) | **not AO3401A**: its gate takes only +/-12 V |
| Q1, Q3, Q5 | 3 | BC847A | BC847, BC817, MMBT3904, SOT-23 | output drivers (NPN) | |
| D5, D7, D9 | 3 | Zener 12 V | 12 V 0.5 W through-hole (BZX85C12 in DO-41 fits after drilling 1.0 mm) | gate protection of Q2 / Q4 / Q6 | optional on the bench, recommended in the car; cathode to +12 V |
| D6, D8, D10 | 3 | SS14 | SS14, SS24, 1N4148W | output clamp diodes | |
| R6, R10, R14, R18 | 4 | 10k | 0805 | gate-source resistors / pull-ups | |
| R7, R9, R11, R13, R15, R17 | 6 | 4k7 | 0805 | base and collector resistors | |
| R8, R12, R16 | 3 | 100k | 0805 | base pull-downs: outputs off after reset | |

## CAN, UART and measurement

| Ref. | Qty | Value | Function | Notes |
|---|---|---|---|---|
| R3 | 1 | 10k, 0805 | SLNT pull-up to 3.3 V | keeps the CAN module silent after reset |
| R1 | 1 | 470k, 0805 | battery divider, top | 470k / 100k: 16 V gives 2.8 V |
| R2 | 1 | 100k, 0805 | battery divider, bottom | |
| R23 | 1 | 1k, 0805 | UART line to the radio (device transmits), by design | on the prototype: which resistor sits on which line not checked |
| R24 | 1 | 4k7, 0805 | UART line from the radio (device receives) | safe with a 5 V level |
| JP1 | 1 | 2-pin header or wire bridge | TX line to the CAN module | **leave open** for a listen-only build ([Prototype PCB](prototype-pcb.md#transmit-path)) |

## Connectors and mechanics

| Ref. | Qty | Part | Notes |
|---|---|---|---|
| J5 | 1 | 4-way screw terminal 5.08 mm (two 2-way blocks side by side) | from the top: CANH, CANL, GND, +12V |
| J4 | - | 5 wire pads, 1.2 mm holes, no part | from the top: radio RX, radio TX, ACC, ILL, REV |
| sockets | 2 + 2 | female headers 2.54 mm: 2x 1x22 (ESP32), 2x 1x7 (modules) | can be cut from 1x40 strips |
| H1, H2, H3 | 3 | M3 holes | M3 x 6-8 mm screws |
| wire | 7 pieces | e.g. cut component legs | the 7 wire vias |

Not in the list: the 2x8 harness socket. It is not on the board; the author wired a socket
desoldered from an old CAN box to J5 and J4 with short leads ([Finding and checking wires](wiring.md)).

## Where the author's board differs

The board in the car does not match this list exactly:

- several parts came from a dismantled aftermarket CAN box: the P-MOSFETs, the output diodes and
  the input fuse;
- D2 and the Zener diodes D5, D7, D9 were not fitted at first.

Fit D2 and use C1 rated 35 V or more before the car, whatever you take the parts from.

Next: [Schematic](schematic.md).
