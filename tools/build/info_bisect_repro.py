#!/usr/bin/env python3
"""Bisect repro: spawn info, then send 'x' first, then 'tab', watching
which one triggers the WM heartbeat stall."""
import subprocess, socket, time, glob, os, json, sys

cands = sorted(glob.glob("output/m4kk1_*-full-test.iso"), key=os.path.getmtime)
ISO = cands[-1].replace("\r", "")
print("ISO:", ISO, flush=True)

PORT = 4485
SOCK = "/tmp/info_bisect_ser.sock"

qemu = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
     "-vga", "std", "-display", "none", "-net", "none", "-no-reboot",
     "-chardev", f"socket,id=ser0,path={SOCK},server=on,wait=off",
     "-serial", "chardev:ser0",
     "-qmp", f"tcp:127.0.0.1:{PORT},server,nowait"],
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

s = None
while time.time() - t0 < 60 and s is None:
    try:
        s = socket.create_connection(("127.0.0.1", PORT), timeout=2)
    except OSError:
        s = None
        time.sleep(1)
if s:
    s.settimeout(2)
    try: s.recv(65536)
    except Exception: pass
    s.sendall((json.dumps({"execute": "qmp_capabilities"}) + "\r\n").encode())

def sendkey(name, times=1):
    for _ in range(times):
        s.sendall((json.dumps({"execute": "send-key", "arguments": {
            "keys": [{"type": "qcode", "data": name}]}}) + "\r\n").encode())
        time.sleep(0.4)
    try: s.recv(65536)
    except Exception: pass

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

def phase(name, secs):
    print(f"--- {name} ({secs}s) ---", flush=True)
    for _ in range(secs // 3):
        readserial()
        time.sleep(3)
    print(f"  stalls={buf.count(b'heartbeat stalled')} "
          f"procs={buf.count(b'[INFO] PROC p=')}", flush=True)

try:
    while time.time() - t0 < 120:
        readserial()
        if b"m4sh ~>" in buf:
            break
        time.sleep(1)
    print("prompt seen:", b"m4sh ~>" in buf, flush=True)
    time.sleep(3)
    ser.sendall(b"spawn /bin/info\n")
    phase("after spawn, no keys", 12)
    sendkey("x")
    phase("after key x", 12)
    sendkey("tab")
    phase("after key tab", 15)
finally:
    with open("logs/info_bisect_serial.log", "wb") as f:
        f.write(buf)
    qemu.kill()
