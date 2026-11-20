## What and why

<!-- One or two sentences. Link the issue if there is one. -->

## Type

- [ ] Bug fix
- [ ] New car or radio data (`vehicles/`, `radios/`, profile)
- [ ] Translation
- [ ] Documentation
- [ ] Hardware
- [ ] Other:

## Car facts

<!-- Delete this section if the change says nothing about a car, radio or CAN box. -->

- [ ] Every new fact has a status: **measured** (how, which recording) or **unverified** (source).
- [ ] Recordings are cleaned with `python -I tools/sanitize_capture.py`.
- [ ] Nothing comes from decompiled apps, box firmware or code under an incompatible licence.

## Transmitting on CAN

<!-- Delete this section if the change does not touch transmitting. -->

- [ ] This change adds or changes a frame the device can send. If yes: the frame was measured,
      it has a content check in code, and a host test covers it (`tx_path`, `can_tx_gate`).
- [ ] Environments without transmitting still have no transmit code (`bash tools/check_no_can_tx.sh`).

## Checks

- [ ] `bash tools/run_host_tests.sh` (from `firmware/`) passes; any change of test patterns is
      explained above.
- [ ] The environments I touched build (`uvx platformio run -e <name>`).
- [ ] `python -I tools/scan_private.py .` reports no errors.
- [ ] Panel texts: new keys only in `en.json`; `python -I tools/check_i18n.py` has no errors.
- [ ] Documentation: `python -I tools/check_doc_links.py`, `python -I tools/check_translations.py` and `mkdocs build --strict` pass.
- [ ] Changes in `firmware/src/`: I tried them on a board / I could not (say which).
- [ ] Console line format, commands or API codes changed: noted in `CHANGELOG.md`.

By opening this pull request I agree that it is published under the licences of the repository
(code Apache-2.0, documentation CC BY 4.0, hardware CERN-OHL-S-2.0).
