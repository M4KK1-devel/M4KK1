#!/usr/bin/env python3
"""Minimal input probe: boot, screendump, send keys, screendump, diff.
Isolates whether HMP keyboard/mouse events reach the guest at all.
"""
import glob, os, socket, subprocess, sys, time

ISO = max(glob.glob("output/m4kk1_*.iso"), key=os.path.getmtime)
MON = "/tmp/m4k_probe_mon.sock"

def parse_ppm(data):
    parts = data.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    off = len(parts[0]) + 1 + len(parts[1]) + 1 + len(parts[2]) + 1
    return w, h, data[off: off + w * h * 3]

def diff(a, b):
    wa, ha, pa = parse_ppm(open(a, "rb").read())
    wb, hb, pb = parse_ppm(open(b, "rb").read())
    assert (wa, ha) == (wb, hb)
    n = 0
    for y in range(0, ha, 2):
        for x in range(0, wa, 2):
            o = (y * wa + x) * 3
            if pa[o:o+3] != pb[o:o+3]:
                n += 1
    return n

def main():
    subprocess.run(["rm", "-f", MON], check=False)
    proc = subprocess.Popen(
        ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
         "-display", "none", "-serial", "file:/tmp/probe_ser.log",
         "-monitor", "unix:%s,server=on,nowait" % MON],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("boot 30s...", flush=True)
    time.sleep(30)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(2)
    for _ in range(20):
        try:
            s.connect(MON); break
        except OSError:
            time.sleep(0.5)
    def cmd(line, wait=0.7):
        s.sendall(line.encode() + b"\n"); time.sleep(wait)

    cmd("screendump /tmp/probe_a.ppm", 1.0)
    # keyboard: type ls into whatever has focus
    for ch in "ls":
        cmd("sendkey %s" % ch, 0.4)
    cmd("sendkey ret", 1.0)
    time.sleep(3)
    cmd("screendump /tmp/probe_b.ppm", 1.0)
    print("keyboard diff px:", diff("/tmp/probe_a.ppm", "/tmp/probe_b.ppm"), flush=True)

    # mouse: big move should repaint cursor
    cmd("mouse_move 50 50", 1.5)
    cmd("screendump /tmp/probe_c.ppm", 1.0)
    print("mouse diff px:", diff("/tmp/probe_b.ppm", "/tmp/probe_c.ppm"), flush=True)

    proc.kill(); proc.wait()

if __name__ == "__main__":
    main()
