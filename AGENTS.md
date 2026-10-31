# 墨墨五子棋 — Baicizhan Ace-1S e-ink device turned into a Gomoku console

Read `docs/HANDOFF.md` (status, pitfalls, next steps) and `NOTES.md` (verified pinout, firmware format, recovery) first.

## Hardware (verified on the device)
- SoC Jieli AC7916AB (WL82 / AC791N), 320 MHz, 8 MB SDRAM; 16 MB SPI NOR flash; chipkey 0xF181.
- E-ink 2.7" 264x176, SSD1680: SCK PC09, MOSI PC10, CS PC06, DC PC07, RST PC08, BUSY PC05 (high = busy).
  Mounted upside down: use `EPD_FLIP_X | EPD_FLIP_Y` (run argument 3).
- Keys, active low with pull-ups: UP PC01, DOWN PA07, LEFT PH03, RIGHT PC00,
  big round key PA08 (confirm, also power-on), small round key PA10.
- PB01 is an ADC key line (external pull-up): top key ~0, side volume+ ~588, volume- ~693 (of 1023).
  Only the top key can be read as a digital input; the volume keys need the ADC.
- Watchdog: OR 0x40 into P33 register 0x80 (P33 bus CON 0x13e08 / BUF 0x13e0c). Soft reset: `*(u32 *)0x10000 |= 1 << 4`.
- Side pinhole = hardware reset button (always recovers to the firmware in flash).

## Layout
- `game/src/` portable game: `gomoku.c` (rules + AI), `app.c` (menus/UI), `fb.c` (1-bit drawing),
  `epd_ssd1680.c` (panel driver), `hal.h` (hardware interface), generated `font_data.c` / `boot_anim.c`.
- `game/ramrun/` tethered build that runs from SRAM in the ROM's USB download mode (`hal_ram.c`, `main.c`, `ram.ld`, `build.sh`).
- `game/sim/` host tests (`selfplay.c` AI self-play, `render.c` frame dumps) — run these on the Mac before the device.
- `game/art/boot_anim.html` + `game/tools/art_server.py` draw the animations and write `boot_anim.c`.
- `jlmac/` macOS libusb backend for kagaimiq's jl-uboot-tool (submodule in `third_party/`), plus `setup.sh`, `backup.sh`, `ramrun.sh`.
- `backup/` (local only, git-ignored) holds the full stock flash dump made with `jlmac/backup.sh`. Never modify or publish it.

## Build and run
- Fresh clone: `git clone --recursive` (jl-uboot-tool is a submodule), then `sh jlmac/setup.sh`.
- `game/src/font_data.c` is not in git (rendered from the Mac's PingFang font, not redistributable):
  generate it with `swift game/tools/mkfont.swift game/src/font_data.c game/src/app.c` before building.
- Compiler and SDK live in the OrbStack VM `jlbuild` (`/opt/jieli`, `~/sdk`). Build: `orb -m jlbuild bash -c "sh '$PWD/game/ramrun/build.sh'"`.
- After changing any UI text: `swift game/tools/mkfont.swift game/src/font_data.c game/src/app.c` (else new characters render as `?`).
- Load onto the device: `sudo ./jlmac/ramrun.sh 3` — needs the user's password, so ask the user to run it; do not run sudo yourself.
  The script ending with "Something failed ... timed out" is normal (the game never returns).

## Rules (each of these has bitten us)
- Do not write, erase or `burnchipkey` the flash with jl-uboot-tool. Burning the chipkey is irreversible (eFuse).
  Flash 0x0000-0x6000 holds the header, bootloader and boot config: corrupting it bricks the boot.
- The SRAM build must never return to the ROM: our RAM use breaks the ROM's USB state and the chip then only
  recovers with the pinhole reset. Exit = soft reset (`hal_reset`).
- Feed the watchdog in every long loop (`hal_delay_ms` does); without it the chip resets after ~2 s.
- Poll keys during long work (`hal_idle`); key presses are queued and debounced in `hal_ram.c`.
- No stdio/libc in the SRAM build (`ramrun/mini_libc.c` provides memset/memcpy/memcmp/strlen).
- Jieli objcopy needs explicit `-j` sections; `build.sh` exports sections and lays them out by address.
- A flashed (release) firmware must be built from the SDK with SDRAM enabled (no `CONFIG_NO_SDRAM_ENABLE`),
  must start USB update support (`TCFG_PC_UPDATE`) first thing, and needs explicit user approval before flashing.
