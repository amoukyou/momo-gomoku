#!/bin/sh
# Build the tethered SRAM test with the Jieli pi32v2 toolchain (run inside the Linux VM).
set -e
cd "$(dirname "$0")/.."
TC=/opt/jieli/pi32v2/bin
OUT=build/ramrun
mkdir -p "$OUT"
if [ ! -f src/font_data.c ]; then
    echo "game/src/font_data.c is missing: on the Mac run  swift game/tools/mkfont.swift game/src/font_data.c game/src/app.c" >&2
    exit 1
fi
CFLAGS="-target pi32v2 -integrated-as -mcpu=r3 -Oz -ffreestanding -fno-common -ffunction-sections -fdata-sections -Isrc -isystem $TC/../include -w"

objs=""
for f in ramrun/main.c ramrun/hal_ram.c ramrun/mini_libc.c \
         src/app.c src/fb.c src/gomoku.c src/font_data.c src/boot_anim.c src/epd_ssd1680.c; do
    o="$OUT/$(basename "$f" .c).o"
    "$TC/clang" $CFLAGS -c "$f" -o "$o"
    objs="$objs $o"
done

"$TC/ld" -T ramrun/ram.ld --gc-sections -o "$OUT/ramrun.elf" $objs "$TC/../lib/r3/libcompiler-rt.a"
# this objcopy wants explicit sections: dump each one, then lay them out by address
"$TC/objdump" -section-headers "$OUT/ramrun.elf" > "$OUT/sections.txt"
for s in .text .rodata .data; do "$TC/objcopy" -O binary -j $s "$OUT/ramrun.elf" "$OUT/sec$s.bin"; done
python3 - "$OUT" <<'PY'
import sys, os
out = sys.argv[1]
addrs = {}
for line in open(os.path.join(out, 'sections.txt')):
    f = line.split()
    if len(f) >= 4 and f[1] in ('.text', '.rodata', '.data'):
        addrs[f[1]] = int(f[3], 16)
base = addrs['.text']
img = bytearray()
for name in [n for n in ('.text', '.rodata', '.data') if n in addrs]:
    data = open(os.path.join(out, 'sec%s.bin' % name), 'rb').read()
    off = addrs[name] - base
    img += bytes(off - len(img)) if off > len(img) else b''
    img[off:off + len(data)] = data
    print('%-8s %#x  %6d bytes' % (name, addrs[name], len(data)))
open(os.path.join(out, 'ramrun.bin'), 'wb').write(img)
PY
sed -n '/Idx/,$p' "$OUT/sections.txt"
ls -l "$OUT/ramrun.bin"
