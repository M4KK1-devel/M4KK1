#!/usr/bin/env python3
"""heap_probe.py — verify the hardened m4k_libc heap allocator.

Boots the fresh full-test ISO over serial (autologin -> m4sht), then
runs `spawn /bin/heaptest`.  The heaptest ELF prints one
"[HEAP] <name>: OK" line per check and a final
"[HEAP] RESULT: <pass>/<total>".  Also greps the boot log for
"/bin/heaptest written" (kernel-side YAFS install).

PASS: all 9 checks OK + RESULT: 9/9 + written banner.
"""
import subprocess, re, sys, time, select, glob, os

os.chdir("/mnt/f/M4KK1")
cands = sorted(glob.glob("output/m4kk1_*-full-test.iso"), key=os.path.getmtime)
if not cands:
    print("RESULT: FAIL (no full-test ISO)")
    sys.exit(1)
ISO = cands[-1]
print("ISO:", ISO, flush=True)

cmd = ["qemu-system-i386", "-cdrom", ISO, "-m", "512",
       "-vga", "std", "-serial", "stdio", "-display", "none",
       "-net", "none"]

p = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
buf = b""
t0 = time.time()
prompt_seen = False
steps_done = set()

def plain(b):
    return re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", b)

def pump(sec):
    global buf
    end = time.time() + sec
    while time.time() < end:
        r, _, _ = select.select([p.stdout], [], [], 0.5)
        if r:
            c = p.stdout.read1(65536)
            if c:
                buf += c

def send(line):
    p.stdin.write((line + "\n").encode())
    p.stdin.flush()

CHECKS = [
    ("written",   rb"/bin/heaptest written \(\d+ bytes\)"),
    ("starting",  rb"\[HEAP\] heaptest starting"),
    ("coalesce",  rb"\[HEAP\] coalesce: OK"),
    ("split",     rb"\[HEAP\] split: OK"),
    ("doublefree", rb"\[HEAP\] doublefree: OK"),
    ("wildfree",  rb"\[HEAP\] wildfree: OK"),
    ("canary",    rb"\[HEAP\] canary: OK"),
    ("callocover", rb"\[HEAP\] callocover: OK"),
    ("calloczero", rb"\[HEAP\] calloczero: OK"),
    ("realloc",   rb"\[HEAP\] realloc: OK"),
    ("recycle",   rb"\[HEAP\] recycle: OK"),
    ("summary",   rb"\[HEAP\] RESULT: 9/9"),
]

try:
    while time.time() - t0 < 150:
        pump(1.0)
        pb = plain(buf)
        if not prompt_seen and b"m4sh ~>" in pb:
            prompt_seen = True
            print(f"[{time.time()-t0:.0f}s] prompt seen", flush=True)
            time.sleep(3)
            send("spawn /bin/heaptest")
            pump(10)
        pb = plain(buf)
        for name, pat in CHECKS:
            if name not in steps_done and re.search(pat, pb):
                steps_done.add(name)
                print(f"[{time.time()-t0:.0f}s] {name}: OK", flush=True)
        if prompt_seen and "summary" in steps_done:
            break
        if "starting" in steps_done and "summary" not in steps_done \
           and time.time() - t0 > 60:
            break   # ran but did not finish — capture partial
finally:
    p.kill()

print("\n=== RESULTS ===", flush=True)
ok = 0
for name, pat in CHECKS:
    r = "OK" if name in steps_done else "MISS"
    print(f"{name:12s} {r}", flush=True)
    ok += r == "OK"
print(f"{ok}/{len(CHECKS)} checks verified", flush=True)
with open("logs/heap_probe_serial.log", "wb") as f:
    f.write(buf)
sys.exit(0 if ok == len(CHECKS) else 1)
