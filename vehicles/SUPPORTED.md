# Supported vehicles

| Vehicle | Firmware profile | Status | What works | Page |
|---|---|---|---|---|
| Peugeot 508 SW, 2015 (PSA AEE2010) | `psa_aee2010` | measured on one car | ignition / ACC, reverse, lights, parking sensors and tone, steering wheel and centre panel buttons, trip computer, outside temperature, climate state, doors, vehicle settings (read; changes only in the separate transmit build) | [peugeot-508-sw-2015](peugeot-508-sw-2015/README.md) |
| any car | `generic` | listen-only | records CAN frames, nothing is sent to the radio or the 12 V outputs | [_template](_template/README.md) |

Other PSA cars of the same generation (AEE2010) probably use many of the same frames, but nobody
has measured them with this project. Treat every frame of another car as **unverified** until you
have recorded it.

## Adding a car

Copy [_template/README.md](_template/README.md) into a new directory `vehicles/<make>-<model>-<year>/`,
fill it in from your own recordings and open a pull request. Every signal needs a status:
**measured** (with the recording and the action that changed it) or **unverified** (with the
source). Run `python -I tools/sanitize_capture.py` on every recording you share.
