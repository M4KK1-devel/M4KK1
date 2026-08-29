#!/usr/bin/env python3
"""C vs Zig pixel-primitive equivalence test.

Builds a tiny host harness: one C file calling the C primitives
(sp_fill/sp_rect/sp_draw_char/sp_draw_str from sprach.c) and one
calling the Zig exports (zsp_*), each dumping a pixel buffer to
stdout; compares byte streams. Run: zig_equivalence_test.py
"""
import os, subprocess, sys

os.chdir("/mnt/f/M4KK1")
ZIG = os.path.expanduser("~/zig-0.13.0/zig")
W = 64; H = 32

HARNESS = r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* C side: verbatim semantics of sprach.c primitives */
static void c_sp_px(uint32_t *buf, int bw, int bh, int x, int y, uint32_t c)
{ if (x >= 0 && x < bw && y >= 0 && y < bh) buf[y * bw + x] = c; }

static void c_sp_fill(uint32_t *buf, int n, uint32_t c)
{ for (int i = 0; i < n; i++) buf[i] = c; }

static void c_sp_rect(uint32_t *buf, int bw, int bh, int x, int y,
                      int rw, int rh, uint32_t c)
{
    int x0 = x > 0 ? x : 0, y0 = y > 0 ? y : 0;
    int x1 = x + rw, y1 = y + rh;
    if (x1 > bw) x1 = bw;
    if (y1 > bh) y1 = bh;
    for (int py = y0; py < y1; py++)
        for (int px = x0; px < x1; px++)
            buf[py * bw + px] = c;
}

extern const unsigned char font5x7[95][7];

static void c_sp_draw_char(uint32_t *buf, int bw, int bh, int x, int y,
                           char ch, uint32_t fg)
{
    if (ch < 0x20 || ch > 0x7E) return;
    int idx = ch - 0x20;
    for (int row = 0; row < 7; row++) {
        unsigned char bits = font5x7[idx][row];
        for (int col = 0; col < 5; col++)
            if (bits & (1 << (4 - col)))
                c_sp_px(buf, bw, bh, x + col, y + row, fg);
    }
}

static void c_sp_draw_str(uint32_t *buf, int bw, int bh, int x, int y,
                          const char *s, uint32_t fg)
{ while (*s) { c_sp_draw_char(buf, bw, bh, x, y, *s, fg); x += 6; s++; } }

/* Zig side */
extern void zsp_fill(uint32_t *buf, int n, uint32_t c);
extern void zsp_rect(uint32_t *buf, int bw, int bh, int x, int y,
                     int rw, int rh, uint32_t c);
extern void zsp_draw_char(uint32_t *buf, int bw, int bh, int x, int y,
                          unsigned char ch, uint32_t fg);
extern void zsp_draw_str(uint32_t *buf, int bw, int bh, int x, int y,
                         const char *s, uint32_t fg);

int main(int argc, char **argv)
{
    int mode = atoi(argv[1]);   /* 0 = C, 1 = Zig */
    static uint32_t buf[%(W)d * %(H)d];
    memset(buf, 0, sizeof(buf));

    if (mode == 0) {
        c_sp_fill(buf, 64, 0x11223344);
        c_sp_rect(buf, %(W)d, %(H)d, -5, -3, 20, 10, 0xFF0000);
        c_sp_rect(buf, %(W)d, %(H)d, 50, 25, 30, 30, 0x00FF00);
        c_sp_draw_str(buf, %(W)d, %(H)d, 3, 15, "M4KK1 zig!", 0xFFFFFF);
    } else {
        zsp_fill(buf, 64, 0x11223344);
        zsp_rect(buf, %(W)d, %(H)d, -5, -3, 20, 10, 0xFF0000);
        zsp_rect(buf, %(W)d, %(H)d, 50, 25, 30, 30, 0x00FF00);
        zsp_draw_str(buf, %(W)d, %(H)d, 3, 15, "M4KK1 zig!", 0xFFFFFF);
    }
    fwrite(buf, sizeof(uint32_t), %(W)d * %(H)d, stdout);
    return 0;
}
""" % {"W": W, "H": H}

FONT_SRC = r"""
/* Reference 5x7 font (subset semantics identical to sprach.c table).
   96 glyphs; 'M','4','K','1',' ','z','i','g','!' plus fillers. */
#include <stdint.h>
"""

def main():
    if not os.path.exists(ZIG):
        print("zig toolchain missing:", ZIG); return 2

    open("/tmp/zt_harness.c", "w").write(HARNESS)

    # Zig object: host target for the equivalence test
    r = subprocess.run([ZIG, "build-obj", "usr/src/sprach/spr_draw.zig",
                        "-O", "ReleaseFast",
                        "-femit-bin=/tmp/zt_spr_draw.o"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print("zig build-obj FAIL:\n", r.stderr); return 2

    # font object: compile the REAL font table from sprach.c via a shim
    shim = r"""
#include <stdint.h>
%s
""" % open("usr/src/sprach/sprach.c").read().split("static const unsigned char font5x7")[1].split(";")[0] + ";"
    # extract the full array incl. initializer
    src = open("usr/src/sprach/sprach.c").read()
    start = src.index("font5x7")
    end = src.index("};", start)
    arr = "static const unsigned char " + src[start:end+2]
    arr = arr.replace("static const unsigned char", "const unsigned char", 1)
    open("/tmp/zt_font.c", "w").write("#include <stdint.h>\n" + arr)

    for cc in ("gcc", "cc"):
        r = subprocess.run([cc, "-o", "/tmp/zt_c", "/tmp/zt_harness.c",
                            "/tmp/zt_font.c", "/tmp/zt_spr_draw.o",
                            "-fno-lto", "-no-pie"],
                           capture_output=True, text=True)
        r2 = subprocess.run([cc, "-o", "/tmp/zt_z", "/tmp/zt_harness.c",
                             "/tmp/zt_font.c", "/tmp/zt_spr_draw.o",
                             "-fno-lto", "-no-pie"],
                            capture_output=True, text=True)
        if r.returncode == 0 and r2.returncode == 0:
            break
    else:
        print("compile FAIL:\nC:", r.stderr, "\nZ:", r2.stderr); return 2

    cbuf = subprocess.run(["/tmp/zt_c", "0"], capture_output=True).stdout
    zbuf = subprocess.run(["/tmp/zt_z", "1"], capture_output=True).stdout

    if cbuf == zbuf:
        print("EQUIVALENCE: PASS (%d bytes identical)" % len(cbuf))
        return 0
    diffs = sum(1 for a, b in zip(cbuf, zbuf) if a != b)
    print("EQUIVALENCE: FAIL (%d/%d bytes differ)" % (diffs, len(cbuf)))
    for i, (a, b) in enumerate(zip(cbuf, zbuf)):
        if a != b:
            print("  first diff at byte %d (px %d): C=%02x Z=%02x"
                  % (i, i // 4, a, b))
            break
    return 1

if __name__ == "__main__":
    raise SystemExit(main())
