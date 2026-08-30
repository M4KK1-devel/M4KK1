#!/usr/bin/env python3
"""wheel_repro.py — verify mouse-wheel scrollback in the terminal.

Chain (all drag_repro-verified): boot full-test ISO → single-click the
desktop terminal icon → type enough lines to fill the scrollback →
wheel up via HMP `mouse_move 0 0 -1` → screendump before/after and
assert the viewport shifted (top rows changed).
"""
import subprocess, time, sys, os

ISO = "output/m4kk1_0.0.1_build8-alpha1-full-test.iso"
SER = "logs/wheel_serial.log"
A = "/tmp/wheel_a.ppm"
B = "/tmp/wheel_b.ppm"

def hmp(mon, cmd):
    mon.stdin.write((cmd + "\n").encode())
    mon.stdin.flush()
    time.sleep(0.3)

def read_ppm(path):
    data = open(path, "rb").read()
    parts = data.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3]

def main():
    if not os.path.exists(ISO):
        print("[wheel] ISO missing"); return 1
    for p in (SER,):
        try: os.remove(p)
        except FileNotFoundError: pass

    qemu = subprocess.Popen(
        ["qemu-system-i386", "-cdrom", ISO, "-m", "512",
         "-vga", "std", "-serial", f"file:{SER}", "-monitor", "stdio",
         "-display", "none"],
        stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL)
    time.sleep(2)
    mon = qemu

    for _ in range(40):
        try:
            if b"Entering main loop" in open(SER, "rb").read(): break
        except (FileNotFoundError, OSError):
            pass
        time.sleep(1)
    time.sleep(6)

    # single-click terminal icon (2nd grid cell)
    hmp(mon, "mouse_move -272 -222")
    time.sleep(1.0)
    hmp(mon, "mouse_button 1"); time.sleep(0.2)
    hmp(mon, "mouse_button 0"); time.sleep(6)

    # generate >25 lines of output via the serial shell? No — the
    # desktop terminal gets keyboard via mailbox. Use sendkey to run
    # `seq 1 40` … typing is slow. Faster: the boot banner already
    # scrolled. Just fill with Enters: each ret prints a prompt line.
    # Move focus to the terminal window (click on its body first).
    # terminal spawns at (60,40); body click at (200,200) focuses it
    # (click-through forwards to terminal as an input event).
    hmp(mon, "mouse_move -200 -100")   # (400,300)->(200,200)
    time.sleep(0.5)
    hmp(mon, "mouse_button 1"); time.sleep(0.2)
    hmp(mon, "mouse_button 0"); time.sleep(1)

    # type "yes" piped output: use `dmesg`-ish… simplest builtin that
    # prints many lines: `help` (lists all commands) or `man ls`.
    # m4sh builtins table prints ~30 lines with `help`.
    for ch in "help":
        hmp(mon, f"sendkey {ch}")
    hmp(mon, "sendkey ret")
    time.sleep(4)

    hmp(mon, f"screendump {A}")
    time.sleep(1)
    # wheel up one notch
    hmp(mon, "mouse_move 0 0 -1")
    time.sleep(1.5)
    hmp(mon, f"screendump {B}")
    time.sleep(1)
    qemu.kill()

    serial = open(SER, "rb").read()
    wa, ha, pa = read_ppm(A)
    wb, hb, pb = read_ppm(B)
    # terminal body region: x in [60,740), y in [58,496) approx.
    # Compare rows in the middle band of the window.
    def sig(px, w, x0, x1, y0, y1):
        s = 0
        for y in range(y0, y1, 2):
            for x in range(x0, x1, 8):
                i = (y * w + x) * 3
                s = (s * 31 + px[i] + px[i+1] + px[i+2]) & 0xFFFFFFFF
        return s
    sa = sig(pa, wa, 100, 700, 100, 450)
    sb = sig(pb, wb, 100, 700, 100, 450)
    checks = {
        "no_panic": b"panic" not in serial.lower(),
        "terminal_spawned": b"terminal ready" in serial,
        "viewport_changed": sa != sb,
    }
    for k, v in checks.items():
        print(f"{k}: {v}")
    ok = all(checks.values())
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
