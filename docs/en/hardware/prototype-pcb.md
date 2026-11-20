# Prototype PCB

Prototype v1 is a double-sided board made for home etching with toner transfer. One board was
built and is used in the reference car. This page tells you how to make it, how to power it the
first time and what is different on the board that exists.

All files are in `hardware/prototype-v1/`. Read `hardware/prototype-v1/STATUS.md` there as well.

## Files

| File | Content |
|---|---|
| `kicad/proto/centralka_proto.kicad_pcb` | PCB with tracks and ground fill (KiCad 10) |
| `print/1_bottom_copper_toner_transfer.pdf` | bottom copper, **not mirrored** |
| `print/2_top_copper_mirrored_toner_transfer.pdf` | top copper, **mirrored** |
| `print/3_assembly_top_view.pdf`, `print/4_assembly_bottom_view.pdf` | part placement at 2:1, only to look at |
| `images/assembly_map_bottom_view.png` | what to solder where, polarity, seen from the solder side |
| `images/drill_map_bottom_view.png` | holes coloured by drill size, seen from the solder side |
| `images/esp32_socket_test_map_bottom_view.png` | multimeter checks of the empty ESP32 sockets |
| `images/ground_wire_vias.png` | where the ground wire vias go |
| `images/preview_*.png`, `images/render_*.png` | previews and 3D renders |

The legends inside the images are in Polish. `hardware/prototype-v1/README.md` translates them.

There are no Gerber or drill files and no STEP model. The board was never made by a PCB factory.

## The board in numbers

- 100 x 77 mm, two layers.
- Tracks 0.8 mm, clearance 0.4 mm (the smallest gap appears in about 17 places).
- 7 wire vias: 5 join the bottom ground with the top ground, 2 are the ends of one 5 V jumper on
  the top side.
- KiCad design rules check (DRC): 0 copper errors, 0 unconnected items. Only silkscreen and
  courtyard overlap warnings remain.

## Making the board

1. Print sheets 1 and 2 at 100 % scale. Turn off "fit to page". The frame on the sheet lets you
   check the scale with a ruler.
2. Check the mirroring: the bottom is **not** mirrored, the top **is** mirrored, because the toner
   always faces the copper. The texts "SPOD v1" (bottom) and "GORA v1" (top) must be mirrored on
   the paper and readable on the copper.
3. Drill the three M3 holes first (top left next to J5, top right corner, bottom right corner) and
   align both sides on them. The top ground keeps 1 mm from every hole, so an offset up to about
   1 mm does not short a socket pin.
4. Transfer the toner (iron or laminator) and etch as you usually do; this page does not cover
   the method.
5. After etching, look at the board with a magnifier.

### Drilling

Hole centres are small dots on the copper.

| Drill | Count | For |
|---|---|---|
| 0.8 mm | 13 | 7 wire vias, 3 Zener diodes |
| 1.0 mm | 63 | sockets, TRACO module, JP1 |
| 1.2 mm | 5 | J4 wire pads |
| 1.3 mm | 4 | J5 terminals |
| 3.2 mm | 3 | M3 holes |

Without a 0.8 mm drill use 1.0 mm (the copper ring left is about 0.65 mm).

> [!WARNING]
> **Do not drill the sockets larger than 1.0 mm**
>
> Along a socket row the copper ring is only 0.52 mm. A bigger drill removes it.

### Soldering

1. Solder in this order:
    1. SMD parts on the bottom.
    2. The 7 wire vias: wire through the hole, solder on both sides. Two of them are 1.6 mm from
       the ESP32 header: do them now, before the sockets.
    3. The TRACO module.
    4. The terminals and the Zener diodes.
    5. The sockets last.

    JP1 stays empty.
2. Solder through-hole pads **on the bottom only**. The rings around the holes on the top side are
   dead copper.
3. Do not skip the 5 ground wire vias. They are the only link between the top and bottom ground.
   Without them parts of the bottom ground float (for example the ground of the converter, of the
   CAN module and of the output transistors).
4. The ground pins of the module sockets join the fill with solid copper (no thermal relief): hold
   the iron a little longer. The socket pads are 0.5 mm apart: check neighbouring pins for shorts.
5. C1 and C4 are 10 mm tall and sit on the bottom: the board needs standoffs of at least 12 mm.
   The [enclosure](enclosure.md) has them.

## First power-up

> [!WARNING]
> **Use a current-limited supply**
>
> Power the bare board from a bench supply limited to about 100 mA. If it hits the limit, switch
> off at once. Switch the supply off every time before you insert a module.

How to find the pins: in `images/esp32_socket_test_map_bottom_view.png` (seen from the solder
side) M1 is the inner 1x22 socket, next to the CAN module socket M3, and M2 is the outer one at the
board edge. Pin 1 of both is at the J5 end of the board, pin 22 at the far end.

1. No modules yet. With a multimeter check: no short +12V-GND, +5V-GND, +3V3-GND (pads near J5, C4
   and C6).
2. Check the empty ESP32 sockets with a continuity tester, no power, as in
   `images/esp32_socket_test_map_bottom_view.png`: neighbouring pins do not beep (except M1 pins
   1-2 and M2 pins 21-22); ground pins beep to GND of J5. M1 pins 7 and 11 have 100k / 10k to
   ground, so silence is right there (an ohmmeter shows about 85k / 10k).
3. Connect 12 V to J5 (terminal 4 = +12V, terminal 3 = GND). Meter minus on GND of J5. Expect
   4.6-4.8 V on M1 pin 21 (5 V after diode D3), 0 V on M1 pins 1 and 2 (3.3 V comes from the ESP32
   board later), about 1.9-2.0 V on M1 pin 7 (battery divider), and a few mA of current.
4. Switch the supply off. Insert the ESP32 dev board (USB on the pin 22 side). Switch on again,
   with the same 100 mA limit. Now M1 pins 1, 2 and the VCC pin of the CAN socket show 3.3 V.
   Current: tens of mA. If the supply hits the limit, switch off at once.
5. Switch the supply off. Insert the CAN module (terminator switch OFF) and the amplifier. Then
   flash the firmware ([Flash the firmware](../quick-start/flash.md)) and run the
   [bench test](../quick-start/bench-test.md).

## What is on the prototype and what is not

On the prototype PCB: 12 V input with fuse, reverse-polarity diode and DC/DC converter; ESP32-S3
dev board; CAN Pal module; MAX98357A amplifier; battery voltage measurement; UART to the radio;
12 V outputs ACC, ILL and REV.

Not on the prototype PCB (only in the [schematic](schematic.md)): expansion connector, I2C,
emergency button, fourth output, spare 3.3 V regulator, selection jumpers `JP_S`, `JP_UART`,
`JP_3V3`.

Fixed instead of selectable:

- **SLNT** of the CAN module is pulled up to 3.3 V by R3 (10k). There is no `JP_S`.
- **3.3 V** for the CAN module comes from the ESP32 dev board. There is no `JP_3V3` and no spare
  regulator.
- **UART** is wired for the direction measured on the reference car. There are no `JP_UART` swap
  jumpers: to swap, swap the two wires on J4.
- **CAN lines** go straight from J5 to the module. The 0 ohm resistors for ferrite beads and the
  optional line protection D4 of the schematic are not on the PCB.
- **Amplifier gain**: the GAIN input is left open (module default).
- **Speaker**: screwed to the terminal of the amplifier module. The amplifier output is bridged:
  neither speaker wire may touch ground.

## Transmit path

Read this before you build or modify the board.

As drawn, the prototype PCB listens only, in hardware:

- SLNT of the CAN module is pulled up by R3, so the TJA1051 stays in silent mode: its transmitter is
  off, its receiver works normally.
- JP1 (the TX line from GPIO5 to the module) is a 2-pin position that you **leave open**. With JP1
  open the module's TX input floats and stays recessive.
- GPIO6, which the firmware uses to drive SLNT, is **not connected** to the module.

JP1 alone does not enable transmission: SLNT is still held high by R3. Transmitting needs both
JP1 closed **and** a wire from SLNT (M3 pin 3) to GPIO6 (M1 pin 6).

The author's board was modified that way for transmit experiments: JP1 closed, a wire from SLNT to
GPIO6, R3 kept as the pull-up during start-up.

> [!CAUTION]
> **No hardware lock on a modified board**
>
> With JP1 closed and the SLNT wire fitted, only the firmware decides whether the device
> transmits (TWAI listen-only mode and GPIO6 high = listen only). Do not make this modification
> unless you build the separate transmit firmware (`decoder_tx_wifi`) on purpose and accept the
> risk. Read [Transmitting on the CAN bus](../safety/transmit.md).

In every environment except `decoder_tx_wifi`, and in `decoder_tx_wifi` as long as it is not armed,
GPIO6 stays high and the CAN controller is in listen-only mode. The transmit firmware pulls GPIO6
low only for the duration of one frame.

On the modified board in the car, frames sent by the transmit firmware were acknowledged on the
bus (measured: device console). What the firmware is allowed to send is described on the
transmit page.

> [!NOTE]
> **Unexplained: no reception with JP1 closed**
>
> Once, in the car with JP1 closed, the board received no CAN frames; with JP1 open reception came
> back. A continuity check found no short between the TX and RX tracks. The cause is not known.

## Verified on the built board

| Check | Status |
|---|---|
| all resistor and capacitor values, continuous ground without shorts to tracks, no short between supply rails, TRACO pins, every pin of the ESP32 sockets, RX-TX | measured: multimeter, before inserting the modules |
| start from USB only (ESP32 + CAN module + amplifier, no 12 V): firmware reports `can=ok audio=ok radio=ok`, the `test` console command plays a tone from the speaker | measured: device console |
| CAN reception, UART to the radio, 12 V supply in the car | measured in the car, see [Peugeot 508 SW](../reference/vehicles/peugeot-508.md) |
| 12 V outputs ACC / ILL / REV | **not measured** on the bench; no separate report |

## Differences of the built board

- Several parts came from a dismantled aftermarket CAN box: the P-MOSFETs (marking `3407`), the
  output diodes, the input fuse. D1 is a 1N4001 from the same box (marking `M1`), not an SS34.
- At first D2 (SMBJ18A) and the Zener diodes D5, D7, D9 were missing, and C1 was a 220 uF / 25 V
  part meant for the bench only.
- The ground vias were drilled where the author chose, because their marks did not survive the
  etching.
- The battery reading is the voltage **after** diode D1, so it shows about 0.8 V (0.7-0.9 V) less than the
  battery (note in `firmware/boards/board_proto_v1.h`). Powered from USB only, without 12 V, the
  reading is noise of about 0-1.5 V; below a threshold the console and the panel treat it as
  "no 12 V".

> [!WARNING]
> **Before the car**
>
> Go through the [Installation checklist](../install-in-car/checklist.md): D2 and C1, JP1 open,
> no terminator, and the checks without power are all there.

## If a part is missing

- No 12 V Zener (D5, D7, D9): the board works without them; they only protect the MOSFET gates.
- No polymer fuses: a wire bridge in place of F1-F4 **on the bench only**, with a fuse in the +12 V
  lead of the supply.
- No SMBJ18A (D2): you may leave it out on the bench. Fit it before the car.

Next: [Enclosure](enclosure.md).
