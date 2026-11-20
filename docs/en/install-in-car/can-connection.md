# CAN connection

This page shows how to find the two CAN wires (CANH and CANL), how to tell which kind of CAN bus
you have, and how to connect the device to them without disturbing the car. The device needs the
two wires plus a common ground.

## Rules

1. **In parallel.** The device joins the existing pair. Never cut the CAN wires and never put the
   device "in series" between the car and another module.
2. **No terminator.** A CAN bus has its terminating resistors (120 ohm) already, inside the car's
   modules. The device must not add one. On the CAN Pal module of prototype v1 the terminator
   switch is OFF.
3. **Listen only first.** The first installation is always with a listen-only build, and on
   prototype v1 with JP1 open. See [Transmitting on the CAN bus](../safety/transmit.md).
4. **Measure before you connect.** Find the pair with a continuity tester and check it with a
   voltmeter. Never trust a wire colour or a seller's drawing alone.

The full list of checks before the car is in the [Installation checklist](checklist.md).

> [!CAUTION]
> **Do not connect CANH and CANL the wrong way round, and never to +12 V**
>
> Check each wire with the meter before you screw it into the device. A CAN wire on +12 V can
> damage the transceiver and the car module on the other end.

## Step 1: find the pair

On a car with an aftermarket CAN box the CAN pair usually goes from the car connector behind the
radio **only to the CAN box plug**. The radio itself has no CAN wires.

1. Unplug the harness from the car.
2. With a continuity tester, find the two wires that go from the car connector only to the CAN
   box plug (not to the radio plugs).
3. Write down both ends of each wire, with the view you used (wire side or contact side, where
   the latch is).

How to map a whole harness: [Finding and checking wires](../hardware/wiring.md).

## Step 2: measure the voltages

1. Plug the harness back in. Ignition on.
2. Multimeter on DC volts, minus probe on ground.
3. Touch each CAN wire in turn with the plus probe. With the harness plugged in, touch the wire
   from the back of the connector with a thin probe (back-probing), or use a T-adapter. Do not
   pierce the CAN wires.
4. Compare with the table.

A multimeter shows the **average** voltage while frames are passing, which is enough to tell the
two kinds of bus apart.

| What you read | Kind of bus | What it means |
|---|---|---|
| both wires near 2.5 V (CANH a little above, CANL a little below) | **high-speed CAN** (ISO 11898-2) | idle level is about 2.5 V on both lines; the usual speed is 500 kbit/s, sometimes 250 kbit/s |
| one wire near 0 V, the other near 5 V | probably **low-speed fault-tolerant CAN** (ISO 11898-3) | idle level: CANH near 0 V, CANL near 5 V; speed up to 125 kbit/s. The wire near 0 V is CANH |
| 0 V on both | no traffic, wrong wires, or the bus is asleep | try again with the ignition on and the engine running |
| about 12 V | a supply wire, not CAN | stop: wrong wire |

These are general properties of the two standards, not facts about one car. A multimeter is a hint
only: an oscilloscope shows the idle and the active levels and the bit length (8 µs per bit at
125 kbit/s, 2 µs at 500 kbit/s).

How to find the bit rate with the device in listen-only mode:
[Find your CAN bus](../make-it-yours/find-your-bus.md).

### Example: Peugeot 508 SW (reference car)

| Fact | Status |
|---|---|
| Quadlock pin 10 = CANH, pin 13 = CANL | measured: continuity from the CAN box transceiver to pin 10, then confirmed by error-free reception with this assignment (17 169 frames, no errors, no losses) |
| DC voltage to ground, ignition on, average: pin 10 = 0.65 V, pin 13 = 4.5 V; the same with and without the device connected | measured: multimeter |
| these levels look like **low-speed fault-tolerant CAN**, not high-speed | conclusion from the voltages, not confirmed: needs an oscilloscope (idle and dominant levels) |
| bit rate 125 kbit/s | measured: frames received without errors at this speed |
| a 3.3 V high-speed transceiver (SN65HVD230, TJA1051) receives this bus | measured: all recordings in `firmware/test_data/captures/` were made this way |
| CANH-CANL resistance of two aftermarket CAN boxes, unpowered: 48 kOhm and 59 kOhm | measured: ohmmeter; the boxes have no terminator |

The resistance in the last row is the reason for rule 2: the CAN boxes that worked on this car
had no terminator, so the device must not have one either.

> [!WARNING]
> **A high-speed transceiver on a low-speed fault-tolerant bus**
>
> The reference device uses a high-speed transceiver on what looks like a low-speed
> fault-tolerant bus. **Receiving** worked on every recording. Two things were **not measured**:
> the bus voltages while the car sleeps with the device connected, and what a high-speed
> transmitter does to such a bus. This is one more reason to stay listen-only. After installing,
> check that the remote key still opens the car after a long parking
> ([First drive](first-drive.md)).

## Step 3: check the device side

Without power:

1. Resistance between the device's CANH and CANL terminals: tens of kilo-ohms. About 120 ohm means
   a terminator is switched on: switch it off.
2. No continuity from CANH or CANL to ground or to +12 V.

On prototype v1 the screw terminal J5 has, from the top: CANH, CANL, GND, +12V.

## Step 4: connect

1. Ignition off, key out.
2. Connect ground first, then +12 V, then CANH and CANL to the same wires that go to the CAN box
   (or to its empty plug in the decoder mode). A short pair of leads to a spare plug, or a
   T-adapter, keeps the car harness intact.
3. Connect the computer on USB and open the console (how: the box at the top of the
   [Installation checklist](checklist.md)).
4. Ignition on. Type `stat`. Expected: `rx=` rises each time you type it, `lost=0`, `errors=0` and
   `bus=active`. The LED on the ESP32 module goes green while frames arrive. A few errors at the
   moment the bus wakes up can be normal; a count that keeps rising is not.

If `errors=` keeps rising or nothing arrives:

1. Switch the ignition off.
2. Check that CANH and CANL are not swapped.
3. Check the ground wire.
4. Still errors: the bit rate of the car profile may be wrong. Find it as in
   [Find your CAN bus](../make-it-yours/find-your-bus.md).

More in [Troubleshooting](../troubleshooting/index.md).

## The bit rate comes from the car profile

Each car profile sets the bus speed: 125 kbit/s for `psa_aee2010`, and 125 kbit/s by default for
`generic`, only because that is the speed of the reference car. With a speed the firmware does not
know, the CAN controller does not start and the device stays in a safe state. How to change it:
[Find your CAN bus](../make-it-yours/find-your-bus.md) and
[Write a car profile](../make-it-yours/write-a-profile.md).

Next: [Radio connection](radio-connection.md).
