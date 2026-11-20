# Transmitting on the CAN bus

The device **listens only**, unless you build the one transmit environment on purpose and arm it.
Read this page before you do that: it says how to switch transmitting on and off, what the
transmit environment can send and what stops it.

> [!CAUTION]
> **Transmitting changes the state of the car**
>
> A frame sent on the car bus can change settings in the body control unit (BSI). Build the
> transmit environment only if you understand this page, only for the reference car, and never
> while driving. The project is a hobby project, provided as is, and not for any safety system.

## Switch transmitting on step by step

Do steps 1 to 7 on the bench, not in the car.

> [!CAUTION]
> **Decoder mode: unplug the CAN box first**
>
> `decoder_tx_wifi` is a decoder environment. It sends frames to the radio and switches the
> 12 V outputs. Unplug the aftermarket CAN box first: two transmitters on the radio line and two
> ACC sources short the outputs. On the bench, connect nothing to the 12 V outputs.

1. Prepare the board. Jumper JP1 open means the CAN module cannot transmit, whatever program
   runs: keep it open for everyday use. To transmit, prototype v1 needs JP1 closed **and** a wire
   from SLNT to GPIO6 ([Prototype PCB](../hardware/prototype-pcb.md#transmit-path)). On the
   author's prototype JP1 is closed. Check the SLNT wire with a multimeter (see
   [Two states of the controller](#two-states-of-the-controller)).
2. Open a terminal in the top directory of the repository and flash the transmit environment:

    ```bash
    cd firmware
    uvx platformio run -e decoder_tx_wifi -t upload
    ```

    Expected: PlatformIO ends with `SUCCESS`. How to install the tools:
    [Flash the firmware](../quick-start/flash.md).
3. Open the [USB console](../reference/console.md) and press RESET on the board. Expected start
   lines: `TRANSMIT 15B: built, armed=0` and `# CAN controller: LISTEN ONLY (transmit disarmed)`.
   A new program always starts disarmed.
4. Do section 5 of the [bench test protocol](../testing/bench-test-protocol.md). Do not go on if
   a step fails.
5. Type `cantx on`. Expected: `[cantx] arming stored. Takes effect AFTER A RESTART ...`. If the
   answer is `[cantx] skipped: the car needs the device ...`, wait until reverse gear and the
   parking sensors are off, then type it again.
6. Restart the device: press RESET on the board, switch its power off and on, or press
   **Restart the device** in the panel (System tab).
7. Type `cantx`. Expected: `[cantx] built=1 armed=1 after_restart=1 controller=NORMAL ...`. The
   start lines now say `armed=1` and `# CAN controller: NORMAL, ...`. If you still see `armed=0`,
   look for `# CAN transmit arming stored by another program: cleared` or an `# ERROR:` line in
   the start lines.
8. In the car, with the ignition on, send a setting from the radio screen (or `set NN VV` on USB).
   By default only item `01` (parking assist) is enabled. If the gate refuses, see
   [When the gate refuses](#when-the-gate-refuses).
9. When the test is over, type `cantx off`. Expected: `[cantx] DISARMED immediately`. The
   controller goes back to listen-only at the next restart. For everyday use flash a listen-only
   environment again and open JP1.

## Which builds can transmit

| Environment | Can it transmit on CAN? |
|---|---|
| `listen`, `decoder`, `listen_wifi`, `decoder_wifi`, `listen_generic_wifi` | **No.** The directory `src/can_tx/` (the only code that calls the transmit function `twai_transmit`) is not compiled. The CAN controller always starts in listen-only mode: it sends no frames, no acknowledge (ACK) bits and no error frames |
| `decoder_tx_wifi` | Only the frames of the car profile's allow list, only after arming and a restart. Without arming it works like `decoder_wifi` |

`tools/check_no_can_tx.sh` checks the built programs: the five listen-only images must not
contain `twai_transmit`, the `decoder_tx_wifi` image must contain it, and in the sources only
`src/can_tx/can_tx_port.cpp` may call it.

The rest of this page shows how the gate works inside. Every number is copied from the code:
`lib/core/can_tx_gate.*`, `lib/core/can_tx_arm.*`, `src/can_tx/*`,
`lib/car_psa_aee2010/tx_allowlist.cpp` and `src/config.h` (paths relative to `firmware/`).

## Locks when you build

`decoder_tx_wifi` sets `-DCAN_TX_MODE=CAN_TX_SETTINGS` and `-DCAN_TX_EXPERIMENTAL=0`. The build
stops with an error when:

- `CAN_TX_MODE` is set without decoder mode (`WORK_MODE` must be `MODE_DECODER`);
- the car profile is `GENERIC` (listen-only profile) or `CUSTOM` (your own profile: its allow list
  cannot be checked when you build);
- the car profile is `EXAMPLE` (this is never allowed, with or without transmit);
- the allow list of the profile is empty (`static_assert` in `src/config.h`);
- `src/can_tx/` is compiled without `CAN_TX_MODE = CAN_TX_SETTINGS`;
- `CAN_TX_EXPERIMENTAL` is anything other than 0 or 1.

`tools/check_tx_profile_lock.sh` builds `decoder_tx_wifi` with `GENERIC` and with `EXAMPLE` in a
temporary directory and passes only if both builds fail with the expected message.

The image also carries a marker `canTx` (1 only in `decoder_tx_wifi`). A Wi-Fi update to an image
with a different `canTx` value needs a separate consent in the panel.

## What may be sent

The allow list of the PSA profile (`lib/car_psa_aee2010/tx_allowlist.cpp`) is `const` data in
flash. Nothing can add a row while the device runs.

| Frame | Build | Length | Content check |
|---|---|---|---|
| `15B` personalisation request | default (`CAN_TX_EXPERIMENTAL=0`) and experimental | 8 | **none: identifier and length only.** The bytes are computed from the current `260` frame (`vset::build15B`). A content check is a separate, open decision |
| `1A9` screen button | only with `-DCAN_TX_EXPERIMENTAL=1` | 8 | exactly one of two contents, both recorded from the aftermarket box with a reaction of the car: `00 7F FF 04 FF 00 10 00` (parking sensors), `00 7F FF 20 FF 00 10 00` (blind spot) |

The gate checks a frame in this order: identifier on the list (first matching row), then length
(`BAD_LEN`), then the content check. The hardware port checks every frame **a second time**
against the same list before it hands it to the controller.

> [!CAUTION]
> **The experimental build**
>
> `-DCAN_TX_EXPERIMENTAL=1` adds `1A9`. Set it only in your own `platformio_override.ini`,
> deliberately. In the reference car `1A9` was accepted only in part (see
> [below](#what-was-tried-in-the-car)). No build has a climate frame (`364`) or the Stop and Start
> content of `1A9`: both were removed on 2026-11-19, because they were never measured with an effect.
> This build is not part of CI: the host tests build its logic, and it was compiled with PlatformIO
> once by hand (2026-11-19, no warnings); it was never flashed from this source tree.

The profile `generic` has no allow list: its gate refuses every frame.

## Arming

Arming is stored in NVS (the settings area of the flash) as the **build string** of the program
in which you armed it (the build date and time, in the form `Mmm dd yyyy hh:mm:ss`). At start-up
the stored string must equal the string of the running program, at most 23 characters compared.
Different, missing, empty or too long = disarmed, and the stored value is erased. So every newly
flashed program starts disarmed, also when you flash the old one back.

> [!WARNING]
> **Arm on the bench first**
>
> Arm and test on the bench before you go to the car. Type `cantx off` as soon as a test is
> over, so the next restart does not come up armed.

| Command | What it does | From where |
|---|---|---|
| `cantx` | prints the state | anywhere |
| `cantx on` | stores the arming; it takes effect **after a restart** | USB, or the web panel through the device's own Wi-Fi network |
| `cantx off` | disarms **at once** and erases the stored arming at once | USB, or the panel through the device's own network |
| `cantx reset` | clears the [fault latch](#the-fault-latch) | USB only |
| `vset on NN`, `vset off NN` | enables or disables one settings item (stored in NVS) | USB, or the panel through the device's own network |
| `set NN VV` | the same path as the radio command `80 NN VV`, for a test without the radio | USB only |

From the panel through your home network these commands are refused. `cantx on` and `vset on|off`
are also refused while the car needs the device (reverse gear or active parking sensors).

## Two states of the controller

The controller mode is chosen once, at start-up (`canBegin()` in `src/can_bus.cpp`):

| State | CAN controller | Transceiver SLNT pin |
|---|---|---|
| disarmed (and every listen-only build) | listen-only | high: transmitter off |
| armed | normal mode, no transmit queue | high, except during the window of one frame |

The SLNT pin ("silent") of the transceiver switches its transmitter: high = transmitter off,
receiver working. The firmware sets it high as the very first instruction of `setup()`, and again
before the CAN driver starts. Only `src/can_tx/can_tx_port.cpp` sets it low.

The hardware port also needs a hardware timer (`esp_timer`). If the timer cannot be created, the
device stays disarmed and prints an error line.

> [!CAUTION]
> **SLNT is the only hardware barrier**
>
> JP1 open = no transmission possible; keep it open for everyday use. On the author's prototype
> JP1 is closed and SLNT is wired to GPIO6. The transceiver (TJA1051T/3 on the CAN module, per
> the board header) pulls its S pin **low inside the chip**, so an open SLNT wire means the
> transmitter is active. A 10 kΩ resistor (R3) to 3.3 V holds it high when the program does not
> drive it. Check this wire with a multimeter before you connect the device to a car. Details:
> `boards/board_proto_v1.h`.

## When the gate refuses

A request goes through these checks in this order. The first one that fails gives the refusal:

1. transmit not built (`NOT_BUILT`), disarmed (`DISARMED`), latched (`FAULT`);
2. frame not on the list or wrong content (`ID_NOT_ALLOWED`), wrong length (`BAD_LEN`);
3. a send or the pause after it is still running (`BUSY`);
4. no ignition (`IGNITION`): there must be a body frame younger than 1 s, received from the bus
   (not pasted), with ignition on or engine running (PSA: `0F6` byte 1 = `88` or `8E`);
5. no bus traffic in the last second (`BUS_QUIET`);
6. a Wi-Fi update session, a flash write or a pending settings write (`FLASH_BUSY`);
7. controller state: bus-off or transmit error counter (TEC) at 32 or more latches the gate. A
   controller that is not running, a receive error counter (REC) above 0, or a bus error less than
   100 ms ago gives a temporary refusal (`CONTROLLER`);
8. frame limit (`RATE`): at most **10 frames in any 60 s**.

The console prints the refusal as text. What to do:

| Refusal | Console text | What to do |
|---|---|---|
| `NOT_BUILT` | `no CAN transmit in this build` | flash `decoder_tx_wifi` (step 2 above) |
| `DISARMED` | `transmit disarmed` | type `cantx on`, restart, check with `cantx` (steps 5 to 7) |
| `FAULT` | `transmit locked after a fault` | write down the reason that `cantx` prints ([The fault latch](#the-fault-latch)), then type `cantx reset` on USB |
| `ID_NOT_ALLOWED` | `frame not on the list` | this build does not allow the frame; nothing to change at run time |
| `BAD_LEN` | `wrong frame length` | the same: the frame is not allowed |
| `BUSY` | `previous send in progress` | temporary: a settings change retries by itself |
| `IGNITION` | `no ignition` | switch the ignition on, or start the engine |
| `BUS_QUIET` | `bus is quiet` | the car is asleep: open it and switch the ignition on |
| `FLASH_BUSY` | `flash write in progress` | wait until the update or the settings write ends, then send again |
| `CONTROLLER` | `CAN controller not ready` | temporary: a settings change retries by itself. If it repeats, see [Frames lost or bus errors](../troubleshooting/index.md#frames-lost-or-bus-errors) |
| `RATE` | `frame count limit` | wait: at most 10 frames in any 60 s |

## The transmitter window

When a request passes, the gate opens the transmitter for **one frame**:

| Step | Value |
|---|---|
| hardware timer started, then SLNT low | timer `CAN_TX_HW_WINDOW_US` = **25 ms**; when it fires it sets SLNT high, whatever the main loop does |
| wait before the frame goes to the controller | **2 ticks** of the millisecond counter (at least 1 ms), so the transceiver can leave silent mode. The real switching time is not in the datasheet: not measured |
| send | one attempt only ("single shot"): the controller does not retry by itself |
| lost arbitration | not an error: up to **2** more attempts inside the same window |
| window limit in the gate | **20 ms**; longer = latch |
| SLNT high again | as soon as the result is known (acknowledged, failed, timeout) |
| pause before the next frame | **500 ms** |

While the window is open the main loop does nothing that could write flash: no USB commands, no
delayed settings writes, no panel requests. Before sleep, restart or an update the gate closes the
transmitter (`forceClose`).

> [!NOTE]
> **The device acknowledges other frames during the window**
>
> In normal mode with the transmitter open, the controller also acknowledges frames of other
> nodes during those few milliseconds.

## The fault latch

The latch is a lock: after an error the gate refuses every frame until you clear it. The gate
locks itself (state `fault`) after any of these:

| Reason | Console text |
|---|---|
| 3 failed sends in a row | `3 failed sends` |
| transmit error counter 32 or more | `transmit error counter` |
| bus-off | `bus-off` |
| transmitter open longer than 20 ms | `transmitter window exceeded` |
| controller event (bus-off alert, a retried frame, peripheral reset, or the hardware timer fired) | `controller event` |
| after a settings change, other bits of `260` changed as well | `foreign bits of 260 changed` |
| another reason from outside the gate | `external lock` |

The latch stays until `cantx reset` (USB only) or a restart. Disarming does not clear it.

## One settings change

The radio command `80 nn vv` (or `set NN VV` on USB) goes through the settings changer:

1. The item must be in the table, sendable, enabled in the item mask, and present in the car
   (`361`). By default the mask enables **only item `01`** (parking assist). Items with no
   measured frame are refused for good and cannot be enabled.
2. `15B` is computed from the current `260`.
3. If `260` already shows the requested state, nothing is sent.
4. The gate gets the frame. Temporary refusals are retried; any other refusal ends the command.
5. After the acknowledge, the changer waits up to **1300 ms** for a new `260`: accepted, or not
   accepted. If bits outside the requested field changed too, the gate latches.

`15B` is never repeated. A second command that arrives during the first one waits in one slot (the
last one wins).

The button items `05`, `0C` and `0D` use `1A9` instead (experimental build only). They need a fresh
state frame from the bus (`227` or `3E1`), send nothing when the car is already in the wanted
state, and wait up to **2500 ms** for the state to change.

## What was tried in the car

All results below come from the console of an earlier transmit build of this firmware in the
reference car (log not published). Details: [Peugeot 508 SW](../reference/vehicles/peugeot-508.md).

| Frame | Result | Status |
|---|---|---|
| `15B`, items `01`, `02`, `06`, `07`, `09`, `0F` | each frame acknowledged on the bus; the BSI showed the new state in `260` after 140-572 ms; each item both ways | measured. **The visible effect in the car was not checked** |
| `1A9` parking sensors | accepted both ways | measured |
| `1A9` blind spot | switching off accepted; switching on not accepted (one trial) | measured |
| `1A9` Stop and Start | not accepted (two trials); the content of this frame was never recorded from another device | measured |
| `364` climate, 4 and 6 bytes | acknowledged on the bus, **no effect on the climate control** | measured |

In this source tree the default `decoder_tx_wifi` refuses `1A9`, and every build refuses `364` and
the Stop and Start content of `1A9` (removed after these trials). The hardware code in
`src/` has no PC test; it is checked with the [bench test protocol](../testing/bench-test-protocol.md).

## Not known yet

- How long the transceiver needs to leave silent mode (oscilloscope).
- How late the hardware timer can fire during a flash write (it runs in a system task, not in an
  interrupt). This is why the gate refuses during the whole update session.
- Whether a content check for `15B` will be added.

Next: [Security model](security.md) for the web panel and Wi-Fi updates.
