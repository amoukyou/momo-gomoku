#!/usr/bin/env python3
"""
Run kagaimiq's jl-uboot-tool on macOS, using the libusb backend in ./scsiio.

usage: sudo python3 -I run_jl.py <path/to/jl-uboot-tool> [--script NAME] [--out DIR] [tool args...]

--script NAME: tool script to run (default jluboottool.py; e.g. jlrunner.py)
--out DIR: files in DIR are chowned back to the invoking user afterwards (sudo writes them as root).
"""
import os
import sys
import runpy

here = os.path.dirname(os.path.abspath(__file__))
tool = os.path.abspath(sys.argv[1])
rest = sys.argv[2:]

script, outdir = 'jluboottool.py', None
while rest[:1] in (['--script'], ['--out']):
    if rest[0] == '--script':
        script = rest[1]
    else:
        outdir = rest[1]
    rest = rest[2:]

sys.path[:0] = [here, tool]
sys.argv = [os.path.join(tool, script)] + rest

try:
    runpy.run_path(sys.argv[0], run_name='__main__')
except SystemExit:
    pass
finally:
    uid, gid = os.environ.get('SUDO_UID'), os.environ.get('SUDO_GID')
    if outdir and uid and gid:
        for name in os.listdir(outdir):
            os.chown(os.path.join(outdir, name), int(uid), int(gid))
