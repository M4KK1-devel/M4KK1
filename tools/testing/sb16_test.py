#!/usr/bin/env python3
"""M4KK1 Phase 2 SB16 audio driver test.

Boots the kernel ISO with the SB16 sound card, verifies DSP
detection at boot, then logs in over the serial console and runs
the userspace `beep` command through the M4K_SYS_BEEP syscall.
"""
import glob
import os
import subprocess
import sys
import threading
import time

ISO = max(glob.glob("output/m4kk1_*.iso"), key=os.path.getmtime)

buf = bytearray()
lock = threading.Lock()


def reader(proc):
    while True:
        chunk = proc.stdout.read(1)
        if not chunk:
            break
        with lock:
            buf.extend(chunk)


def expect(proc, marker, timeout=60, start=0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        with lock:
            idx = bytes(buf).find(marker, start)
        if idx >= 0:
            return idx + len(marker)
        if proc.poll() is not None:
            break
        time.sleep(0.05)
    return -1


def send(proc, data, delay=0.05):
    proc.stdin.write(data)
    proc.stdin.flush()
    time.sleep(delay)


def main():
    proc = subprocess.Popen(
        [
            "qemu-system-i386",
            "-boot", "d",
            "-cdrom", ISO,
            "-m", "512",
            "-display", "none",
            "-serial", "stdio",
            "-device", "sb16",
        ],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    t = threading.Thread(target=reader, args=(proc,), daemon=True)
    t.start()

    results = []

    def check(name, ok):
        results.append((name, ok))
        print(("PASS" if ok else "FAIL") + ": " + name, flush=True)

    try:
        # Boot-time SB16 detection
        pos = expect(proc, b"[SB16] DSP detected, version 4.05", 60)
        check("kernel detects SB16 DSP v4.05", pos >= 0)
        pos = expect(proc, b"Playing 440Hz boot beep", 30)
        check("boot beep started", pos >= 0)
        pos = expect(proc, b"Boot beep complete", 30)
        check("boot beep completed (DMA + IRQ5)", pos >= 0)

        # Login
        pos = expect(proc, b"M4KK1 login: ", 60)
        check("login prompt appears", pos >= 0)
        send(proc, b"root\n")
        pos = expect(proc, b"Password: ", 30)
        check("password prompt appears", pos >= 0)
        send(proc, b"123456\n")
        pos = expect(proc, b"Login successful", 30)
        check("root login succeeds", pos >= 0)

        # Shell prompt
        pos = expect(proc, b"~> ", 60)
        check("shell prompt appears", pos >= 0)

        # Userspace beep via syscall (defaults 440Hz 200ms)
        send(proc, b"beep\n")
        pos = expect(proc, b"beep: played 440Hz for 200ms", 60)
        check("userspace beep command works", pos >= 0)
        pos = expect(proc, b"~> ", 30, pos)

        # beep with explicit args
        send(proc, b"beep 880 100\n")
        pos = expect(proc, b"beep: played 880Hz for 100ms", 60)
        check("beep with explicit args works", pos >= 0)

    finally:
        try:
            proc.kill()
        except Exception:
            pass

    print("\n=== SUMMARY ===")
    failed = [n for n, ok in results if not ok]
    for n, ok in results:
        print(("  PASS " if ok else "  FAIL ") + n)
    print("Total: %d  Passed: %d  Failed: %d"
          % (len(results), len(results) - len(failed), len(failed)))

    with lock:
        tail = bytes(buf)[-2000:]
    sys.stdout.write("\n--- serial tail ---\n")
    sys.stdout.write(tail.decode("latin-1", "replace"))
    sys.stdout.write("\n--- end tail ---\n")

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
