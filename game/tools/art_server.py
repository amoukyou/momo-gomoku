#!/usr/bin/env python3
"""
Serve game/art/ and accept the rendered boot animation frames from boot_anim.html.

usage: python3 art_server.py <port>
POST /save writes game/src/boot_anim.c (1-bit 264x176 frames, row-major, MSB first).
"""
import base64
import json
import os
import sys
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, '..', 'art')
OUT = os.path.join(HERE, '..', 'src', 'boot_anim.c')
FRAME_BYTES = 264 // 8 * 176


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=ART, **kw)

    def do_POST(self):
        if self.path != '/save':
            self.send_error(404)
            return
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        groups = [('boot', body['frames'], 'boot_anim_frames', 'BOOT_ANIM_FRAMES'),
                  ('sleep', body.get('sleep', []), 'sleep_anim_frames', 'SLEEP_ANIM_FRAMES'),
                  ('off', body.get('off', []), 'off_frames', 'OFF_FRAMES')]
        lines = ['/* Generated from art/boot_anim.html by tools/art_server.py -- do not edit. */',
                 '#include "boot_anim.h"', '']
        counts = []
        for name, b64s, array, count in groups:
            frames = [base64.b64decode(f) for f in b64s]
            if any(len(f) != FRAME_BYTES for f in frames):
                self.send_error(400, 'bad frame size')
                return
            for i, f in enumerate(frames):
                lines.append('static const uint8_t %s%d[%d] = {' % (name, i, FRAME_BYTES))
                for k in range(0, FRAME_BYTES, 24):
                    lines.append('    ' + ','.join('0x%02x' % b for b in f[k:k + 24]) + ',')
                lines.append('};')
            lines.append('const uint8_t *const %s[%s] = { %s };' %
                         (array, count, ', '.join('%s%d' % (name, i) for i in range(len(frames)))))
            lines.append('')
            counts.append('%s=%d' % (name, len(frames)))
        frames = counts
        with open(OUT, 'w') as fp:
            fp.write('\n'.join(lines))

        print('saved %s to %s (font_ok=%s)' % (' '.join(frames), OUT, body.get('font_ok')), flush=True)
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'ok')


if __name__ == '__main__':
    ThreadingHTTPServer(('127.0.0.1', int(sys.argv[1])), Handler).serve_forever()
