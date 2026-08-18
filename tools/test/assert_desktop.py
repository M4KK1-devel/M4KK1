"""Pixel-level assertions on M4KK1 desktop screenshots (800x600 BGRA fb,
QEMU screendump PPM is RGB)."""
import struct
import sys
import zlib


def load_ppm(path):
    d = open(path, 'rb').read()
    parts = d.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3]


def px(pix, w, x, y):
    o = (y * w + x) * 3
    return (pix[o], pix[o + 1], pix[o + 2])


def near(c1, c2, tol=24):
    return all(abs(a - b) <= tol for a, b in zip(c1, c2))


def main():
    w, h, pix = load_ppm(sys.argv[1] if len(sys.argv) > 1 else 'shot1.ppm')
    print(f"size {w}x{h}")

    # 1. Menubar: rows 0..23 should be mostly light gray 0xCCCCCC
    mb = [px(pix, w, x, 4) for x in range(0, w, 8)]
    gray = sum(1 for c in mb if near(c, (0xCC, 0xCC, 0xCC)))
    print(f"menubar gray ratio: {gray}/{len(mb)}")

    # dark text pixels in menubar left (M4KK1 brand @ x=8..50, y~8)
    brand = sum(1 for x in range(6, 60) for y in range(4, 18)
                if sum(px(pix, w, x, y)) < 200)
    print(f"menubar brand dark pixels: {brand}")

    # center text "Desktop 1" @ ~x=370..430
    desk = sum(1 for x in range(360, 440) for y in range(4, 18)
               if sum(px(pix, w, x, y)) < 250)
    print(f"menubar center dark pixels: {desk}")

    # right clock text @ ~x=720..790
    clk = sum(1 for x in range(715, 795) for y in range(4, 18)
              if sum(px(pix, w, x, y)) < 250)
    print(f"menubar right dark pixels: {clk}")

    # 2. Dock: bottom 48 rows mostly dark 0x333333
    dk = [px(pix, w, x, h - 20) for x in range(0, w, 8)]
    dark = sum(1 for c in dk if near(c, (0x33, 0x33, 0x33), tol=40))
    print(f"dock dark ratio: {dark}/{len(dk)}")

    # dock top 1px light edge @ y = h-48
    edge = px(pix, w, 400, h - 48)
    print(f"dock top edge px: {edge}")

    # launcher icon @ x=760..792, y=h-40..h-8 (terminal icon: dark frame + green body 0x306030)
    green = sum(1 for x in range(758, 794) for y in range(h - 42, h - 6)
                if near(px(pix, w, x, y), (0x30, 0x60, 0x30), tol=50))
    print(f"launcher icon green pixels: {green}")

    # window icon (first window) @ x=8..40
    anyicon = sum(1 for x in range(8, 44) for y in range(h - 42, h - 6)
                  if not near(px(pix, w, x, y), (0x33, 0x33, 0x33), tol=40))
    print(f"dock first-slot non-bg pixels: {anyicon}")

    # 3. window area: any demo windows? count strongly-colored title bars
    for name, col in [("win1 maroon 0x803030", (0x80, 0x30, 0x30)),
                      ("win2 green 0x308030", (0x30, 0x80, 0x30)),
                      ("win3 navy 0x303080", (0x30, 0x30, 0x80))]:
        n = sum(1 for x in range(0, w, 4) for y in range(24, h - 48, 4)
                if near(px(pix, w, x, y), col, tol=40))
        print(f"{name}: {n} px")

    # 4. terminal window: dark blue-ish screen 0x080818 blocks
    term = sum(1 for x in range(0, w, 4) for y in range(24, h - 48, 4)
               if near(px(pix, w, x, y), (0x08, 0x08, 0x18), tol=24))
    print(f"terminal dark-screen px: {term}")


if __name__ == '__main__':
    main()
