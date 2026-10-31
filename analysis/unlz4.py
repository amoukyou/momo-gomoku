import sys
def lz4_block(src, out_size):
    dst = bytearray(); i = 0
    while i < len(src) and len(dst) < out_size:
        tok = src[i]; i += 1
        n = tok >> 4
        if n == 15:
            while True:
                b = src[i]; i += 1; n += b
                if b != 255: break
        dst += src[i:i+n]; i += n
        if len(dst) >= out_size or i >= len(src): break
        off = src[i] | (src[i+1] << 8); i += 2
        m = tok & 15
        if m == 15:
            while True:
                b = src[i]; i += 1; m += b
                if b != 255: break
        m += 4
        if off == 0 or off > len(dst): raise ValueError('bad offset %d at %d (out %d)' % (off, i, len(dst)))
        for _ in range(m): dst.append(dst[-off])
    return bytes(dst), i
img = open(sys.argv[1], 'rb').read()
lz = img[0x120:0x120+0x1319a4]
hdr = [int.from_bytes(lz[k:k+4], 'little') for k in range(0, 16, 4)]
print('header', [hex(x) for x in hdr])
out, used = lz4_block(lz[16:], hdr[3])
print('decompressed', len(out), 'consumed', used, 'of', len(lz) - 16)
open(sys.argv[2], 'wb').write(out)
