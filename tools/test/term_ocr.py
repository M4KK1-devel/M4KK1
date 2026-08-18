"""Glyph-template OCR for the M4KK1 terminal screenshot (fixed matcher)."""
import re
import sys


def load_font(path):
    src = open(path, encoding='utf-8').read()
    glyphs = {}
    pat = re.compile(r'/\* (?:0x[0-9A-Fa-f]{2}|.) \*/ \{([^}]*)\}')
    chars = [chr(c) for c in range(0x20, 0x7F)]
    for i, m in enumerate(pat.finditer(src)):
        if i >= len(chars):
            break
        rows = [int(x, 16) for x in m.group(1).split(',')]
        glyphs[chars[i]] = rows
    return glyphs


def load_ppm(path):
    d = open(path, 'rb').read()
    parts = d.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3]


def ocr_term(path, cols, rows, ox, oy, cw, ch):
    glyphs = load_font('usr/src/cmd/term_font_8x16.h')
    w, h, pix = load_ppm(path)
    out = []
    for r in range(rows):
        line = ''
        for c in range(cols):
            gx = ox + c * cw
            gy = oy + r * ch
            # sample the cell bitmap first
            cell = []
            for yy in range(16):
                rowbits = 0
                for xx in range(8):
                    p = pix[((gy + yy) * w + gx + xx) * 3:
                            ((gy + yy) * w + gx + xx) * 3 + 3]
                    if p[0] > 140 and p[1] > 140 and p[2] > 140:
                        rowbits |= 0x80 >> xx
                cell.append(rowbits)
            # exact match first, then best hamming distance
            best, best_d = '?', 999
            for ch_, rows_ in glyphs.items():
                d = sum(bin(a ^ b).count('1')
                        for a, b in zip(cell, rows_))
                if d < best_d:
                    best, best_d = ch_, d
                if d == 0:
                    best, best_d = ch_, 0
                    break
            line += best
        out.append(line.rstrip())
    return out


if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else 't3.ppm'
    ox = int(sys.argv[2]) if len(sys.argv) > 2 else 80
    oy = int(sys.argv[3]) if len(sys.argv) > 3 else 64
    lines = ocr_term(path, 80, 25, ox, oy, 8, 16)
    for i, l in enumerate(lines):
        print('%2d|%s' % (i, l))
