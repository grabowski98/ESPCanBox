# Power and ACC

This page tells you where the device takes 12 V and ground, what it does when the car is parked,
and how the radio gets its ACC signal.

Words used here:

- **+12 V permanent** (often called B+ or BAT): live all the time, also with the car locked.
- **+12 V switched**: live only with the ignition on (or in the accessory position).
- **ACC** (accessory power): the wire that tells the radio "switch on". With an aftermarket CAN
  box it often does not come from the car, but from an output of the box.

## How it works on the reference car

On the reference car (Peugeot 508 SW) the CAN box is fed from Quadlock pin 12 (probably permanent
+12 V) and switches the radio's ACC itself. If pin 12 is permanent, as assumed, the box (and the
device in its place) is powered all the time and decides itself when the radio may run. The
evidence:

| Fact | Status |
|---|---|
| CAN box plug lower 1 (yellow) has continuity to Quadlock pin 12, which is +12 V | measured: continuity tester. That this +12 V is permanent (live with the key out) is an assumption: the voltage in the car was not measured |
| CAN box plug upper 1 (black) is ground, continuity to Quadlock pin 16 | measured: continuity tester |
| the radio's ACC IN (pin 15 of its 16-pin power plug) is fed from an output of the CAN box, not from the car | measured: unplugging the box with the engine running was seen by the radio as "ACC lost" (radio log). Which plug position carries it (lower 3, red) is from the wire colour and the harness topology; continuity was not recorded |
| after the key is removed the bus carries full traffic for about 100 s, then 14 s of frame `036` only, then silence until the car is opened | measured: listen-only recording |

Full harness map: [Peugeot 508 SW](../reference/vehicles/peugeot-508.md).

## What the device does when the car is parked

- **ACC:** on while the ignition frame says "ignition on" (any state except off). It goes off
  2.5 s after the ignition frame says "off". If the ignition frame stops arriving, the device
  treats the ignition as off after 1 s without it and then waits the same 2.5 s, so ACC goes off
  about 3.5 s after the last frame. This copies the CAN box: on the reference car the box
  switched the radio off 2.5 s after the ignition went off (measured: CAN and UART recording
  `firmware/test_data/captures/can_20261111_191045.log`; the host test `power_loss` replays it).
- **Sleep:** after 30 s without CAN frames the device goes to deep sleep. It wakes up as soon as
  the car starts talking on the bus again. It stays awake while the radio still has ACC, while a
  computer is connected on USB, or while the web panel asks for it. The panel may keep it awake
  only while the supply is above 11.0 V. These values are set in `src/config.h`
  ([Configuration and pins](../reference/config-and-pins.md)).

> [!WARNING]
> **Sleep current not measured**
>
> Deep sleep is in the code but was not tested on the hardware. The current the device draws
> from the battery while the car is parked was **not measured**. Measure it yourself before you
> leave the device in a car for days (Step 3).

## Step 1: find out what your supply wire is

Multimeter on DC volts, minus probe on a good ground.

1. Ignition on: the wire must show the battery voltage (about 12 to 14.5 V).
2. Ignition off, key out: permanent wire = still about 12 V, switched wire = 0 V.
3. Lock the car and wait a few minutes: a permanent wire stays at about 12 V.

Write down the result. A seller's drawing or a colour code is not enough. If the wire shows 0 V
already in step 1, it is not a supply wire: check your pin map again.

## Step 2: choose how to power the device

### Option A: +12 V permanent (like the CAN box)

This is how the CAN box was powered on the reference car. The device can keep ACC for 2.5 s,
follow the car until the bus goes quiet, and then sleep. It is the only option for the decoder
mode if you want the radio to behave like it did with the box.

The cost: the device stays connected to the battery all the time. If it does not sleep, it drains
the battery. Check it with Step 3.

### Option B: +12 V switched, from the fuse box

You can feed the device from a circuit that is live only with the ignition on, for example with a
fuse tap ("add-a-fuse") in the fuse box.

> [!WARNING]
> **Do not cut the car harness**
>
> The +12 V wire of the harness also feeds the CAN box (and, through the box, the radio's ACC).
> Leave it whole. You only change the lead that goes to terminal 4 (+12V) of J5 on the device.

1. Find a fuse slot that is live only with the ignition on. Use the fuse table in the owner's
   manual and confirm with the meter: ignition off = 0 V, ignition on = 12 V, car locked = 0 V.
2. Put the fuse tap in that slot, with a small fuse in the tap for the device. The board has its
   own input fuse as well (F1, 1.1 A).
3. Run a new wire from the fuse tap to J5 terminal 4 (+12V).
    - **Beside the box:** do not connect J5-4 to the box's +12 V wire. The box keeps its own
      supply as before.
    - **Decoder** (box unplugged, the device in its plug): do not connect the +12 V position of
      the harness plug (lower 1 on the reference harness) to J5-4. Insulate that lead of the
      device.
4. Keep ground and CAN as they are.
5. Check with the ignition off: the device has 0 V on J5-4, and the radio still has its own
   permanent B+ (on the reference radio pin 08 of its power plug, unverified: radio seller's
   drawing). If J5-4 still shows 12 V, it is still joined to a permanent wire: find it before you
   go on.

What changes:

- the device switches off with the ignition, so the radio loses ACC at once, not 2.5 s later;
- the device cannot see what the car does after the ignition is off (no recordings of the parked
  car);
- the device cannot drain the battery while parked.

> [!NOTE]
> **Not tried on the reference car**
>
> Option B was planned for the CAN box on the reference car but is **not reported as tried**
> with the device. It is a standard wiring method; check every step with the meter.

## Step 3: measure the parked current

Do this once with the device and once without it.

> [!WARNING]
> **Opening the battery circuit resets things**
>
> Disconnecting the battery minus may reset the radio, the clock and some car settings. On some
> cars it wakes the whole car. Follow the owner's manual. The steps below keep the circuit
> closed while you put the meter in.

1. Car locked, all doors closed. Wait until the car has gone to sleep (on the reference car the
   bus is silent about two minutes after the key is out).
2. Multimeter on the 10 A DC range. Clip one probe to the battery minus post and the other to the
   minus clamp.
3. Loosen the clamp and lift it off the post. The current now flows through the meter.
4. Wait until the reading settles: the car may wake up and go to sleep again.
5. Write down the value. Do not open a door or start the engine while the meter is in the
   circuit.
6. Put the clamp back on the post before you remove the meter.

Compare the two values. The difference is what the device costs while parked.

## The battery voltage in the console

On prototype v1 a divider measures the supply. `stat` prints it as `supply=12.31V (after the
diode)`: the value is **behind the input diode**, about 0.8 V (0.7-0.9 V) below the battery (the diode
drop is not measured exactly). On USB only, without 12 V, the line says `supply=no 12V`.

## ACC to the radio: which mode does what

| Mode | ACC output of the device | Who powers the radio's ACC |
|---|---|---|
| beside the box (`listen`, `listen_wifi`) | always off | the CAN box |
| decoder (`decoder`, `decoder_wifi`) | follows the ignition frame, as above | the device |
| `generic` profile | always off (the profile has no ignition frame) | the CAN box: leave it in place, the device only listens |

Next: [CAN connection](can-connection.md). How to wire the ACC, ILL and REV outputs:
[Radio connection](radio-connection.md).
