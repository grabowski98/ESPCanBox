# Finding and checking wires

The prototype board has no harness plug of its own. You connect it with wires: four to a screw
terminal, five to solder pads. This page shows what goes where on the board, how the author wired
it to the harness of the reference car, and how to find the right wires in **your** harness.

> [!WARNING]
> **A drawing is only a hint until it beeps**
>
> Pinouts of CAN box plugs differ between makers and harnesses. Do not trust a seller's drawing, a
> forum post or a photo with labels. Check every wire with a continuity tester before you power
> anything.

## Connections on the board

**J5, screw terminals** (from the top, the J5 side of the board):

| Terminal | Signal |
|---|---|
| 1 | CANH |
| 2 | CANL |
| 3 | GND |
| 4 | +12V (permanent battery plus) |

**J4, wire pads** (from the top, the J5 side):

| Pad | Signal | GPIO |
|---|---|---|
| 1 "radio RX" | data **device -> radio** (the radio receives it) | 10 |
| 2 "radio TX" | data **radio -> device** (the radio transmits it) | 11 |
| 3 | ACC output to the radio | 12 |
| 4 | ILL (illumination) output to the radio | 13 |
| 5 | REV (reverse) output to the radio | 14 |

The pad names follow the radio's point of view: "radio RX" is the line the board transmits on.
Pins come from `firmware/boards/board_proto_v1.h`.

> [!NOTE]
> **Swapped data wires do nothing harmful, but nothing works**
>
> With the two data wires swapped, the radio neither receives nor answers (measured on the
> reference car). Swap them and try again.

In the `listen`, `listen_wifi` and `listen_generic_wifi` environments the device sends nothing to
the radio and the 12 V outputs stay off. ACC, ILL, REV and the "radio RX" line matter only in decoder mode. See
[Radio connection](../install-in-car/radio-connection.md).

## Example: the reference car

On the reference car (Peugeot 508 SW) the board replaces the aftermarket CAN box. The author
desoldered the 2x8 socket from an old CAN box and joined it to J5 and J4 with short leads. The
harness plug then fits the socket as before.

Positions are counted on the harness plug (2x8, pitch about 2.0 mm, estimated without a caliper), seen from the wire side with the
latch at the top, 1-8 from the left. The full map, with wire colours and the Quadlock pins, is in
`vehicles/peugeot-508-sw-2015/wiring.md` and on the [Peugeot 508 SW](../reference/vehicles/peugeot-508.md) page.

| Plug position | Signal | Board | Status |
|---|---|---|---|
| upper 1 | ground | J5-3 GND | measured: continuity to the box ground and the Quadlock |
| lower 1 | +12 V (assumed permanent) | J5-4 +12V | measured: continuity to the box fuse and the Quadlock; voltage in the car not measured |
| upper 2 | CANH | J5-1 CANH | measured: continuity to the box transceiver, confirmed by error-free reception |
| lower 2 | CANL | J5-2 CANL | measured: confirmed by reception |
| lower 8 | data device -> radio | J4 "radio RX" | measured with the listen-only recorder |
| upper 7 | data radio -> device | J4 "radio TX" | measured with the recorder: only radio commands on this line |
| lower 4 | reverse -> radio | J4 REV | measured: continuity in one of two harnesses; colour only in the other |
| lower 3 | ACC -> radio | J4 ACC | unverified: wire colour and harness topology only; continuity not recorded |
| upper 3 | illumination -> radio | J4 ILL | measured: continuity in one of two harnesses; colour only in the other |
| upper 4 | "BRAKE" (handbrake) | not connected, insulated | measured: loose lead in the harness |
| upper 6, lower 6 | speaker of the CAN box | not connected (could carry the amplifier output) | wire label only |

If you feed the speaker wires from the amplifier, remember its output is bridged: neither wire may
touch ground.

The socket dimensions were not measured, which is why the prototype has wire pads and no socket
footprint.

> [!TIP]
> **Mark the socket pins before you solder**
>
> Seen from the pin side, the socket is mirrored compared with the plug. Plug the harness into the
> desoldered socket, find each pin with a continuity tester from the wire of known colour, and
> mark it before you solder any lead.

## How to find the wires in your harness

1. **Unplug the harness from the car.** Number the positions of every plug as seen from one side,
   and write down the view (wire side or contact side, where the latch is). Most pin mix-ups come
   from a mirrored view.
2. **Continuity tester on the bench.** From each wire of the CAN box plug to the car connector and
   to the radio plugs. Write down colour, both ends and how you found it.
3. **Find the CAN pair.** It is usually the two wires that go only from the car connector to the
   CAN box plug.
4. **Find power and ground.** Ground beeps to the ground pin of the car connector. +12 V goes to
   the permanent battery pin, often through a branch to the radio power plug.
5. **Measure the CAN voltages in the car.** Ignition on, multimeter (DC) to ground on each CAN wire.
   About 2.5 V on both lines suggests high-speed CAN. One line near 0 V and the other near 5 V
   suggests low-speed fault-tolerant CAN; confirm with an oscilloscope. More in
   [CAN connection](../install-in-car/can-connection.md).
6. **Check for a terminator.** Unpowered CANH-CANL resistance of the CAN box: about 120 ohm means it
   has a terminator, kilo-ohms means it does not. The car bus is terminated already, so the device
   must not add one.
7. **Find the data direction.** With the radio on, measure the idle voltage of the two data lines.
   On the reference car the box line idled at 3.3 V and the radio line at 2.9 V; a small
   difference is a hint, not proof. Then record them with a listen-only recorder
   (`extras/sniffer`, environment `recorder`; it needs a second ESP32-S3 board and a CAN module):
   the line that carries the radio's commands is the radio TX. Details:
   [Radio connection](../install-in-car/radio-connection.md#find-the-direction).
8. **Connect the device in parallel, listen-only first.** See
   [Find your CAN bus](../make-it-yours/find-your-bus.md).

On the reference car the CAN lines measured 0.65 V (CANH) and 4.5 V (CANL) to ground with the
ignition on (measured: multimeter, DC average). That looks like low-speed fault-tolerant CAN at
125 kbit/s; an oscilloscope is still needed to confirm it.

## Checks before you power the board

> [!WARNING]
> **Check without power first**
>
> After soldering the leads, before any power, go through the
> [Installation checklist](../install-in-car/checklist.md). It includes the checks of the
> soldered socket: no short between neighbouring pins, none between +12 V and ground, CANH-CANL
> tens of kOhm.

Next: [Installation checklist](../install-in-car/checklist.md).
