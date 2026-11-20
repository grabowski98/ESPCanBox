# Installation checklist

This page is the one list of checks to do before, during and after the installation. Every line is
a check you can do with a multimeter, a continuity tester ("beeper") and the USB console. The pages
of this section explain each step:

1. [Power and ACC](power-and-acc.md): where the device gets 12 V, and what happens when the car
   is parked.
2. [CAN connection](can-connection.md): how to connect to the car bus without disturbing it.
3. [Radio connection](radio-connection.md): the UART data lines and the ACC, ILL and REV outputs.
4. [First drive](first-drive.md): what to check in the car, in which order.

Read [Safety](../getting-started/safety.md) first if you have not done it yet.

> [!CAUTION]
> **The device must only listen during installation**
>
> Install with a build that only listens on the car CAN bus: `listen`, `listen_wifi`,
> `listen_generic_wifi`, `decoder` or `decoder_wifi` (the decoder builds talk to the radio, never
> to the car). Never install for the first time with `decoder_tx_wifi`. On prototype v1 keep
> jumper JP1 open: then the CAN transceiver cannot transmit, whatever program runs. See
> [Transmitting on the CAN bus](../safety/transmit.md).

> [!NOTE]
> **How to open the USB console**
>
> You need the USB console for most checks on this list.
>
> 1. Connect the device to a computer with a USB cable.
> 2. In the repository folder run:
>
>     ```bash
>     uvx platformio device monitor -d firmware
>     ```
>
>     You see the lines the device prints. If the window stays empty, the device started before
>     the monitor: type `ver` and press Enter, it answers with one `[ver]` line.
>
> 3. Type a command, for example `stat`, and press Enter.
> 4. Leave the monitor with Ctrl+C.
>
> Details: [USB console](../reference/console.md#connect-to-the-console).

## Before you go to the car

1. A computer with the USB console works with the device (box above).
2. The device passed the [bench test](../quick-start/bench-test.md): `stat` shows `can=ok
   audio=ok radio=ok`, and the startup line says `LISTEN ONLY`.
3. You know which build is flashed: the console line `# profile: car=... radio=...` and `ver`.
   For an unknown car use the `generic` profile (`listen_generic_wifi`): it listens only, sends
   nothing to a radio and never switches the 12 V outputs.
4. Prototype v1: protection diode D2 (SMBJ18A) is fitted and C1 is rated 35 V or more. The Zener
   diodes D5, D7, D9 are recommended.
5. Prototype v1: JP1 is open (listen-only hardware).
6. Without power: no short between +12 V and ground, and none between neighbouring socket pins.
7. No 120 ohm terminator on the device. On the CAN Pal module the terminator switch is OFF.
   Unpowered resistance CANH-CANL of the device: tens of kilo-ohms.
8. You have the pin map of **your** harness, made with a continuity tester (method in
   [Finding and checking wires](../hardware/wiring.md)). A seller's drawing is only a hint.
9. You decided the operating mode:
    - **beside the box** (the console prints `alongside the box`): the original CAN box stays, the
      device only listens to the car and to the radio. Default (`listen`, `listen_wifi`).
    - **decoder**: the box is unplugged, the device talks to the radio and switches ACC, ILL and
      REV (`decoder`, `decoder_wifi`).
10. Decoder mode only: on the bench, with a small load (for example a 12 V bulb) on each output,
    ACC, ILL and REV read 0 V after start, in both modes. See
    [Radio connection](radio-connection.md#the-12-v-outputs-acc-ill-rev).

## In the car, before you plug anything in

Do these in this order.

1. Note the state of the car before the change: does the remote key open the car after one or
   two hours of parking? Does the parking tone behave? You need this to compare later.
2. Ignition on: measure the CAN pair. Which wire is CANH, which is CANL, high-speed or low-speed
   fault-tolerant ([CAN connection](can-connection.md#step-2-measure-the-voltages)).
3. Measure the supply wire with the ignition on, off and with the car locked: +12 V permanent or
   switched ([Power and ACC](power-and-acc.md#step-1-find-out-what-your-supply-wire-is)).
4. Ignition off, key out, no obstacle in front of or behind the car (the parking sensors must be
   quiet; see [First drive](first-drive.md#before-you-start) why). Only now plug anything in.

## Connecting

"In parallel" means: the original wire stays whole and goes on to the CAN box; you add a branch to
the device. Use a T-adapter or a spare plug. If you have neither, strip a short piece of
insulation, solder the branch wire and cover the joint with heat-shrink tube. Never cut the
original wire.

1. Ground first, then +12 V, then CANH and CANL **in parallel** with the existing wiring.
2. In the "beside the box" mode connect only: ground, +12 V, CANH, CANL and the radio TX data
   line (in parallel, to the device RX). Leave the device ACC, ILL, REV and its UART TX output
   unconnected.
3. In the decoder mode unplug the CAN box **before** the device takes its place. Two boxes on one
   UART line and two ACC sources would short their outputs.
4. Open the USB console before you switch the ignition on, so that you see it from the first
   second.

## After the first drive

1. `stat`: `lost=0`, `errors=0` and `bus=active` with the ignition on. A few errors at the moment
   the bus wakes up can be normal; a count that keeps rising is not: see
   [CAN connection](can-connection.md#step-4-connect).
2. `can-tx=none` in the last `[stat]` line (no transmit code in the program).
3. The radio starts and stops with the ignition (decoder mode).
4. The remote key still opens the car after a long parking.
5. You wrote down what you saw. A short note with the build, the mode and the result helps you
   and other people later ([Share your car](../make-it-yours/contribute.md)).

Next: [Power and ACC](power-and-acc.md).
