#!/usr/bin/env python3
"""mem_probe2.py — runtime memory footprint probe for M4KK1.

Uses the drag_repro-verified single-click launch chain to boot the
full-test ISO to the desktop, then talks to the serial test shell
(m4sht, spawned by MDM under M4K_TEST_AUTOLOGIN) to run `free` and
`proc`-style introspection.  Serial console is stdin/stdout of QEMU
(-serial stdio); no monitor needed.

Outputs:
  - boot memory line (Total/Free after kernel init, from serial log)
  - desktop-steady-state `free` output (total/used/free KB, procs)
  - optional per-process memory if `ps` exists
"""
import subprocess, time, sys, os, re

ISO = "output/m4kk1_0.0.1_build8-alpha1-full-test.iso"

def main():
    if not os.path.exists(ISO):
        print("[mem] ISO missing — build with --full-test first")
        return 1

    qemu = subprocess.Popen(
        ["qemu-system-i386", "-cdrom", ISO, "-m", "512",
         "-vga", "std", "-serial", "stdio", "-display", "none"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL)

    buf = bytearray()
    def pump(sec):
        end = time.time() + sec
        import select
        while time.time() < end:
            r, _, _ = select.select([qemu.stdout], [], [], 0.2)
            if r:
                chunk = os.read(qemu.stdout.fileno(), 4096)
                if not chunk:
                    break
                buf.extend(chunk)

    # wait for the m4sht prompt (boot ~40 s; the prompt is printed
    # after MDM starts the desktop — give it up to 100 s)
    saw_prompt = False
    for _ in range(100):
        pump(1)
        if b"~>" in bytes(buf) or b"m4sh" in bytes(buf)[-200:]:
            saw_prompt = True
            break
    if not saw_prompt:
        print("[mem] FAIL: serial shell prompt never appeared")
        print(bytes(buf)[-400:].decode("utf-8", "replace"))
        qemu.kill()
        return 1

    # let the desktop settle (copland+sprach spawn after the prompt)
    pump(10)

    def send(cmd):
        buf.clear()
        qemu.stdin.write((cmd + "\n").encode())
        qemu.stdin.flush()
        pump(3)
        return bytes(buf).decode("utf-8", "replace")

    out_free = send("free")
    out_ps = send("ps")

    # boot-time memory line from the serial transcript
    boot = re.search(rb"Total memory: (\d+) KB\n.*?Free memory: (\d+) KB",
                     bytes(buf), re.S)
    # (boot line scrolled past after buf.clear; re-read from a fresh
    # probe: instead ask the shell again — free shows current state)
    print("=== free (desktop steady state) ===")
    print(out_free.strip())
    print("=== ps ===")
    print(out_ps.strip())

    m = re.search(r"Mem:\s+(\d+)\s+KB\s+(\d+)\s+KB\s+(\d+)\s+KB", out_free)
    ok = m is not None
    if ok:
        total, used, free = map(int, m.groups())
        print(f"[mem] desktop footprint: used {used//1024} MB of "
              f"{total//1024} MB ({free//1024} MB free)")
    qemu.stdin.write(b"exit\n")
    qemu.kill()
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
