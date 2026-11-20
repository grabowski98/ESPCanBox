# Continuous integration

> [!NOTE]
> **Not run on GitHub yet**
>
> The workflow files below exist and pass `actionlint`, and `tools/release_check.sh` passes on a
> Windows PC (Git Bash). They have not run on a GitHub runner yet: that happens with the first
> push. The host tests with `g++` in particular have only been planned so far.

The repository has three files for GitHub:

| File | When it runs | What it does |
|---|---|---|
| `.github/workflows/ci.yml` | every push and pull request, or by hand | all checks (one command) and the host tests with a second compiler |
| `.github/workflows/release.yml` | by hand, or on a version tag `v<x.y.z>` | builds the release images; only images without CAN transmit |
| `.github/dependabot.yml` | monthly | a pull request when a pinned GitHub Action has a new version |

Both workflows run on `ubuntu-24.04`, install only `uv` (pinned version) and get every other tool
through `uvx` with pinned versions (PlatformIO, MkDocs, actionlint). The actions they use are pinned
to commit hashes. They use no secrets and get a read-only token (`permissions: contents: read`).
Neither of them talks to a board.

## One command: `tools/release_check.sh`

CI runs the same script you run at home, from the repository root:

```bash
bash tools/release_check.sh                 # every check; exit code 0 only when all passed
bash tools/release_check.sh --skip-build    # quick run without the PlatformIO builds (not a release check)
make release-check                          # the same as the first line
```

Every step runs even after a failure, and a summary at the end lists the passed, skipped and
failed steps. The exit code is 1 when any step failed.

| Step | Command | Why |
|---|---|---|
| `host_tests` | `bash tools/run_host_tests.sh` (in `firmware/`) | logic of `lib/` against the test patterns; compiler from `CXX`, default zig through `uvx` |
| `can_diff_selftest` | `python -I tools/can_diff.py test_data/synthetic/marker_demo.log --selftest` (in `firmware/`) | the bit finder of [Decode a new frame](../make-it-yours/decode.md) still finds the known bits |
| `build_envs` | `uvx platformio run -e …` for all six environments (in `firmware/`) | every environment compiles; nothing is uploaded |
| `check_no_can_tx` | `bash tools/check_no_can_tx.sh` (in `firmware/`, after the build) | `twai_transmit` exists only in `decoder_tx_wifi` |
| `check_tx_profile_lock` | `bash tools/check_tx_profile_lock.sh` (in `firmware/`) | `decoder_tx_wifi` must not build with the `GENERIC`, `EXAMPLE` or `CUSTOM` car profile |
| `check_layering` | `python -I tools/check_layering.py` (in `firmware/`) | `lib/core` stays car-neutral |
| `check_i18n` | `python -I tools/check_i18n.py --langs en,pl,de,es,ru,uk` (in `firmware/`) | every panel language has every key, same placeholders |
| `project_identity` | `python -I tools/set_project_name.py --check` | the name in `project.json` matches the firmware and panel |
| `scan_private` | `python -I tools/scan_private.py .` | no VIN, e-mail, MAC address, network name or author name |
| `check_excluded` | `python -I tools/check_excluded.py .` | no file from the exclusion list |
| `strip_exif` | `python -I tools/strip_exif.py --check .` | no EXIF data (for example a GPS position) in images |
| `check_provenance` | `python -I tools/check_provenance.py --root .` | every protocol constant in the code has a row and a source in `radios/xp-simple-soft/provenance.md` |
| `check_doc_links` | `python -I tools/check_doc_links.py` | no dead relative links or anchors |
| `check_translations` | `python -I tools/check_translations.py` | every translated page has an English page and the banner; same headings, code blocks and table rows as English |
| `mkdocs_strict` | `mkdocs build --strict` (mkdocs-material, mkdocs-static-i18n) into a temporary directory | the documentation site builds without warnings; no `site/` is left behind |
| `actionlint` | `actionlint .github/workflows/*.yml` (with shellcheck) | the workflow files themselves are valid |

`--binaries DIR` adds one more step: `scan_private.py` over the images in `DIR`.

Variables: `PIO` (default `uvx platformio`), `PYTHON` (default `python`), `CXX` (host tests),
`MKDOCS` and `ACTIONLINT` (other commands for those two tools).

## `ci.yml`

1. Check out, set up Python 3.13, `pip install uv==<pinned>`.
2. `bash tools/release_check.sh` (all steps above, with the zig compiler for the host tests).
3. `CXX=g++ bash tools/run_host_tests.sh` in `firmware/`: the host tests a second time with the
   runner's `g++`. Any compiler warning fails the test, so warnings that only one compiler reports
   are caught.

## `release.yml`

> [!WARNING]
> **Bench test before the first release**
>
> The tree has not been flashed to a device yet. Before the first version tag, run the
> [bench test protocol](bench-test-protocol.md) with an image from a manual run of this workflow,
> and push the tag only after it passed. A manual run with the input `bench_tested` left at
> `false` names every image `…-NOT-BENCH-TESTED`.

1. Reads `FW_VERSION` from `firmware/src/version.h`; on a tag, the tag must be `v` + that version.
2. Sets `PLATFORMIO_BUILD_FLAGS` to `-ffile-prefix-map` options, so the project code in the images
   holds `.` and `~/.platformio` instead of the runner paths. Images are built **only in CI**: a
   local build carries paths of your PC.
3. `bash tools/release_check.sh`: every check, and the build of all six environments. The transmit
   build `decoder_tx_wifi` is built only so `check_no_can_tx.sh` can confirm that it is the one
   program with `twai_transmit`.
4. Packs the five environments **without CAN transmit** (`listen`, `decoder`, `listen_wifi`,
   `decoder_wifi`, `listen_generic_wifi`) into `dist/`:
    - `<env>-<version>.bin`: the application image (update over Wi-Fi, or esptool at `0x10000`);
    - `<env>-<version>-merged.bin`: bootloader, partition table, `boot_app0` and application in one
      file written at address `0` (ESP Web Tools or one esptool command). The addresses come from
      PlatformIO (`pio project metadata`), merged by `firmware/tools/merge_image.py`, which refuses
      any environment with transmit.
5. Checks `dist/`: no file name with `tx`, `scan_private.py dist`, then writes `SHA256SUMS`.
6. Uploads `dist/` as a workflow artifact (kept 30 days). Turning it into a GitHub release is a
   manual step.

No image that can transmit on CAN is ever packed. Whoever wants to transmit builds
`decoder_tx_wifi` on purpose (see [Transmitting on the CAN bus](../safety/transmit.md)).

## Before you open a pull request

```bash
bash tools/release_check.sh
```

Without a C++ compiler on the PC the host tests use zig through `uvx`; CI adds the `g++` run.
