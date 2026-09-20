#!/usr/bin/env python3
"""smoke_sysinfo_probe.py — verify SYSINFO timer_waiters telemetry.

Boot the full-test ISO over serial, run `free`, and check:
  1. the new "Sleep: N in m4k_sleep" line is present and parses;
  2. after spawning two GUI apps (which poll via m4k_sleep), at
     least one sample reports >= 1 timer waiter — proves the
     counter is wired end-to-end (PCB WAIT_TIMER -> sysinfo -> uapi).
Related: refine#3 of the blocking-sleep feature (2026-09-20).
"""
import os, re, socket, subprocess, sys, time

REPO = "/mnt/f/M4KK1"
ISO_CAND = os.path.join(REPO, "output")
SER = "/tmp/smoke_sysinfo.sock"
MON = "/tmp/smoke_sysinfo_mon.sock"

SLEEP_RE = re.compile(rb"Sleep:\s+(\d+) in m4k_sleep")


def pick_iso():
    cands = [f for f in os.listdir(ISO_CAND)
             if f.endswith("-full-test.iso") or f.endswith("-full.iso")]
    if not cands:
        sys.exit("no candidate ISO")
    newest = lambda fs: max(fs, key=lambda f: os.path.getmtime(os.path.join(ISO_CAND, f)))
    ft = [f for f in cands if f.endswith("-full-test.iso")]
    return os.path.join(ISO_CAND, newest(ft) if ft else newest(cands))


def main():
    iso = pick_iso()
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
            print("BOOT: FAIL"); print("tail:", st["buf"][-400:])
            print("RESULT: FAIL"); sys.exit(1)
        print("BOOT: OK")

        def run_free():
            st["buf"] = b""
            s.setblocking(True)
            s.sendall(b"free\n")
            s.setblocking(False)
            drain(2.5)
            return SLEEP_RE.search(st["buf"])

        # 1) idle sample: line must exist and parse
        m = run_free()
        if not m:
            print("FREE-OUTPUT: FAIL — no 'Sleep: N in m4k_sleep' line")
            print("tail:", st["buf"][-400:])
            print("RESULT: FAIL"); sys.exit(1)
        idle = int(m.group(1))
        print(f"FREE-OUTPUT: OK (idle timer_waiters={idle})")

        # 2) loaded sample: spawn two sleeping apps, expect >= 1
        s.setblocking(True)
        s.sendall(b"spawn /bin/sysmon\n")
        s.setblocking(False)
        drain(4.0)
        s.setblocking(True)
        s.sendall(b"spawn /bin/cal\n")
        s.setblocking(False)
        drain(4.0)
        loaded_max = -1
        for _ in range(5):
            m = run_free()
            if m:
                loaded_max = max(loaded_max, int(m.group(1)))
        print(f"LOADED: max timer_waiters over 5 samples = {loaded_max}")
        ok = loaded_max >= 1
        print(f"RESULT: {'PASS' if ok else 'FAIL'}")
        sys.exit(0 if ok else 1)
    finally:
        q.kill()


if __name__ == "__main__":
    main()
