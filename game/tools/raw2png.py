#!/usr/bin/env python3
"""Convert 264x176 1-bit raw frames to PNG (scaled up) for previewing. usage: raw2png.py scale in.raw... """
import struct
import sys
import zlib

W, H = 264, 176


def png(path, rows, w, h):
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    raw = b''.join(b'\0' + bytes(r) for r in rows)
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 0, 0, 0, 0))
    data += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(data)


scale = int(sys.argv[1])
for path in sys.argv[2:]:
    fb = open(path, 'rb').read()
    rows = []
    for y in range(H):
        row = []
        for x in range(W):
            on = fb[y * (W // 8) + x // 8] & (0x80 >> (x % 8))
            row += [24 if on else 245] * scale
        rows += [row] * scale
    png(path[:-4] + '.png', rows, W * scale, H * scale)
