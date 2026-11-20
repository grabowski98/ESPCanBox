# How it works

This page explains what the device does and how its parts fit together. You do not need it to
flash the firmware.

## The problem it solves

An aftermarket Android radio does not understand the car. A small **CAN box** sits between them:
it reads the CAN bus of the car and tells the radio, over a serial line (UART), what happens:
steering wheel buttons, parking sensors, doors, outside temperature, ignition. It also switches
the radio on and off (the ACC wire) and gives it the reverse and illumination signals.

This device does the same job with an ESP32-S3 board, open firmware and a web panel. It can work
**beside** the box (to learn and compare) or **instead of** it.

## The picture

```text
   car CAN bus (125 kbit/s on the reference car)
        |
        v
   CAN transceiver module (SLNT high = transmitter off)
        |
        v
 +--------------------------- ESP32-S3 ---------------------------+
 |                                                                |
 |  TWAI controller (listen-only)                                 |
 |        |                                                       |
 |        v                                                       |
 |  car profile (lib/car_*)                                       |
 |        |                                                       |
 |        v                                                       |
 |  decoder logic (lib/app) ---+--> beeper -------> I2S amplifier --> speaker
 |                             |                                  |
 |                             +--> radio protocol (lib/radio_*) ---> UART to the radio  (*)
 |                             |                                  |
 |                             +--> power policy -> 12 V outputs ---> ACC, ILL, REV      (*)
 |                                                                |
 |  USB console, 115200 baud  <--> computer                       |
 |  Wi-Fi panel (*_wifi builds only)  <--> phone or laptop        |
 +----------------------------------------------------------------+

 (*) decoder mode only. In "beside the box" mode the device sends nothing
     to the radio and keeps the 12 V outputs off; it only listens to the
     radio -> box line.
```

TWAI = the CAN controller built into the ESP32-S3. UART = the serial line to the radio.
I2S = the digital audio link to the amplifier.

## Two modes

The mode is chosen by the PlatformIO **environment** you build (a named set of build options in
`firmware/platformio.ini`), never by editing the code. The table of all environments is on the
page [Flash the firmware](../quick-start/flash.md).

| Mode | Environments | When | What the device sends |
|---|---|---|---|
| beside the box (default) | `listen`, `listen_wifi`, `listen_generic_wifi` | the factory or aftermarket CAN box stays connected | nothing to the radio, 12 V outputs off; it beeps from its own speaker and listens to the CAN bus and to the radio -> box line |
| decoder | `decoder`, `decoder_wifi`, `decoder_tx_wifi` | the CAN box is unplugged, the device takes its place | frames to the radio over UART; ACC, illumination and reverse on the 12 V outputs |

In both modes the car CAN bus is only listened to. The one exception is the separate environment
`decoder_tx_wifi`, described on the page [Transmitting on the CAN bus](../safety/transmit.md).

## What happens to one CAN frame

1. The **CAN transceiver** turns the bus voltages into logic levels. Its silent pin is held high,
   so it cannot drive the bus.
2. The **TWAI controller** (the CAN controller built into the ESP32-S3) receives the frame in
   listen-only mode.
3. The **car profile** knows which frames matter: for the reference car the profile
   `psa_aee2010` reads, for example, the ignition from frame `0F6`, the lights from `036`, the
   parking sensors from `0E1` and the steering wheel buttons from `21F`. The profile `generic`
   decodes nothing: it is the starting point for another car.
4. The **decoder logic** keeps the state of the car and decides what to do:
   - the **beeper** plays the parking sensor rhythm from the nearest zone, a continuous tone when
     the car asks for it, and goes silent 400 ms after the last `0E1` frame;
   - the **radio protocol** (`XP` for radios set to the Simple Soft CAN box profile, or `none`)
     turns key presses and states into UART frames, at least 10 ms apart;
   - the **power policy** decides when ACC, illumination and reverse go on.
5. In decoder mode the UART frames go to the radio and the 12 V outputs switch. In "beside the
   box" mode both stay off.

The same logic (everything in `firmware/lib/`) also compiles on a PC. The host tests replay real
recordings from the car through it and compare the result with stored references. See
[Host tests](../testing/host-tests.md).

## Where the code lives

| Directory | What it holds |
|---|---|
| `firmware/lib/core/` | logic without hardware: frame parsing, beeper rhythm, power policy, transmit gate, panel helpers |
| `firmware/lib/car_psa_aee2010/`, `lib/car_generic/` | car profiles: which frames, which bits, which bus speed |
| `firmware/lib/radio_xp/`, `lib/radio_none/` | radio protocols |
| `firmware/lib/app/` | the `Decoder` class that joins car, radio and outputs |
| `firmware/src/` | hardware: CAN driver, sound, UART, 12 V outputs, sleep, console, Wi-Fi panel |
| `firmware/boards/` | pin numbers of each board |

More on the page [Architecture](../reference/architecture.md).

## Console and panel

- The **USB console** (115200 baud) prints the state and accepts commands such as `stat`, `ver`,
  `test` and `dump`. With `dump` it prints every CAN frame in the format
  `(seconds) can0 ID#DATA`, the same format the recordings use. All commands:
  [Console](../reference/console.md).
- The **web panel** (only `*_wifi` builds) runs on the device's own Wi-Fi network. It shows the
  car state, the CAN traffic, the radio link and a console, and can update the firmware over
  Wi-Fi. See [First boot](../quick-start/first-boot.md) and the
  [panel guide](../reference/panel-guide.md).

## Sleep

The device stays on permanent 12 V. After 30 s without CAN frames (and without a computer on
USB) it goes to deep sleep and wakes up on the next CAN frame. On the reference car the bus goes
quiet about 100 s after the key is taken out (measured: recording of a switch-off). The sleep
itself and the current in sleep have not been measured on the hardware yet.

## What is known to work

The status of each function on the reference car is on the page
[Peugeot 508 SW](../reference/vehicles/peugeot-508.md) and in `vehicles/SUPPORTED.md`. In short:
ignition and ACC, reverse, lights, parking sensors and tone, steering wheel and centre panel
buttons, trip computer, outside temperature, climate state and doors are decoded from measured
frames. The bus at the radio of that car runs at 125 kbit/s (measured); its voltages look like
low-speed fault-tolerant CAN, which still needs an oscilloscope to confirm.

## Next

[What you need](requirements.md): board, parts and tools.
