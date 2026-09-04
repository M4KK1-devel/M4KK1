#!/usr/bin/env python3
"""Suite smoke probe: boot the fresh ISO, wait for the m4sh prompt,
spawn each new /bin app, verify its serial banner."""
import subprocess, re, sys, time, select, glob, os

cands = sorted(glob.glob("output/m4kk1_*-full-test.iso"), key=os.path.getmtime)
ISO = cands[-1].replace("\r", "")
print("ISO:", ISO, flush=True)

APPS = [
    ("sysmon",  rb"\[SYSMON\] surface ready"),
    ("mpl4yer", rb"\[MPL\] surface ready"),
    ("cal",     rb"\[CAL\] surface ready"),
    ("disk",    rb"\[DISK\] surface ready"),
    ("pref",    rb"\[PREF\] surface ready"),
    ("altr",    rb"\[ALTR2\] surface ready"),
]

cmd = ["qemu-system-i386", "-cdrom", ISO, "-m", "512",
       "-vga", "std", "-serial", "stdio", "-display", "none",
       "-net", "none"]

p = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
buf = b""
results = {}
prompt_seen = False
t0 = time.time()

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
    while time.time() - t0 < 150:
        pump(1.0)
        pb = plain(buf)
        if not prompt_seen and b"m4sh ~>" in pb:
            prompt_seen = True
            print(f"[{time.time()-t0:.0f}s] prompt seen", flush=True)
            time.sleep(3)
            for name, _ in APPS:
                try:
                    p.stdin.write(f"spawn /bin/{name}\n".encode())
                    p.stdin.flush()
                except Exception as e:
                    print("stdin err", e, flush=True)
                time.sleep(2.0)
            pump(10)
        if prompt_seen:
            pb = plain(buf)
            for name, pat in APPS:
                if name not in results and re.search(pat, pb):
                    results[name] = "OK"
                    print(f"[{time.time()-t0:.0f}s] {name}: banner OK", flush=True)
            if len(results) == len(APPS):
                break
            if time.time() - t0 > 140:
                break
finally:
    p.kill()

print("\n=== RESULTS ===", flush=True)
ok = 0
for name, pat in APPS:
    r = results.get(name, "MISS")
    print(f"{name:10s} {r}", flush=True)
    ok += r == "OK"
print(f"{ok}/{len(APPS)} apps verified", flush=True)
# save evidence
with open("logs/suite_probe_serial.log", "wb") as f:
    f.write(buf)
sys.exit(0 if ok == len(APPS) else 1)
