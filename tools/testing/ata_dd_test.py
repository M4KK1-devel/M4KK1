#!/usr/bin/env python3
"""M4KK1 Phase 1 ATA/devfs/dd interactive test.

Boots the kernel ISO with an ATA disk, logs in over the serial
console, then runs `dd if=/dev/hda of=/tmp/mbr.bin bs=512 count=1`
and verifies the read-back MBR content matches the host-side marker.
"""
import subprocess
import sys
import threading
import time

ISO = max(glob.glob("output/m4kk1_*.iso"), key=os.path.getmtime)
DISK = "disk.img"
MARKER = b"M4KK1MBR"

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
    """Wait until marker (bytes) appears in buf at/after start.

    Returns the index after the marker, or -1 on timeout.
    """
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
            "-hda", DISK,
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
        # Kernel ATA detection
        pos = expect(proc, b"[ATA] Master drive detected: 64MB", 60)
        check("kernel detects 64MB master drive", pos >= 0)
        pos = expect(proc, b"[ATA] MBR: 1 partition(s) found", 30,
                     max(pos, 0))
        check("kernel parses MBR partition table", pos >= 0)

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

        # ls /dev
        send(proc, b"ls /dev\n")
        pos = expect(proc, b"hda", 30)
        check("ls /dev lists hda", pos >= 0)
        pos = expect(proc, b"~> ", 30, pos)

        # dd the MBR
        send(proc, b"dd if=/dev/hda of=/tmp/mbr.bin bs=512 count=1\n")
        pos = expect(proc, b"512 bytes copied", 60)
        check("dd copies 512 bytes from /dev/hda", pos >= 0)
        pos = expect(proc, b"~> ", 30, pos)

        # Verify content
        send(proc, b"cat /tmp/mbr.bin\n")
        pos = expect(proc, MARKER, 30)
        check("read-back MBR contains marker " + MARKER.decode(),
              pos >= 0)

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

    # Dump the tail of the serial log for debugging
    with lock:
        tail = bytes(buf)[-2000:]
    sys.stdout.write("\n--- serial tail ---\n")
    sys.stdout.write(tail.decode("latin-1", "replace"))
    sys.stdout.write("\n--- end tail ---\n")

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
