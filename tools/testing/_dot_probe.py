#!/usr/bin/env python3
"""Check whether Copland composites keep running (bouncing-dot animation)
before and after a maximize click."""
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

    def shot():
        cmd("screendump /tmp/sp_dot.ppm", 0.8)
        with open("/tmp/sp_dot.ppm", "rb") as f:
            w, h, px = parse_ppm(f.read())
        return int(w), int(h), px

    def dot_samples(label):
        w, h, px = shot()
        row = []
        for y in (160, 200, 240):
            dots = [x for x in range(140, 400)
                    if px_at(w, px, x, y) == (32, 96, 32)]
            row.append("y%d:%d" % (y, len(dots)))
        print(label + " " + " ".join(row), flush=True)
        return row

    dot_samples("before t0")
    time.sleep(2)
    dot_samples("before t1")
    time.sleep(2)
    dot_samples("before t2")

    # click max on win0 (181,99): guest lands (182,100)
    cmd("mouse_move -109 -100", 1.2)
    cmd("mouse_button 1", 0.4)
    cmd("mouse_button 0", 1.5)

    dot_samples("after  max")
    time.sleep(2)
    dot_samples("after  max2")
    time.sleep(2)
    dot_samples("after  max3")

    # also sample the max title area
    w, h, px = shot()
    print("max title (10,40):", px_at(w, px, 10, 40), flush=True)
    print("max title (400,40):", px_at(w, px, 400, 40), flush=True)
    print("win0 old pos (300,100):", px_at(w, px, 300, 100), flush=True)

    proc.kill()


if __name__ == "__main__":
    main()
