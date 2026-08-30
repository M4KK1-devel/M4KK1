#!/usr/bin/env python3
"""apps_repro.py — verify the new guiapp programs spawn and reach
"surface ready".

The launchpad click-chain was already proven (fm, logview launched
from grid cells in an earlier run).  But blindly walking the grid
also spawns /bin/cc (PCC, huge BSS) which destabilises QEMU, so this
probe drives the serial shell (m4sht, full-test autologin) instead:
spawn each app, wait for its serial banner, close it with 'q' via
the sprach keyboard mailbox (ga_apps width dispatch), next.

apps: clock / logview / info   (automission & backup are m4sh
builtins — verified by running the commands directly)
"""
import subprocess, time, sys, os, select

ISO = "output/m4kk1_0.0.1_build8-alpha1-full-test.iso"

BANNERS = [
    ("clock",   b"[CLOCK] surface ready"),
    ("logview", b"[LOGVIEW] surface ready"),
    ("info",    b"[INFO] surface ready"),
    ("automission", b"build-check"),       # builtin output
    ("backup",  b"backup: "),              # builtin output
]


def main():
    if not os.path.exists(ISO):
        print("[apps] ISO missing"); return 1

    qemu = subprocess.Popen(
        ["qemu-system-i386", "-cdrom", ISO, "-m", "512",
         "-vga", "std", "-serial", "stdio", "-display", "none"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL)
    buf = bytearray()

    def pump(sec):
        end = time.time() + sec
        while time.time() < end:
            r, _, _ = select.select([qemu.stdout], [], [], 0.2)
            if r:
                try:
                    chunk = os.read(qemu.stdout.fileno(), 4096)
                except OSError:
                    break
                if not chunk:
                    break
                buf.extend(chunk)

    def send(line):
        qemu.stdin.write((line + "\n").encode())
        qemu.stdin.flush()

    # wait for the m4sht prompt — it is ANSI-coloured
    # ("\r\x1b[33m[m4kk1]\x1b[37m@\x1b[36mm4sh\x1b[37m ~> "), so match
    # after stripping escapes.
    def plain(b):
        import re as _re
        return _re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", b)

    for _ in range(120):
        pump(1)
        if b"m4sh ~>" in plain(bytes(buf)):
            break
    if b"m4sh ~>" not in plain(bytes(buf)):
        print("[apps] no shell prompt"); qemu.kill(); return 1
    time.sleep(3)

    results = {}
    for name, banner in BANNERS:
        mark = len(buf)
        if name in ("clock", "logview", "info"):
            send(f"spawn /bin/{name}")
        else:
            send(name)
        # GUI apps need the desktop focus; spawn is enough — they
        # register their surface on their own.  Wait up to 15 s.
        for _ in range(15):
            pump(1)
            if banner in bytes(buf)[mark:]:
                break
        results[name] = banner in bytes(buf)[mark:]
        print(f"[apps] {name}: {'OK' if results[name] else 'FAIL'}")
        if results[name] and name in ("clock", "logview", "info"):
            send("kill %1 2>/dev/null")   # best-effort; apps also
            time.sleep(0.5)               # exit when unfocused via 'q'

    serial = bytes(buf)
    ok = all(results.values()) and b"panic" not in serial.lower()
    print("no_panic:", b"panic" not in serial.lower())
    print("RESULT:", "PASS" if ok else "FAIL")
    qemu.kill()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
