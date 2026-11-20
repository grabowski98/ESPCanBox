# Bench test

This page checks the device on your desk before it goes near the car. Each part needs a little
more hardware than the one before. Stop at the first thing that does not match and find the
cause.

This is the short version for a first build. The full, step-by-step protocol for a new firmware
build, with the developer checks, is on the page
[Bench test protocol](../testing/bench-test-protocol.md).

## 1. The ESP32-S3 board on USB

Flash `listen_wifi` (or `listen`) as on the page [Flash the firmware](flash.md) and open the
monitor. Then type (your typing is not shown; press Enter after each command):

| Command | What you should see |
|---|---|
| `ver` | the version and the build time |
| `stat` | `[stat] mode=alongside the box can=ok audio=ok radio=ok` and the counters; with no 12 V the supply reads `no 12V` |
| `help` | the list of commands |
| `cantx` | `[cantx] built=0: no CAN transmit in this build (CAN listen only)` |

The LED on the ESP32-S3 board shows the state: blue at start, off while the bus is silent,
green while frames arrive, yellow while the device beeps, red when a driver failed at start (the
outputs stay off) or the CAN controller is not running.

You can paste a CAN frame into the console, for example `0E1#B0F000FCFC1EC200`. The device
treats it as if it came from the car, so you can try the decoding without a car. Expected: the
`[park]` line of the next `stat` changes.

## 2. The carrier board on 12 V, without modules

The carrier board is the board that connects the ESP32-S3 board to the car. Check it empty
first: no ESP32-S3 board and no modules in its sockets.

> [!WARNING]
> **Use a current limit**
>
> Power the carrier board from a bench supply with the current limit set to about 100 mA. If
> the supply hits the limit, switch off at once and look for a short.

1. Without power, with a multimeter: no short between +12 V and ground, +5 V and ground,
   +3.3 V and ground. Between CANH and CANL you should read tens of kΩ, not about 120 Ω.
2. 12 V on the power input (on prototype v1: terminal J5, `+12V` and `GND`). On the 5 V rail you
   should read about 4.6-4.8 V (the 5 V converter output after a diode).
3. Switch off, insert the ESP32-S3 board, switch on. Now the 3.3 V pins read 3.3 V and the
   current is tens of mA.

The prototype page has the full check list with the socket pins:
[Prototype PCB](../hardware/prototype-pcb.md).

## 3. With the modules

1. Switch off. Insert the CAN module (terminator switch OFF) and the amplifier module with its
   speaker.
2. Switch on and connect USB. Type `stat`. Expected: `can=ok audio=ok radio=ok`, and a supply
   voltage instead of `no 12V`. (On USB only, without 12 V, the prototype also showed
   `can=ok audio=ok radio=ok`; measured: ESP32-S3 + CAN module + amplifier.)
3. Type `test`. Expected: 1.5 s of test beeping from the speaker.
4. Type `vol 30` to change the volume. The firmware limits it to 80 % to protect a small
   speaker. A value below 15 % works only until the next restart.

## 4. The 12 V outputs (decoder builds only)

> [!WARNING]
> **Not with the CAN box connected**
>
> Flash `decoder` or `decoder_wifi` on the bench, or in the car only with the CAN box
> unplugged. The outputs of the device and of the box must never drive the same wire.

The ACC, ILL and REV outputs switch only in decoder mode, and they follow the car: ACC from the
ignition frames, ILL from the lights bit, REV from the reverse bit. Without CAN traffic they stay
off. A builder's bench report for these outputs does not exist yet (**unverified**); check them
with a multimeter and a small load before the radio is connected.

## 5. The web panel (builds with Wi-Fi)

With `listen_wifi` on the board, follow [First boot](first-boot.md) to log in. Then:

1. The panel opens and shows the **Dashboard**.
2. On the **Console** tab type `stat`. Expected: the same answer as on USB.

Update and rollback over Wi-Fi are steps 7.1-7.7 of the
[Bench test protocol](../testing/bench-test-protocol.md).

<details class="note" markdown="1">
<summary>Developer checks of the panel</summary>

Check these with the values on the panel or from `stat` on the console:

1. While Wi-Fi starts and stops, the main loop does not slow down ("Passes / s" and "Longest
   Wi-Fi step" on the System tab).
2. A firmware update through the panel works, and so does going back to the previous program.
   A wrong file is refused.
3. With a CAN source on the bench: during an update the counters of lost CAN frames stay at
   zero, and the radio link counters show no lost frames while a browser keeps the panel open.
4. A Wi-Fi scanner on your phone never shows an open network named `ESP_...` during several
   start-ups.
5. Deep sleep and the current in sleep after Wi-Fi was on: measure them. They have not been
   measured on the hardware yet.

Steps 1 and 2 above and items 1, 2 and 4 were checked on the prototype on USB,
without CAN and radio (measured: bench test of an earlier build of this firmware). Items 3
and 5 need CAN traffic or 12 V and are still open, and so is the time the radio stays without
ACC while the device restarts after an update.

</details>

## 6. Before the car

- The surge clamp diode (`D2` on prototype v1) is fitted and the input capacitor is rated 35 V
  or more.
- No 120 Ω terminator anywhere: the CAN module terminator switch is OFF.
- `JP1` on prototype v1 is open (listen-only hardware), unless you build the transmit firmware
  on purpose.
- You know the pins of your car.

## Next

[Installation checklist](../install-in-car/checklist.md) for the reference car, or
[Find your CAN bus](../make-it-yours/find-your-bus.md) for another car.
