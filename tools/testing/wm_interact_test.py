#!/usr/bin/env python3
"""Live-interactive test: drive the Sprach WM via QEMU monitor mouse
commands and verify with screendump pixel sampling.

QEMU `mouse_move dx dy` = screen deltas 1:1 (guest cursor lands at the
intended screen position).  Sprach tracks mouse_x += dx, mouse_y += dy
starting at (400,300).

Windows (stacking mode): w0(140,90) title(128,48,48),
w1(180,130) title(48,128,48), w2(220,170) title(48,48,128) - w2 on top.
Title bar height 18.  Buttons local: close(8,4), min(22,4), max(36,4), 10x10.
Taskbar y 570..600; button i at x = 4 + i*(96+8), w96.
"""
import glob
import os
import socket
import subprocess
import sys
import time

ISO = max(glob.glob("output/m4kk1_*.iso"), key=os.path.getmtime)
MON = "/tmp/m4k_mon.sock"

DESKTOP = (31, 62, 155)
ACCENT = (255, 208, 96)
TB_BG = (32, 32, 48)
TB_BTN = (64, 64, 96)
T0, T1, T2 = (128, 48, 48), (48, 128, 48), (48, 48, 128)


def parse_ppm(data):
    parts = data.split(b"\n", 3)
    w, h = parts[1].split()
    w, h = int(w), int(h)
    off = len(parts[0]) + 1 + len(parts[1]) + 1 + len(parts[2]) + 1
    return w, h, data[off: off + w * h * 3]


def px_at(w, px, x, y):
    i = (y * w + x) * 3
    return px[i], px[i + 1], px[i + 2]


def main():
    subprocess.run(["rm", "-f", MON], check=False)
    proc = subprocess.Popen(
        [
            "qemu-system-i386",
            "-boot", "d",
            "-cdrom", ISO,
            "-m", "512",
            "-display", "none",
            "-serial", "file:/tmp/sprach_serial.log",
            "-monitor", "unix:%s,server=on,nowait" % MON,
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    print("booting...", flush=True)
    time.sleep(14)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(2)
    ok = False
    for _ in range(30):
        try:
            s.connect(MON)
            ok = True
            break
        except OSError:
            time.sleep(0.5)
    if not ok:
        print("monitor connect failed", flush=True)
        proc.kill()
        sys.exit(1)

    def cmd(line, wait=0.7):
        s.sendall(line.encode() + b"\n")
        time.sleep(wait)

    def shot():
        cmd("screendump /tmp/sp_cur.ppm", 0.8)
        with open("/tmp/sp_cur.ppm", "rb") as f:
            return parse_ppm(f.read())

    results = []

    def check(name, ok, detail=""):
        results.append((name, ok))
        print(("PASS" if ok else "FAIL") + ": " + name + ("  " + detail if detail else ""), flush=True)

    mx, my = 400, 300

    def goto(tx, ty):
        nonlocal mx, my
        # Sprach: x += 2*raw_dx, y -= 2*raw_dy; QEMU mouse_move sends
        # raw (dx, -dy).  So to land at (tx,ty): ddx=(tx-mx)/2, ddy=(ty-my)/2.
        ddx = (tx - mx) // 2
        ddy = (ty - my) // 2
        cmd("mouse_move %d %d" % (ddx, ddy), 1.2)
        mx += 2 * ddx
        my += 2 * ddy

    def click():
        cmd("mouse_button 1", 0.4)
        cmd("mouse_button 0", 2.2)

    # ---- 1. close window 2 (active, top) ----
    # Sample points: (450,176) is win2's title FILL (322..474); after the
    # close it shows the desktop (win2's x range 220..476, win1 ends at 436).
    w, h, px = shot()
    p0 = px_at(w, px, 450, 176)
    goto(233, 179)               # close btn center
    click()
    w, h, px = shot()
    p1 = px_at(w, px, 450, 176)
    check("close: win2 title gone", p0 == T2 and p1 != T2, "%s -> %s" % (p0, p1))
    p_tb = px_at(w, px, 260, 590)  # taskbar btn2 (cursor is far away)
    check("taskbar: btn2 removed after close", p_tb == TB_BG, "btn2 px %s" % (p_tb,))

    # ---- 2. minimize window 1 ----
    w, h, px = shot()
    p0 = px_at(w, px, 400, 136)  # win1 title fill
    goto(207, 139)               # min btn
    click()
    w, h, px = shot()
    p1 = px_at(w, px, 400, 136)
    check("min: win1 title hidden", p0 == T1 and p1 != T1, "%s -> %s" % (p0, p1))
    p_tb = px_at(w, px, 156, 590)  # btn1 must REMAIN (minimized window)
    check("taskbar: btn1 remains (minimized)", p_tb in (ACCENT, TB_BTN), "btn1 px %s" % (p_tb,))

    # ---- 3. restore win1 via taskbar button 1 ----
    w, h, px = shot()
    p0 = px_at(w, px, 400, 136)
    goto(156, 585)               # taskbar btn1 center
    click()
    w, h, px = shot()
    p1 = px_at(w, px, 400, 136)
    check("taskbar click restores win1", p1 == T1, "title px %s" % (p1,))

    # ---- 4. maximize window 0 ----
    w, h, px = shot()
    p0 = px_at(w, px, 10, 40)    # desktop (maximized title starts at y=24)
    goto(181, 99)                # max btn
    click()
    w, h, px = shot()
    p1 = px_at(w, px, 10, 40)
    check("max: win0 fills work area", p0 == DESKTOP and p1 == T0, "%s -> %s" % (p0, p1))

    # ---- 5. restore window 0 ----
    w, h, px = shot()
    p0 = px_at(w, px, 10, 40)
    goto(45, 33)                 # max btn (now at top-left of full-width title bar)
    click()
    w, h, px = shot()
    p1 = px_at(w, px, 10, 40)
    check("max toggle restores win0", p0 == T0 and p1 == DESKTOP, "%s -> %s" % (p0, p1))

    proc.kill()
    print("\n=== SUMMARY ===")
    for n, okk in results:
        print(("  PASS " if okk else "  FAIL ") + n)
    print("Total: %d  Failed: %d" % (len(results), len(results) - sum(1 for _, o in results if o)))


if __name__ == "__main__":
    main()
