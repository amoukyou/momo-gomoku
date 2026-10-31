#!/bin/sh
# One-time setup on the Mac: Python venv with the USB tool's dependencies (needs Homebrew libusb).
cd "$(dirname "$0")/.." || exit 1
command -v brew >/dev/null && { brew list libusb >/dev/null 2>&1 || brew install libusb; }
python3 -m venv .venv
.venv/bin/pip install -q pyusb crcmod pyyaml pycryptodomex tqdm
echo "ok: .venv ready"
