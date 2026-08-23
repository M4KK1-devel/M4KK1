#!/usr/bin/env python3
"""Interactive QEMU test: click the dock terminal launcher, type
`cptest`, and pixel-assert the Copland-protocol surface appears.

Pattern (see wm_interact_test.py): QEMU HMP mouse_move deltas are
doubled by the guest PS/2 stack (Sprach tracks x += 2*raw_dx, and the
kernel input path is the same), guest cursor starts at (400,300).
The dock terminal launcher icon sits at x=760..792, y=h-40..h-8
(center ~(776, 576) on 800x600).
"""
import glob
import os
import socket
import subprocess
import sys
import time

ISO = max(glob.glob("output/m4kk1_*.iso"), key=os.path.getmtime)
MON = "/tmp/m4k_cp_mon.sock"
SER = "/tmp/cptest_smoke.log"


def parse_ppm(data):
    parts = data.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    off = len(parts[0]) + 1 + len(parts[1]) + 1 + len(parts[2]) + 1
    return w, h, data[off: off + w * h * 3]


def main():
    subprocess.run(["rm", "-f", MON], check=False)
    proc = subprocess.Popen(
        [
            "qemu-system-i386",
            "-boot", "d",
            "-cdrom", ISO,
            "-m", "512",
            "-display", "none",
            "-serial", "file:" + SER,
            "-monitor", "unix:%s,server=on,nowait" % MON,
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    print("booting (28s)...", flush=True)
    time.sleep(28)
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
        print("FAIL: monitor connect failed", flush=True)
        proc.kill()
        sys.exit(1)

    def cmd(line, wait=0.7):
        s.sendall(line.encode() + b"\n")
        time.sleep(wait)

    # ---- 1. desktop sanity ----
    cmd("screendump /tmp/cp_desk.ppm", 1.0)
    r = subprocess.run(
        [sys.executable, "tools/test/assert_desktop.py", "/tmp/cp_desk.ppm"],
        capture_output=True, text=True)
    tail = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else "?"
    print("desktop assert: %s" % tail, flush=True)

    # ---- 2. click the dock terminal launcher ----
    # guest cursor (400,300) -> launcher center (776,576): raw = /2
    ddx = (776 - 400) // 2
    ddy = (576 - 300) // 2
    cmd("mouse_move %d %d" % (ddx, ddy), 1.5)
    cmd("mouse_button 1", 0.4)
    cmd("mouse_button 0", 2.5)
    print("clicked dock launcher", flush=True)

    # ---- 3. type cptest ----
    for ch in "cptest":
        cmd("sendkey %s" % ch, 0.3)
    cmd("sendkey ret", 0.8)
    print("typed cptest, waiting 18s...", flush=True)
    time.sleep(18)

    # ---- 4. pixel-assert the protocol surface ----
    cmd("screendump /tmp/cp_surf.ppm", 1.0)
    r = subprocess.run(
        [sys.executable, "tools/test/assert_cptest.py", "/tmp/cp_surf.ppm"])
    rc = r.returncode

    # ---- 5. serial evidence ----
    try:
        with open(SER, "rb") as f:
            data = f.read().decode("utf-8", "replace")
        cp_lines = [l for l in data.splitlines() if "CPTEST" in l]
        print("serial CPTEST lines (%d):" % len(cp_lines), flush=True)
        for l in cp_lines:
            print("  " + l, flush=True)
        if not cp_lines:
            print("  (none - cptest produced no serial output)", flush=True)
    except OSError:
        pass

    proc.kill()
    proc.wait()
    sys.exit(rc)


if __name__ == "__main__":
    main()
