import sys, crcmod
crc16 = crcmod.mkCrcFun(0x11021, initCrc=0, rev=False)
def enc(b, key=0xffff):
    b = bytearray(b)
    for i in range(len(b)):
        b[i] ^= key & 0xff
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xffff
    return bytes(b)
d = open(sys.argv[1], 'rb').read()
def walk(base, depth=0, maxn=64):
    off = base + (32 if depth == 0 else 0)
    for n in range(maxn):
        e = enc(d[off:off+32])
        ok = crc16(e[2:32]) == int.from_bytes(e[0:2], 'little')
        if not ok:
            print('  '*depth + '  [%08x] stop (crc bad)' % off); return
        dcrc = int.from_bytes(e[2:4], 'little'); o = int.from_bytes(e[4:8], 'little'); sz = int.from_bytes(e[8:12], 'little')
        attr = e[12]; idx = int.from_bytes(e[14:16], 'little'); name = e[16:32].split(b'\0')[0].decode('latin1')
        print('  '*depth + '  [%08x] %-16s attr=%02x idx=%d off=%#x size=%#x dcrc=%04x' % (off, name, attr, idx, o, sz, dcrc))
        off += 32
        if idx: return
walk(0)
print('--- app_dir_head @0x6020')
walk(0x6020, 1, 200)
