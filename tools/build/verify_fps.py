#!/usr/bin/env python3
"""
verify_fps.py — M4KK1 desktop FPS verification probe.

Boots the full-test ISO (built with SPRACH_FPS_DIAG=1) in QEMU,
lets the desktop run, then collects the per-second [FPS] serial
telemetry lines emitted by the sprach main loop.

Phases:
  1. BOOT   — wait for "[SPRACH] Entering main loop"
  2. IDLE   — collect >= 10 s of [FPS] lines (desktop at rest)
  3. MOUSE  — HMP mouse_move storm (~2 moves/s for 10 s) while
              collecting [FPS] lines (hover/drag-free movement)
  4. Verdict — avg idle FPS >= 30, avg mouse FPS >= 30

Exit: prints RESULT: PASS/FAIL.
"""
import os
import re
import socket
import subprocess
import sys
import time

REPO = "/mnt/f/M4KK1"
ISO = os.path.join(REPO, "output", "m4kk1_0.0.1_build16-alpha1-full-test.iso")
SER_SOCK = "/tmp/fps_probe_serial.sock"
MON_SOCK = "/tmp/fps_probe_mon.sock"

FPS_RE = re.compile(r"\[FPS\] (\d+) FPS")


def start_qemu():
    cmd = [
        "qemu-system-i386", "-m", "1024",
        "-cdrom", ISO,
        "-serial", f"unix:{SER_SOCK},server,nowait",
        "-monitor", f"unix:{MON_SOCK},server,nowait",
        "-display", "none",
        "-no-reboot",
    ]
    return subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                            stderr=subprocess.STDOUT)


def sock_path_ok():
    return os.path.exists(SER_SOCK) and os.path.exists(MON_SOCK)


class Serial:
    def __init__(self):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(SER_SOCK)
        self.s.setblocking(False)
        self.buf = b""

    def drain(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            try:
                d = self.s.recv(65536)
                if d:
                    self.buf += d
            except BlockingIOError:
                time.sleep(0.05)

    def lines(self):
        out = self.buf.split(b"\n")
        self.buf = b""
        return [ln.decode("utf-8", "replace") for ln in out]


class Mon:
    def __init__(self):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(MON_SOCK)
        self.s.setblocking(False)
        self.buf = b""

    def cmd(self, c):
        try:
            self.s.sendall((c + "\n").encode())
        except BlockingIOError:
            pass

    def mouse_move(self, dx, dy):
        self.cmd(f"mouse_move {dx} {dy}")


def collect_fps(ser, seconds, mon=None, move=False):
    """Collect [FPS] lines for `seconds`, optionally wiggling mouse."""
    fps = []
    end = time.time() + seconds
    last_move = 0.0
    while time.time() < end:
        ser.drain(0.5)
        for ln in ser.lines():
            m = FPS_RE.search(ln)
            if m:
                fps.append(int(m.group(1)))
        if move and mon and time.time() - last_move >= 0.5:
            dx = 12 if (int(time.time() * 2) % 2) == 0 else -12
            mon.mouse_move(dx, 3)
            last_move = time.time()
    return fps


def main():
    if not os.path.exists(ISO):
        print(f"ISO not found: {ISO}")
        print("RESULT: FAIL (no full-test ISO)")
        sys.exit(1)

    for p in (SER_SOCK, MON_SOCK):
        if os.path.exists(p):
            os.unlink(p)

    qemu = start_qemu()
    try:
        deadline = time.time() + 20
        while not sock_path_ok() and time.time() < deadline:
            time.sleep(0.2)
        ser = Serial()
        mon = Mon()

        # Phase 1: boot to desktop
        t0 = time.time()
        booted = False
        while time.time() - t0 < 120:
            ser.drain(1.0)
            txt = b"\n".join(l.encode() for l in ser.lines())
            if b"Entering main loop" in txt:
                booted = True
                break
        if not booted:
            print("BOOT: FAIL (no main loop within 120 s)")
            print("RESULT: FAIL")
            sys.exit(1)
        print("BOOT: OK")

        # Settle: drop the boot-transition second (first [FPS] line
        # covers the bursty startup window and can be an outlier)
        time.sleep(3)

        # Phase 2: idle FPS
        idle = collect_fps(ser, 12)
        if idle and idle[0] < 10:
            idle = idle[1:]   # drop boot-transition outlier
        if not idle:
            print("IDLE: FAIL (no [FPS] telemetry — was ISO built with SPRACH_FPS_DIAG=1?)")
            print("RESULT: FAIL")
            sys.exit(1)
        avg_idle = sum(idle) / len(idle)
        print(f"IDLE: avg={avg_idle:.1f} FPS  samples={idle}")

        # Phase 3: mouse-move FPS
        moved = collect_fps(ser, 12, mon=mon, move=True)
        avg_move = sum(moved) / len(moved) if moved else 0.0
        print(f"MOUSE: avg={avg_move:.1f} FPS  samples={moved}")

        ok = avg_idle >= 30 and avg_move >= 30
        print(f"VERDICT: idle {avg_idle:.1f} / mouse {avg_move:.1f} "
              f"(target >= 30 both)")
        print("RESULT: " + ("PASS" if ok else "FAIL"))
        sys.exit(0 if ok else 1)
    finally:
        qemu.kill()


if __name__ == "__main__":
    main()
