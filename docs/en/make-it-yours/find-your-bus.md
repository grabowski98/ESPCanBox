# Find your CAN bus

Before you can record or decode anything, you need three facts about your car:

1. which two wires carry the CAN bus at the radio,
2. what kind of CAN bus it is (high-speed or low-speed fault-tolerant),
3. its bit rate (the speed, in kbit/s).

This page shows how to find them with a multimeter and with the device in listen-only mode.
Nothing on this page sends anything to the car.

The whole way, from measuring the bus to sharing your car, is on one page:
[Porting checklist](porting-checklist.md). Keep it open and tick the rows as you go.

> [!CAUTION]
> **Use only a listen-only build on this page**
>
> Steps 1 and 2 need only a multimeter. From step 3 on, use only a listen-only build:
> `listen_generic_wifi` or the separate sniffer in `extras/sniffer/`. Never use an
> environment with transmission (`decoder_tx_wifi`) on a car you do not know yet. In a
> listen-only build the CAN controller sends no frames, no acknowledge bits and no error
> frames, so it stays invisible to the car even at a wrong bit rate. Read
> [Safety first](../getting-started/safety.md) before you start.

## What you are looking for

A modern car usually has several CAN buses. Each one links a group of modules and has its own
speed. The radio connector gives you access to one of them, and only that one. Which bus it is,
and which signals it carries, depends on the car: for your car it is not known until you
measure it.

On the reference car (Peugeot 508 SW, 2015) the bus at the radio carries ignition, reverse gear,
lights, steering wheel buttons, doors and parking sensors (measured: recordings in
`firmware/test_data/captures/`). Another car may put some of these signals on a bus that the radio
cannot see.

Collect your facts in the table of the vehicle template, `vehicles/_template/README.md`, section
"Bus at the radio". Fill a row only when you have measured it.

## Step 1: find the two wires

If your car has an aftermarket CAN box, its harness is the easiest way in: the CAN pair goes from
the car connector to the CAN box plug. [CAN connection](../install-in-car/can-connection.md)
(step 1) shows how to find it with a continuity tester, and [Wiring](../hardware/wiring.md) shows
how to map a whole harness.

Write down both ends of each wire and the side you looked from (wire side or contact side, where
the latch is). A pin number without a view is easy to read the wrong way round.

Example from the reference car: Quadlock pin 10 = CANH, pin 13 = CANL (measured: continuity from
the CAN box transceiver, then error-free reception with this assignment). The pin numbers of the
Quadlock housing are the author's own numbering; the match with the manufacturer's numbering is
unverified. Details: `vehicles/peugeot-508-sw-2015/wiring.md`.

### Check the terminator of the old box

The car's bus already has its terminating resistors. If the device replaces a CAN box, check
that the box had none, so you know the device must not have one either:

1. Unplug the CAN box and leave it unpowered.
2. Measure the resistance between its CANH and CANL pins with a multimeter.
3. Write the value down. Tens of kilo-ohms = no terminator in the box. About 120 ohm = the box
   terminates the bus; write it down and ask in an issue before you go on.

Example from the reference car: two aftermarket boxes, 48 kOhm and 59 kOhm (measured: ohmmeter,
unpowered), so neither box terminates the bus.

## Step 2: tell the kind of bus

Ignition on, multimeter on DC volts, minus probe on ground, plus probe on each CAN wire. The table
in [CAN connection, step 2](../install-in-car/can-connection.md#step-2-measure-the-voltages) tells
you what the readings mean. In short:

- both wires near 2.5 V: probably **high-speed CAN**;
- one wire near 0 V and the other near 5 V: probably **low-speed fault-tolerant CAN**; the wire
  near 0 V is CANH;
- about 12 V: not a CAN wire, stop.

A multimeter shows an average, so this is a hint, not a proof. An oscilloscope shows the real idle
and dominant levels.

Example from the reference car: CANH 0.65 V, CANL 4.5 V to ground, ignition on, the same with and
without the device connected (measured: multimeter). These levels look like low-speed
fault-tolerant CAN; this is a conclusion from the voltages and still needs an oscilloscope to
confirm.

> [!NOTE]
> **The device has a high-speed transceiver**
>
> The transceiver of the device is a 3.3 V high-speed type. On the reference car it received the
> bus without errors although the voltages look like low-speed fault-tolerant CAN (measured: all
> recordings in `firmware/test_data/captures/`). Whether it receives your car's bus is not known
> until you try. What a high-speed transmitter would do on such a bus was not measured, which is
> one more reason to stay listen-only.

## Step 3: find the bit rate

### With an oscilloscope

Look at the shortest pulse on CANH or CANL while frames are passing. That pulse is one bit:

| Shortest pulse | Bit rate |
|---|---|
| 2 µs | 500 kbit/s |
| 4 µs | 250 kbit/s |
| 8 µs | 125 kbit/s |
| 10 µs | 100 kbit/s |
| 20 µs | 50 kbit/s |

### With the device, by trial

The bit rate comes from the car profile. For an unknown car you use the `generic` profile: it
decodes nothing, never transmits and never switches the 12 V outputs on. Its speed is a single
number in `firmware/lib/car_generic/profile_generic.cpp`:

```cpp
    125000,
```

125 kbit/s is only the default, because it is the speed of the reference car. It is not a fact
about your car.

1. Connect the device to the two wires from step 1
   ([CAN connection](../install-in-car/can-connection.md)).
2. Pick a first guess from step 2. A low-speed fault-tolerant bus runs at 125 kbit/s or less; a
   high-speed bus often at 500 kbit/s, sometimes 250 kbit/s (general properties of the two
   standards, not facts about your car). Try in this order: low-speed `125000`, then `100000`,
   `50000`; high-speed `500000`, then `250000`, `125000`.
3. Put the speed in bits per second into `profile_generic.cpp`. The firmware knows only these
   values: `50000`, `100000`, `125000`, `250000`, `500000`, `1000000` (the list is in
   `firmware/src/can_bus.cpp`). With any other value the CAN controller does not start and the
   device stays in its safe state.
4. Build and flash, from the `firmware` folder: `uvx platformio run -e listen_generic_wifi -t upload`.
   Expected: `[SUCCESS]`. If the upload does not start, see
   [Flash the firmware](../quick-start/flash.md).
5. Ignition on. Open the USB console with `uvx platformio device monitor` and type `stat` a few
   times ([Console](../reference/console.md)).
6. Read the line that starts with `[stat] rx=`:

| What you see | What it means |
|---|---|
| `rx=` rises by hundreds per second, `errors=` stays the same | right speed |
| `rx=` stays at 0, `errors=` rises | wrong speed, or CANH and CANL swapped |
| `rx=` stays at 0, `errors=` stays at 0 | no traffic: bus asleep, wrong wires, or no ground |

A few errors at the moment the bus wakes up can be normal. In one reference recording the error
counter of the sniffer went from 136 to 192 while the bus woke up and then stayed at 192 for the
rest of the 14 minutes (measured: the `# stat` lines of `can_20261110_190421.log`).

If the speed is wrong, change the number, flash again and repeat. Try the next value only after
you have checked the wires.

The separate sniffer in `extras/sniffer/` has its speed fixed in its source: `CAN_TIMING`,
125 kbit/s (`extras/sniffer/README.md`). The `scan` command of its recorder build measures the
two UART lines to the radio, not the CAN bus.

## Step 4: when is the bus awake

Find out when your bus is silent. You need this to plan the recordings: a marker set while the bus
sleeps gets the time of the moment the bus wakes up ([Record](record.md)).

Example from the reference car: with the key out the bus is silent. After the ignition is switched
off there is full traffic for about 100 s, then 14 s of one frame (`036`) only, then silence
(measured: recordings of this car; `vehicles/peugeot-508-sw-2015/wiring.md`).

## Step 5: a first look

With the right speed, type `dump` in the USB console. The device prints every frame as one line:

```
(140.548) can0 0F6#8E501E33BE5D5D24
(140.569) can0 128#C0B0010200411400
(140.573) can0 0E1#B4003FFCFCFCC200
```

That is time in seconds, interface, frame ID, `#`, and the data bytes in hex. Type `dump` again to
stop it. The lines above are from `centralka_can_20261117_184411.log`.

For scale: the reference car sends about 218 frames per second with 70 different IDs with the
ignition on (measured: `can_20261110_190421.log`). Your car can send more or fewer.

Write down what you found (connector, pins and view, kind of bus, bit rate, when the bus sleeps).

Next: [Record the bus](record.md).
