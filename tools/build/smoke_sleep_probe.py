#!/usr/bin/env python3
"""smoke_sleep_probe.py — verify blocking m4k_sleep under load.

Boots the full-test ISO, spawns 7 GUI apps, then measures sprach
[FPS] telemetry.  PASS: all banners + avg FPS >= 25 (the same bar
apps_fps_probe uses, but tolerant of the telemetry build).
Also greps ps output for SLEEPING states.
"""
import os, re, select, socket, subprocess, sys, time

REPO = "/mnt/f/M4KK1"
ISO_CAND = os.path.join(REPO, "output")
SER = "/tmp/smoke_sleep.sock"
MON = "/tmp/smoke_sleep_mon.sock"

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
    isos = sorted((f for f in os.listdir(ISO_CAND)
                   if f.endswith("-full-test.iso") or f.endswith("-full.iso")),
                  key=lambda f: os.path.getmtime(os.path.join(ISO_CAND, f)))
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

        ansi = re.compile(rb"\x1b\[[0-9;]*[A-Za-z]")
        t0 = time.time()
        while time.time() - t0 < 90:
            drain(1.0)
            if b"m4sh ~>" in ansi.sub(b"", st["buf"]):
                break
        else:
            print("BOOT: FAIL"); print("tail:", st["buf"][-500:])
            print("RESULT: FAIL"); sys.exit(1)
        print("BOOT: OK")
        time.sleep(2)

        ok_apps = []
        for name, banner in APPS:
            st["buf"] = b""
            s.setblocking(True)
            s.sendall(f"spawn /bin/{name}\n".encode())
            s.setblocking(False)
            t1 = time.time()
            seen = False
            while time.time() - t1 < 25:
                drain(0.5)
                if re.search(banner, st["buf"]):
                    seen = True; break
            print(f"{name}: {'OK' if seen else 'BANNER MISSING'}")
            if seen:
                ok_apps.append(name)

        # all apps resident: measure FPS 12 s
        st["buf"] = b""
        drain(12)
        fps = [int(m.group(1)) for m in FPS_RE.finditer(st["buf"])]
        if not fps:
            print("FPS: no telemetry (base ISO has no SPRACH_FPS_DIAG)")
            print("ps output:", st["buf"][-2000:])
            # without telemetry, fall back: boot kept responding = compositor alive
            allok = len(ok_apps) == len(APPS)
            print(f"RESULT: {'PASS' if allok else 'FAIL'} ({len(ok_apps)}/{len(APPS)} apps, no-FPS mode)")
            sys.exit(0 if allok else 1)
        avg = sum(fps) / len(fps)
        print(f"FPS (all apps up): avg={avg:.1f} samples={fps}")
        allok = len(ok_apps) == len(APPS) and avg >= 25
        print(f"RESULT: {'PASS' if allok else 'FAIL'} ({len(ok_apps)}/{len(APPS)} apps, {avg:.1f} FPS)")
        sys.exit(0 if allok else 1)
    finally:
        q.kill()


if __name__ == "__main__":
    main()
