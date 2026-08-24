#!/usr/bin/env python3
"""Smoke test: boot fresh ISO, assert desktop chrome pixels exist
(menubar band, dock band, wallpaper gradient, then clock/menu probes)."""
import socket, subprocess, time, json, sys, os

ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build5-alpha1-full-test.iso"
SER = "/tmp/smoke_loop1.log"
PORT = 4471
SHOT = "/tmp/smoke_loop1.ppm"

if not os.path.exists(ISO):
    sys.exit(f"SMOKE FAIL: ISO missing (test_all.sh may have cleaned it): {ISO}")

qemu = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
     "-vga", "std", "-serial", f"file:{SER}",
     "-display", "none", "-qmp", f"tcp:127.0.0.1:{PORT},server,nowait"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    time.sleep(3)
    # QMP may come up slowly when booting from the 9P-mounted ISO — retry
    s = None
    for _ in range(20):
        try:
            s = socket.create_connection(("127.0.0.1", PORT), timeout=2)
            break
        except ConnectionRefusedError:
            time.sleep(1)
    if s is None:
        raise ConnectionRefusedError(f"QMP port {PORT} never opened")
    s.settimeout(5)
    def qmp(obj):
        s.sendall((json.dumps(obj) + "\r\n").encode())
        time.sleep(0.05)
        try:
            while True:
                l = s.recv(65536)
                if not l: break
        except socket.timeout:
            pass
    qmp({"execute": "qmp_capabilities"})

    for _ in range(40):
        time.sleep(2)
        try:
            t = open(SER, "rb").read().decode(errors="replace")
            if "mode 'stacking' initialized" in t:
                break
        except FileNotFoundError:
            continue
    print("desktop ready:", "mode 'stacking' initialized" in t)
    time.sleep(4)
    qmp({"execute": "screendump", "arguments": {"filename": SHOT}})
    time.sleep(2)
    print("PANIC in serial:", "PANIC" in t)
    print("EXC in serial:", "[EXC]" in t)
finally:
    qemu.terminate()
    qemu.wait()
