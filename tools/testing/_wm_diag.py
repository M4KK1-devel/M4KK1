#!/usr/bin/env python3
"""Diagnose WM interaction state with full-row pixel dumps."""
import glob
import os
import socket
import subprocess
import sys
import time

ISO = max(glob.glob("output/m4kk1_*.iso"), key=os.path.getmtime)
MON = "/tmp/m4k_mon.sock"


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
    for _ in range(30):
        try:
            s.connect(MON)
            break
        except OSError:
            time.sleep(0.5)

    def cmd(line, wait=0.7):
        s.sendall(line.encode() + b"\n")
        time.sleep(wait)

    mx, my = 400, 300

    def goto(tx, ty):
        nonlocal mx, my
        # Sprach: x += 2*raw_dx, y -= 2*raw_dy; QEMU sends raw (dx,-dy)
        ddx = (tx - mx) // 2
        ddy = (ty - my) // 2
        cmd("mouse_move %d %d" % (ddx, ddy), 1.2)
        mx += 2 * ddx
        my += 2 * ddy
        return mx, my

    def click():
        cmd("mouse_button 1", 0.4)
        cmd("mouse_button 0", 1.3)

    def shot():
        cmd("screendump /tmp/sp_diag.ppm", 0.8)
        with open("/tmp/sp_diag.ppm", "rb") as f:
            w, h, px = parse_ppm(f.read())
        return int(w), int(h), px

    def dump_row(y, label):
        w, h, px = shot()
        print(label + " y=%d: " % y, end="", flush=True)
        for x in range(0, 800, 4):
            p = px_at(w, px, x, y)
            print("%d:%02x%02x%02x" % (x, p[0], p[1], p[2]), end=" ", flush=True)
        print(flush=True)

    def rowscan(y):
        w, h, px = shot()
        return [px_at(w, px, x, y) for x in range(800)]

    def spans(row, target):
        out = []
        x = 0
        while x < 800:
            if row[x] == target:
                x0 = x
                while x < 800 and row[x] == target:
                    x += 1
                out.append((x0, x - 1))
            else:
                x += 1
        return out

    dump_row(176, "before: title row")
    dump_row(585, "before: taskbar row")

    # click close of win2: screen (233,179)
    goto(233, 179)
    click()

    row = rowscan(176)
    print("after close click:")
    print("  title row 176 T2 spans:", spans(row, (48, 48, 128)))
    print("  title row 176 BODY spans:", spans(row, (208, 208, 208)))
    row = rowscan(585)
    print("  taskbar 585 ACCENT spans:", spans(row, (255, 208, 96)))
    print("  taskbar 585 BTN spans:", spans(row, (64, 64, 96)))
    print("  taskbar 585 TBG spans:", spans(row, (32, 32, 48)))
    dump_row(300, "after: mid row")

    # click min of win1: (207,139)
    goto(207, 139)
    click()
    row = rowscan(136)
    print("after min click on win1:")
    print("  title row 136 T1 spans:", spans(row, (48, 128, 48)))
    print("  title row 136 BODY spans:", spans(row, (208, 208, 208)))
    dump_row(585, "taskbar after min")

    # taskbar btn1: (156,585)
    goto(156, 585)
    click()
    row = rowscan(136)
    print("after taskbar btn1 click:")
    print("  title row 136 T1 spans:", spans(row, (48, 128, 48)))

    proc.kill()


if __name__ == "__main__":
    main()
