#!/usr/bin/env python3
"""Assert the cptest protocol surface is on screen.

The cptest client commits a 128x128 buffer filled with
0xFF0000FF | (x & 127) << 8  (BGRA-ish 32bpp little endian:
per-pixel = 0xFF | gradient<<8 | 0<<16 | 0xFF<<24).

In memory a pixel dword is 0xFF00xxFF? No - we wrote
pool_mem[i] = 0xFF0000FF | ((i & 127) << 8):
  byte0=0xFF (B), byte1=gradient (G), byte2=0x00 (R), byte3=0xFF.
So the visible color is BLUE channel 255, GREEN = gradient,
RED = 0: a blue->cyan gradient column pattern.

Reads the QEMU screendump PPM (RGB) and checks the top-left
128x128 region for blue-dominant pixels with a green ramp.
"""
import sys

def main(path):
    with open(path, 'rb') as f:
        data = f.read()
    # parse P6 header
    if data[:2] != b'P6':
        print("FAIL: not P6"); return 1
    idx = 2
    fields = []
    while len(fields) < 3:
        while idx < len(data) and data[idx:idx+1].isspace():
            idx += 1
        if data[idx:idx+1] == b'#':
            while data[idx:idx+1] not in (b'\n', b''):
                idx += 1
            continue
        start = idx
        while not data[idx:idx+1].isspace():
            idx += 1
        fields.append(int(data[start:idx]))
    idx += 1
    w, h, maxv = fields
    px = data[idx:]

    def rgb(x, y):
        o = (y * w + x) * 3
        return px[o], px[o+1], px[o+2]

    # sample the gradient: at (x, 64) expect R=0, G=x, B=255
    hits = 0
    for x in (8, 32, 64, 96, 120):
        r, g, b = rgb(x, 64)
        if r < 40 and b > 200 and abs(g - x) <= 12:
            hits += 1
    print(f"gradient hits: {hits}/5")
    if hits >= 4:
        print("CPTEST PIXEL ASSERT PASS: protocol surface composited")
        return 0
    # debug dump a few pixels
    for x in (0, 16, 64, 120):
        print("  px(%d,64) =" % x, rgb(x, 64))
    print("CPTEST PIXEL ASSERT FAIL")
    return 1

if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
