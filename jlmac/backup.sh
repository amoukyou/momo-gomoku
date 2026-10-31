#!/bin/sh
# Read-only backup of the Ace-1S 16 MB SPI flash. Needs sudo (USB access on macOS).
# Puts the chip into Jieli's ROM download mode, loads the read loader into RAM,
# reads the flash, then resets the chip back to the normal firmware.
cd "$(dirname "$0")/.." || exit 1
VENV=.venv
TOOL=third_party/jl-uboot-tool
"$VENV/bin/python" -I jlmac/run_jl.py "$TOOL" --out backup --device jl:WL82 \
    "dump 0 0x100" \
    "read 0 0x1000000 backup/ace1s_spiflash_16M.bin" \
    "reset" 2>&1 | tee backup/backup_log.txt
