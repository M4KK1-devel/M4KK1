#!/usr/bin/env python3
"""
apps_fps_probe.py — FPS-under-load verification for the desktop apps.

Boots the full-test ISO (built with SPRACH_FPS_DIAG=1), spawns each
GUI app via the m4sh `spawn` builtin, waits for its banner, then
collects the sprach [FPS] telemetry while that app is running.

PASS criteria: every app banner seen AND avg FPS >= 25 while the
apps are resident (the goal spec's per-app floor).
"""
import os
import re
import select
import socket
import subprocess
import sys
import time

REPO = "/mnt/f/M4KK1"
ISO_CAND = os.path.join(REPO, "output")
SER = "/tmp/apps_fps.sock"
MON = "/tmp/apps_fps_mon.sock"

APPS = [
    ("sysmon",  rb"\[SYSMON\] surface ready"),
    ("fm",      rb"\[FM\]"),
    ("mpl4yer", rb"\[MPL\] surface ready"),
    ("cal",     rb"\[CAL\] surface ready"),
    ("disk",    rb"\[DISK\] surface ready"),
    ("calcg",   rb"\[CALC\] surface ready"),
    ("pref",    rb"\[PREF\] surface ready"),
]

FPS_RE = re.compile(rb"\[FPS\] (\d+) FPS")


def main():
    isos = sorted(
        (f for f in os.listdir(ISO_CAND) if f.endswith("-full-test.iso")),
        key=lambda f: os.path.getmtime(os.path.join(ISO_CAND, f)),
    )
    if not isos:
        print("no full-test ISO")
        print("RESULT: FAIL")
        sys.exit(1)
    iso = os.path.join(ISO_CAND, isos[-1])
    print("ISO:", iso)

    for p in (SER, MON):
        if os.path.exists(p):
            os.unlink(p)

    q = subprocess.Popen(
        ["qemu-system-i386", "-m", "1024", "-cdrom", iso,
         "-serial", f"unix:{SER},server,nowait",
         "-monitor", f"unix:{MON},server,nowait",
         "-display", "none", "-no-reboot"],
        stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    st = {"buf": b""}

    def drain(t):
        end = time.time() + t
        while time.time() < end:
            try:
                d = s.recv(65536)
                if d:
                    st["buf"] += d
            except BlockingIOError:
                time.sleep(0.1)

    try:
        while not (os.path.exists(SER) and os.path.exists(MON)):
            time.sleep(0.2)
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(SER)
        s.setblocking(False)

        # boot to shell prompt (strip ANSI colour codes first — the
        # prompt is \x1b[36mm4sh\x1b[37m ~> so a raw match fails)
        ansi = re.compile(rb"\x1b\[[0-9;]*[A-Za-z]")
        t0 = time.time()
        while time.time() - t0 < 90:
            drain(1.0)
            if b"m4sh ~>" in ansi.sub(b"", st["buf"]):
                break
        else:
            print("BOOT: FAIL")
            print("RESULT: FAIL")
            sys.exit(1)
        print("BOOT: OK")
        time.sleep(2)

        # spawn each app, wait for banner
        ok_apps = []
        for name, banner in APPS:
            st["buf"] = b""
            s.setblocking(True)
            s.sendall(f"spawn /bin/{name}\n".encode())
            s.setblocking(False)
            t1 = time.time()
            seen = False
            while time.time() - t1 < 20:
                drain(0.5)
                if re.search(banner, st["buf"]):
                    seen = True
                    break
            print(f"{name}: {'OK' if seen else 'BANNER MISSING'}")
            if seen:
                ok_apps.append(name)

        # all apps resident: measure FPS for 12 s
        st["buf"] = b""
        drain(12)
        fps = [int(m.group(1)) for m in FPS_RE.finditer(st["buf"])]
        if not fps:
            print("FPS: no telemetry (need SPRACH_FPS_DIAG=1 ISO)")
            print("RESULT: FAIL")
            sys.exit(1)
        avg = sum(fps) / len(fps)
        print(f"FPS (all apps up): avg={avg:.1f} samples={fps}")

        allok = len(ok_apps) == len(APPS) and avg >= 25
        print(f"RESULT: {'PASS' if allok else 'FAIL'} "
              f"({len(ok_apps)}/{len(APPS)} apps, {avg:.1f} FPS)")
        sys.exit(0 if allok else 1)
    finally:
        q.kill()


if __name__ == "__main__":
    main()
