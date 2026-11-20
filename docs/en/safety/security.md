# Security model

This page says what the design of the device assumes and what it does not protect against. Read
it before you build an environment with Wi-Fi (`*_wifi`) or with transmitting
(`decoder_tx_wifi`). Everything here is a property of the code, unless it says otherwise.

How to report a security problem: `SECURITY.md` in the root of the repository.

## The car bus

- By default the device **only listens**. In every environment except `decoder_tx_wifi` the
  transmit code (`firmware/src/can_tx/`) is not compiled at all.
  `firmware/tools/check_no_can_tx.sh` checks the built images for the transmit function.
- In `decoder_tx_wifi` the device can send only the frames of the allow list of the car profile
  (`firmware/lib/car_psa_aee2010/tx_allowlist.cpp`). By default this is frame `15B`, checked by
  **ID and length only, not by content**. `1A9` is in the list only in a build with
  `-DCAN_TX_EXPERIMENTAL=1`. The list is constant data in flash; nothing can add to it while the
  device runs.
- Transmitting also needs arming (`cantx on`) and a restart. The gate sends one frame at a time,
  at least 500 ms apart, at most 10 per minute, and keeps the transmitter enabled for at most
  20 ms. Details: [Transmitting on the CAN bus](transmit.md).
- A transmit environment does not compile with the `generic` profile or with an empty list.

> [!CAUTION]
> **Software is the only barrier when JP1 is closed**
>
> JP1 open = no transmission possible: jumper JP1 in the TX line of the CAN module of the
> prototype gives a hardware guarantee of listen-only. Keep it open for everyday use. Close it
> only to transmit on purpose with `decoder_tx_wifi` ([Transmitting on the CAN bus](transmit.md#switch-transmitting-on-step-by-step)).
> With JP1 closed, the flashed program decides. On the author's prototype JP1 is closed.

## Wi-Fi and the web panel

- The device makes its own Wi-Fi network (WPA2). Its password is also the panel password:
  whoever knows it can log in.
- On first start the device makes a random password of 12 characters. A password you set
  yourself (`wifi pass`) has 10 to 63 printable characters, without spaces. The password is shown
  only on the USB console (`wifi showpass`). `wifi newpass` makes a new random one.
- The panel uses **plain HTTP, without TLS**. On the device's own network, WPA2 is the only
  protection of the traffic. On a home network (`wifi sta`), anybody on that network can read
  the traffic, including the login.
- Login is HTTP Basic, and the password is compared in constant time. For a client on a home
  network, 5 wrong passwords within 60 s lock the login for 30 s; each further lockout doubles
  the pause, up to 5 minutes. Clients on the device's own network are not locked out.
- The browser keeps the password in plain text, because the panel adds it to every request. By
  default it is in `sessionStorage`: only that tab, gone when the tab is closed. With
  **Remember on this device** ticked at login it is in `localStorage`: every tab of that browser,
  kept after a restart, readable by anyone who can use that browser profile or reads its files.
  **Log out** removes it from both places; a rejected password (`401`) is removed too. Tick the box
  only on your own device.
- From a home network the panel is **read-only** unless you type `wifi lan on` on the USB
  console.
- Commands that arm or disarm transmitting (`cantx on`, `cantx off`, `vset on NN`,
  `vset off NN`) are accepted from the panel **only** from a client on the device's own network,
  even after `wifi lan on`. `wifi` commands (they carry passwords), `cantx reset` and `set NN VV`
  work only on the USB console.
- A CAN frame typed into the console (`0E1#...`) goes only into the device's own logic, as if it
  was received. It is never sent to the car.

## Firmware update over Wi-Fi

> [!WARNING]
> **Update images are not signed**
>
> Anyone who can log in to the panel can flash any program, including one that transmits. The
> checks before an update (product name and marker in the image) prevent mistakes, not attacks.

- An image with a different operating mode or a different CAN transmit capability than the
  running program needs a separate confirmation in the panel (the API answers `409 mode_change`
  first). This is a safety question, not a signature.
- Arming is stored together with the build string of the program, so any other program starts
  disarmed.

The update API: [Panel API](../reference/panel-api.md).

## What is not covered

- **Physical access.** Anyone with a USB cable can read the password and flash any program.
- **The radio and the car.** This project does not make them more or less secure, and it does
  not protect the car against other devices on its bus.
- **Secure boot and flash encryption** of the ESP32-S3 are not used.

## Before you share anything

Recordings of the car bus contain the VIN (frames `336`, `3B6` and `2B6` on the reference car)
and show when and how the car was driven. Clean a recording before you share it. Type this in the
top directory of the repository (not in `firmware/`):

```
python -I tools/sanitize_capture.py IN.log OUT.log
```

Expected: a line `IN.log -> OUT.log: ... frames overwritten ...` and a new file `OUT.log`.
`python -I tools/sanitize_capture.py --check OUT.log` must then print `0 problem(s)`.

Never post the line `[wifi] password:` of the console.

Next: [Share your car](../make-it-yours/contribute.md) says how to share a recording.
