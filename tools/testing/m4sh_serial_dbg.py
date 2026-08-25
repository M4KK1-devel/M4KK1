#!/usr/bin/env python3
"""Serial input diagnosis: char-by-char slow typing vs burst send."""
import glob, socket, subprocess, sys, time

ISO = (sorted(glob.glob("/mnt/f/M4KK1/output/m4kk1_*-full-test.iso")) or [""])[-1]
SOCK = "/tmp/m4sh_dbg.sock"
subprocess.run(["rm", "-f", SOCK], check=False)
proc = subprocess.Popen(["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
    "-m", "512", "-display", "none",
    "-serial", f"unix:{SOCK},server=on,wait=off"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
for _ in range(60):
    if glob.glob(SOCK):
        break
    time.sleep(0.5)
time.sleep(1)
ser = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
ser.connect(SOCK)
ser.settimeout(0.2)
buf = ""
def drain():
    global buf
    try:
        while True:
            d = ser.recv(65536)
            if not d: break
            buf += d.decode(errors="replace")
    except socket.timeout:
        pass
def wait_for(n, t=150):
    t0=time.time()
    while time.time()-t0 < t:
        drain()
        if n in buf: return True
        time.sleep(0.4)
    return False
def mark(): return len(buf)
def slow(line, d=0.06):
    off = mark()
    for ch in line:
        ser.sendall(ch.encode()); time.sleep(d)
    ser.sendall(b"\r"); time.sleep(1.5); drain()
    return buf[off:]
def burst(line):
    off = mark()
    ser.sendall(line.encode()+b"\r"); time.sleep(1.5); drain()
    return buf[off:]

if not wait_for("~>"): proc.kill(); sys.exit("no prompt")
time.sleep(1)
print("=== burst  'echo AAA' ==="); print(repr(burst("echo AAA")[-120:]))
print("=== slow   'echo BBB' ==="); print(repr(slow("echo BBB")[-120:]))
print("=== slow   eval \"echo CCC\" ==="); print(repr(slow('eval "echo CCC"')[-160:]))
print("=== burst  eval \"echo DDD\" ==="); print(repr(burst('eval "echo DDD"')[-160:]))
print("=== slow   trap list ==="); print(repr(slow("trap")[-120:]))
print("=== slow   trap \"echo CAUGHT\" INT ==="); print(repr(slow('trap "echo CAUGHT" INT')[-160:]))
off = mark(); ser.sendall(b"\x03"); time.sleep(1.5); drain()
print("=== ctrl-C ==="); print(repr(buf[off:][-160:]))
proc.kill()
