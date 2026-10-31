import sys
CK = 0xF181
BASE = 0x6020
def enc_inplace(b, off, n, key):
    for i in range(off, off + n):
        b[i] ^= key & 0xff
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xffff
d = open(sys.argv[1], 'rb').read()
end = 0x490000
img = bytearray(d[BASE:end])
for rel in range(0, len(img), 32):
    enc_inplace(img, rel, min(32, len(img) - rel), (CK ^ (rel >> 2)) & 0xffff)
open(sys.argv[2], 'wb').write(img)
print('wrote', len(img))
