#!/usr/bin/env python3
"""Minimal repro: spawn /bin/info, send NOTHING, watch for WM stalls."""
import subprocess, socket, time, glob, os, re, sys

cands = sorted(glob.glob("output/m4kk1_*-full-test.iso"), key=os.path.getmtime)
ISO = cands[-1].replace("\r", "")
print("ISO:", ISO, flush=True)

PORT = 4484
SOCK = "/tmp/info_min_ser.sock"

qemu = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
     "-vga", "std", "-display", "none", "-net", "none", "-no-reboot",
     "-chardev", f"socket,id=ser0,path={SOCK},server=on,wait=off",
     "-serial", "chardev:ser0"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

buf = b""
t0 = time.time()
ser = None
while time.time() - t0 < 30 and ser is None:
    try:
        ser = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        ser.connect(SOCK)
    except OSError:
        ser = None
        time.sleep(1)

def readserial():
    global buf
    quiet = 0
    while quiet < 2:
        try:
            ser.settimeout(0.5)
            c = ser.recv(65536)
            if c: buf += c; quiet = 0
            else: quiet += 1
        except socket.timeout:
            quiet += 1

try:
    while time.time() - t0 < 120:
        readserial()
        if b"m4sh ~>" in buf:
            break
        time.sleep(1)
    print("prompt seen:", b"m4sh ~>" in buf, flush=True)
    time.sleep(3)
    ser.sendall(b"spawn /bin/info\n")
    print("spawned, waiting 60s without any key...", flush=True)
    for _ in range(12):
        readserial()
        time.sleep(3)
    stalls = buf.count(b"heartbeat stalled")
    procs = buf.count(b"[INFO] PROC p=")
    print(f"stalls={stalls} proc_lines={procs}", flush=True)
    tail = buf[-600:].decode(errors="replace")
    print("--- tail ---")
    print(tail)
finally:
    qemu.kill()
