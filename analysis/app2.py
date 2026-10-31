import sys, crcmod
crc16 = crcmod.mkCrcFun(0x11021, initCrc=0, rev=False)
CK = 0xF181
def enc(b, key):
    b = bytearray(b)
    for i in range(len(b)):
        b[i] ^= key & 0xff
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xffff
    return bytes(b)
d = open(sys.argv[1], 'rb').read()
BASE = 0x6020
def dec_block(off):  # 32-byte block at absolute offset, SFC-style key
    return enc(d[off:off+32], (CK ^ ((off - BASE) >> 2)) & 0xffff)
def ent(off):
    e = dec_block(off)
    if crc16(e[2:32]) != int.from_bytes(e[0:2], 'little'): return None
    return e
n = 0
for off in range(BASE, BASE + 0x200000, 32):
    e = ent(off)
    if e and e[16] != 0xff:
        print('%#08x (rel %#07x) %-16s attr=%02x idx=%#06x off=%#x size=%#x dcrc=%04x' % (off, off-BASE, e[16:32].split(b'\0')[0].decode('latin1'), e[12], int.from_bytes(e[14:16],'little'), int.from_bytes(e[4:8],'little'), int.from_bytes(e[8:12],'little'), int.from_bytes(e[2:4],'little')))
        n += 1
        if n > 80: break
