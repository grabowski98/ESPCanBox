# Security

This device is connected to a car. Read this page before you build an environment with Wi-Fi
(`*_wifi`) or with transmitting (`decoder_tx_wifi`). More detail:
[docs/en/safety/security.md](docs/en/safety/security.md) and
[docs/en/safety/transmit.md](docs/en/safety/transmit.md).

## Reporting a problem

This is a hobby project with one maintainer and no paid support.

- If the repository has GitHub's private **"Report a vulnerability"** button (Security tab), use
  it.
- If not, open an issue that says only that you found a security problem and in which part
  (panel, firmware update, CAN transmit). Do not post details or exploit code in public. The
  maintainer will answer in the issue and agree on a private way to share the details.
- There is no e-mail contact.

There is no promise of a fix within any time. Only the latest version on the default branch is
looked at.

## Threat model

What the design assumes and what it does not protect against. "Measured" here means checked on
the bench with the device or the mock device; everything else is a property of the code.

### The car bus

- By default the device **only listens**. In every environment except `decoder_tx_wifi` the
  transmit code (`firmware/src/can_tx/`) is not compiled at all; `firmware/tools/check_no_can_tx.sh`
  checks the built images for the transmit function.
- In `decoder_tx_wifi` the device can transmit only the frames of the car profile's allow list
  (`firmware/lib/car_psa_aee2010/tx_allowlist.cpp`): by default frame `15B`, checked by **ID and
  length only, not by content**. `1A9` is in the list only in a build with
  `-DCAN_TX_EXPERIMENTAL=1`. The list is constant data in flash; nothing can add to it at run
  time. Transmitting also needs arming (`cantx on`) and a restart, and the gate limits it to one
  frame at a time, 500 ms apart, at most 10 per minute, with the transmitter enabled for at most
  20 ms. The build refuses a transmit environment with the `generic` profile or an empty list.
- **Software is the only barrier on a board where the transmit line of the CAN module is
  connected.** On the prototype board, jumper JP1 in the TX line gives a hardware guarantee of
  listen-only when it is open. On the author's prototype JP1 is closed, so the flashed program
  decides. Leave JP1 open unless you transmit on purpose.

### Wi-Fi and the web panel

- The device makes its own Wi-Fi network (WPA2). Its password is also the panel password: whoever
  knows it can log in. On first start the device makes a random password of 12 letters and
  digits; a password you set yourself (`wifi pass`) has 10 to 63 printable characters. It is shown
  only on the USB console (`wifi showpass`). `wifi newpass` makes a new random one.
- The panel uses **plain HTTP, no TLS**. On the device's own network the WPA2 encryption is the
  only protection of the traffic. On a home network (`wifi sta`) anybody on that network can
  read the traffic, including the login.
- Login is HTTP Basic, compared in constant time. For a client on a home network, 5 wrong
  passwords within 60 s lock the login for 30 s; each further lockout doubles the pause, up to
  5 minutes. Clients on the device's own network are not locked out. The lockout is one for all
  home-network clients and applies to a correct password too: anybody on the home network can keep
  the owner locked out of the panel there, without knowing the password (the device's own network
  still works).
- From a home network the panel is **read-only** unless you type `wifi lan on` on the USB
  console. Commands that arm or disarm transmitting (`cantx on`, `cantx off`, `vset on NN`,
  `vset off NN`) are accepted from the panel **only** from a client on the device's own network,
  even after `wifi lan on`. `wifi` commands (they carry passwords), `cantx reset` and `set NN VV`
  work only on the USB console.
- A CAN frame typed into the console (`0E1#...`) is fed only into the device's own logic, as if
  received; it is never sent to the car. That logic drives the 12 V outputs (ACC, reverse,
  illumination), the frames to the radio and the beeper, so a pasted frame (from USB, or from the
  panel after `wifi lan on`) can switch them like a frame from the car would. Frames pasted from the
  panel do not keep the device or its Wi-Fi awake.
- Every response is sent within a total time limit (20 s): a client that reads very slowly is
  disconnected and cannot hold the panel for others. The panel does not check the HTTP `Host`
  header, so a web page open in a browser on the device's own network could reach the panel by DNS
  rebinding; it still needs the password.

### Firmware update over Wi-Fi

- **Images are not signed.** Anyone who can log in to the panel can flash any program, including
  one that transmits. The checks before an update (product name and marker in the image, see
  `firmware/docs/PANEL_API.md`) prevent mistakes, not attacks.
- An image whose operating mode or `canTx` marker differs from the running program (for
  example listen-only to transmit, or back) needs a separate confirmation in the panel (`409 mode_change`). This is a safety question,
  not a signature. Such an upload, and a rollback to such a program, is accepted only from a client on
  the device's own network (`403 ap_only` from the home network, even after `wifi lan on`), like
  `cantx on`. An image whose marker lies is not stopped by this: after `wifi lan on`, anyone who can
  log in from the home network can still flash a program that transmits.
- Arming is stored together with the build string of the program, so any other program starts
  disarmed.

### What is not covered

- Physical access: anyone with a USB cable can read the password and flash anything.
- The radio and the car: this project does not make them more or less secure, and it does not
  protect the car against other devices on its bus.
- Secure boot and flash encryption of the ESP32-S3 are not used.

## Before you share anything

Recordings of the car bus contain the VIN (frames `336`, `3B6`, `2B6` on the reference car) and
show when and how the car was driven. Run `python -I tools/sanitize_capture.py IN.log OUT.log`
before you share a recording, and never post the line `[wifi] password:` of the console.
