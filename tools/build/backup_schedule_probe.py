#!/usr/bin/env python3
"""backup_schedule_probe.py — verify the backup -s HH:MM feature.

Boots the fresh full-test ISO over serial (cmd line like suite_probe),
then at the m4sh prompt runs:
  1. automission            -> expect "(no scheduled missions" hint
  2. backup -s 23:45        -> expect "scheduled daily at 23:45"
  3. automission            -> expect "backup" + "daily 23:45" + "in "
     (countdown line)
  4. backup -s 99:99        -> expect "bad time"
All assertions come from m4sh serial output.
"""
import subprocess, re, sys, time, select, glob, os

os.chdir("/mnt/f/M4KK1")
cands = sorted(glob.glob("output/m4kk1_*-full-test.iso"), key=os.path.getmtime)
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
    ("empty_hint",  rb"\(no scheduled missions"),
    ("registered",  rb"scheduled daily at 23:45"),
    ("countdown",   rb"backup +daily 23:45 +in \d{2}:\d{2}:\d{2}"),
    ("badtime",     rb"bad time '99:99'"),
]

try:
    while time.time() - t0 < 150:
        pump(1.0)
        pb = plain(buf)
        if not prompt_seen and b"m4sh ~>" in pb:
            prompt_seen = True
            print(f"[{time.time()-t0:.0f}s] prompt seen", flush=True)
            time.sleep(3)
            send("automission")
            time.sleep(2)
            send("backup -s 23:45")
            time.sleep(2)
            send("automission")
            time.sleep(2)
            send("backup -s 99:99")
            time.sleep(2)
            pump(5)
        if prompt_seen:
            pb = plain(buf)
            for name, pat in CHECKS:
                if name not in steps_done and re.search(pat, pb):
                    steps_done.add(name)
                    print(f"[{time.time()-t0:.0f}s] {name}: OK", flush=True)
            if len(steps_done) == len(CHECKS):
                break
            if time.time() - t0 > 140:
                break
finally:
    p.kill()

print("\n=== RESULTS ===", flush=True)
ok = 0
for name, pat in CHECKS:
    r = "OK" if name in steps_done else "MISS"
    print(f"{name:12s} {r}", flush=True)
    ok += r == "OK"
print(f"{ok}/{len(CHECKS)} checks verified", flush=True)
with open("logs/backup_sched_serial.log", "wb") as f:
    f.write(buf)
sys.exit(0 if ok == len(CHECKS) else 1)
