#!/bin/sh
# Load the game test into SRAM over USB and run it there (nothing is written to flash). Needs sudo.
# usage: sudo ./jlmac/ramrun.sh [arg]   arg bit0/1 = e-ink orientation (3 = this device: rotated 180)
cd "$(dirname "$0")/.." || exit 1
VENV=.venv
TOOL=third_party/jl-uboot-tool
JL_TIMEOUT_MS=30000 "$VENV/bin/python" -I jlmac/run_jl.py "$TOOL" --script jlrunner.py \
    --device jl:WL82 --encrypt --arg "${1:-0x10}" 0x1c02000 game/build/ramrun/ramrun.bin
