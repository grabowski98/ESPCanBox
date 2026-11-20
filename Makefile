# Shortcuts for the usual checks. Run from the repository root (Git Bash on Windows, any shell elsewhere).
#   make test      host tests of lib/core against the test patterns (CXX=g++ make test to pick a compiler)
#   make build     compile all six PlatformIO environments (nothing is uploaded)
#   make check     safety and identity checks (after make build): no CAN transmit outside the tx build,
#                  identity in sync, private data scan, excluded files, image metadata, constant provenance
#   make release-check   every check in one go, the same as CI (tools/release_check.sh; builds included)
# Nothing here uploads firmware or opens a serial port.

FW := firmware
PIO ?= uvx platformio
PYTHON ?= python
ENVS := listen decoder listen_wifi decoder_wifi decoder_tx_wifi listen_generic_wifi

.PHONY: all test build check release-check

all: test build check

test:
	cd $(FW) && bash tools/run_host_tests.sh < /dev/null

build:
	cd $(FW) && $(PIO) run $(addprefix -e ,$(ENVS)) < /dev/null

check:
	cd $(FW) && bash tools/check_no_can_tx.sh
	$(PYTHON) -I tools/set_project_name.py --check
	$(PYTHON) -I tools/scan_private.py .
	$(PYTHON) -I tools/check_excluded.py .
	$(PYTHON) -I tools/strip_exif.py --check .
	$(PYTHON) -I tools/check_provenance.py --root .

release-check:
	bash tools/release_check.sh
