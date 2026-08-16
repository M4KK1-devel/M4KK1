#!/usr/bin/env python3
"""Minimal repro: boot, maximize win0 (via its max button), dump full
screen rows to see what the composite actually produced."""
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
        ddx = (tx - mx) // 2
        ddy = (ty - my) // 2
        cmd("mouse_move %d %d" % (ddx, ddy), 1.2)
        mx += 2 * ddx
        my += 2 * ddy
        return mx, my

    def shot():
        cmd("screendump /tmp/sp_max.ppm", 0.8)
        with open("/tmp/sp_max.ppm", "rb") as f:
            w, h, px = parse_ppm(f.read())
        return int(w), int(h), px

    def scan_rows(rows, label):
        w, h, px = shot()
        for y in rows:
            line = []
            for x in range(0, 800, 8):
                p = px_at(w, px, x, y)
                line.append("%d:%02x%02x%02x" % (x, p[0], p[1], p[2]))
            print(label + " y=%d: %s" % (y, " ".join(line)), flush=True)

    scan_rows([40, 100, 300, 560], "before")
    # click max button of win0 at (181,99)
    goto(181, 99)
    cmd("mouse_button 1", 0.4)
    cmd("mouse_button 0", 2.0)
    scan_rows([40, 100, 300, 560], "after max")
    # click max again to restore at (45,33)
    goto(45, 33)
    cmd("mouse_button 1", 0.4)
    cmd("mouse_button 0", 2.0)
    scan_rows([40, 100, 300, 560], "after restore")

    proc.kill()


if __name__ == "__main__":
    main()
