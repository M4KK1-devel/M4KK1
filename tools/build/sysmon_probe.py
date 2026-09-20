#!/usr/bin/env python3
"""sysmon heap-panel probe: boots full-test ISO (autologin -> m4sht),
spawns /bin/sysmon, asserts the boot banner and that its heap-alloc'd
proc table came up clean (no "heap alloc failed")."""
import subprocess, re, sys, time, select, glob, os

os.chdir("/mnt/f/M4KK1")
cands = sorted(glob.glob("output/m4kk1_*-full-test.iso"), key=os.path.getmtime)
if not cands:
    print("RESULT: FAIL (no full-test ISO)")
    sys.exit(1)
ISO = cands[-1]
print("ISO:", ISO, flush=True)

p = subprocess.Popen(
    ["qemu-system-i386", "-cdrom", ISO, "-m", "512",
     "-vga", "std", "-serial", "stdio", "-display", "none",
     "-net", "none"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
buf = b""
t0 = time.time()
prompt_seen = False
done = False

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

try:
    while time.time() - t0 < 120:
        pump(1.0)
        pb = plain(buf)
        if not prompt_seen and b"m4sh ~>" in pb:
            prompt_seen = True
            print(f"[{time.time()-t0:.0f}s] prompt seen", flush=True)
            time.sleep(3)
            p.stdin.write(b"spawn /bin/sysmon\n")
            p.stdin.flush()
            pump(15)
            pb = plain(buf)
            done = b"[SYSMON] surface ready" in pb
            break
finally:
    p.kill()

pb = plain(buf)
ok = done and b"heap alloc failed" not in pb and \
     b"[SYSMON] starting" in pb
for l in pb.splitlines():
    if b"SYSMON" in l:
        print("  " + l.decode("utf-8", "replace"), flush=True)
with open("logs/sysmon_probe_serial.log", "wb") as f:
    f.write(buf)
print("PROBE:", "PASS" if ok else "FAIL", flush=True)
sys.exit(0 if ok else 1)
