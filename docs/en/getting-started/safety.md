# Safety first

Read this page before you build, flash or connect anything. It is short on purpose.

## Words used on these pages

- **the device**: the ESP32-S3 board of this project;
- **the box** or **CAN box**: the aftermarket interface box sold with the radio;
- **the radio**: the Android head unit;
- **listen-only**: the device receives CAN frames and sends nothing on the car bus;
- **TWAI**: the CAN controller built into the ESP32-S3;
- **SLNT**: the silent pin of the CAN transceiver module; high = its transmitter is off;
- **BSI**: the body control unit of the car;
- **`JP1`**: a jumper on the prototype board in the TX line to the CAN module;
- **environment**: a named set of build options; it decides what the program can do.

The [glossary](../contributing/glossary.md) lists more terms.

## What this project is

- A hobby project, provided **as is**, without any warranty. You use it at your own risk.
- It was developed and measured on **one car** (a Peugeot 508 SW from 2015) with one Android
  radio. On any other car every frame is **unverified** until you have recorded it yourself.
- It is **not** for anything that keeps people safe. Do not connect it to brakes, steering,
  airbags, engine control or any bus that carries them.
- The project is not affiliated with Peugeot, Stellantis, Simple Soft, Raise, Junsun or FYT.
  Their names appear only to say what was tested.

## The device listens by default

The car side of the device is a CAN receiver. In every normal build it **only listens**:

- the TWAI controller runs in listen-only mode (it does not even send the acknowledge bit);
- the program keeps the `SLNT` pin of the CAN transceiver high, so the transmitter of the chip
  is off;
- the builds `listen`, `decoder`, `listen_wifi`, `decoder_wifi` and `listen_generic_wifi` do not
  contain the transmit code at all (the directory `firmware/src/can_tx/` is not compiled;
  `firmware/tools/check_no_can_tx.sh` checks the compiled programs).

> [!CAUTION]
> **Transmitting can change the state of the car**
>
> Do not flash `decoder_tx_wifi` unless you have read
> [Transmitting on the CAN bus](../safety/transmit.md) and accept the risk. Never send frames you
> have not recorded from the car itself.

Only one environment, `decoder_tx_wifi`, can transmit on the car bus. It does so only after you
arm it on purpose and restart the device:

| Build | Frames it may send on the car bus |
|---|---|
| all environments except `decoder_tx_wifi` | none |
| `decoder_tx_wifi` (default) | only frame `15B`, checked by ID and length only, after `cantx on` and a restart |
| `decoder_tx_wifi` with `-DCAN_TX_EXPERIMENTAL=1` in your own `platformio_override.ini` | `15B`, plus the trial frame `1A9` (parking sensors and blind spot buttons only) |

Climate control from the radio does **not** work with this project. For `15B` the BSI accepted
the settings (seen in frame `260`), but the visible effect in the car was **not checked**. What
each frame did on the reference car: [Transmitting on the CAN bus](../safety/transmit.md).

## Hardware lock on the prototype board

The [prototype board](../hardware/prototype-pcb.md) as drawn has a hardware lock: the `SLNT` pin
of the CAN module is pulled up to 3.3 V by a resistor, and jumper `JP1` stays **open**. With
`JP1` open no program can transmit on the bus.

The author's own board was later changed for transmit tests (`JP1` closed, `SLNT` wired to
GPIO6). On such a board **only the firmware** decides whether the device transmits. Keep `JP1`
open unless you build the transmit firmware on purpose.

## Before you start

These rules come from how the project itself was built. Follow them in this order.

1. **Measure before you connect.** Find which pins carry CAN, what type of bus it is
   (high-speed or low-speed fault-tolerant) and its speed, with a multimeter and, better, an
   oscilloscope or a logic analyser. Do not connect anything to the bus before you know this.
   The page [Find your bus](../make-it-yours/find-your-bus.md) shows how.
2. **No terminator.** The car bus is terminated already. Switch off the 120 Ω terminator of your
   CAN module and do not add one.
3. **Listen first.** On the desk any listen build is fine. In a car, use `listen` or
   `listen_wifi` only on the reference car (Peugeot 508, PSA AEE2010 profile); in any other car
   use only `listen_generic_wifi` (generic profile: records frames, sends nothing to the radio).
   Record, decode, compare. Transmitting comes last, if ever.
4. **No writes to the car's control units.** This project does not do diagnostics and never
   writes to modules of the car (BSI or any other). Do not add such code.
5. **One source per wire.** Use the decoder mode (`decoder`, `decoder_wifi`) only with the
   aftermarket CAN box unplugged. Two transmitters on the radio data line, or two sources of the
   ACC wire, mean shorted outputs.
6. **Bench before car.** Power the carrier board from a bench supply with a current limit (about
   100 mA for the first power-up) before you take it to the car. The ESP32-S3 board alone on a
   USB cable does not need a bench supply. See [Bench test](../quick-start/bench-test.md).

## 12 V and the battery

> [!WARNING]
> **Fit the input protection before the car**
>
> On the prototype board fit the surge clamp diode `D2` (SMBJ18A) and use an input capacitor
> rated 35 V or more before you connect the board to the car. Keep the fuses in place; a wire
> bridge instead of a fuse is for the bench only.

- The device sits on permanent 12 V (like the CAN box it replaces). It goes to deep sleep after
  30 s of silence on the bus. Sleep and the current in sleep have **not been measured** on the
  hardware yet. Measure the quiescent current of the car with and without the device before you
  leave it connected for days.
- With Wi-Fi on the device draws more current. The firmware turns Wi-Fi off when the car is
  asleep and nobody uses the panel.

## The web panel and updates over Wi-Fi

The `*_wifi` builds open their own Wi-Fi network with a web panel. Know the limits:

- the panel uses plain HTTP, no encryption beyond WPA2 of the Wi-Fi network;
- the device password is also the Wi-Fi key;
- firmware images sent over Wi-Fi are **not signed**. Anyone who has the password can flash
  another program. On a board without the hardware lock that program could transmit.

More on the page [Security model](../safety/security.md).

## Next

[What you need](requirements.md): board, parts and tools.
