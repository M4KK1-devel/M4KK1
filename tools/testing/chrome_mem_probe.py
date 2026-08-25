#!/usr/bin/env python3
"""Probe: boot autologin ISO, wait 40s, then via QMP pmemsave dump
the guest back_buffer region and the sprach BSS chrome buffers,
plus a screendump; compare bytes to find where the chrome pixels
actually are."""
import socket, subprocess, time, json, sys, glob

ISO = (sorted(glob.glob("/mnt/f/M4KK1/output/m4kk1_*-full-test.iso")) or [""])[-1]
SER = "/tmp/probe_serial.log"
PORT = 4484
SHOT = "/tmp/probe_shot.ppm"
MEM = "/tmp/probe_mem.bin"

subprocess.run(["rm", "-f", SER, SHOT, MEM], check=False)
qemu = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
     "-vga", "std", "-serial", f"file:{SER}",
     "-display", "none", "-qmp", f"tcp:127.0.0.1:{PORT},server,nowait"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    time.sleep(3)
    s = None
    for _ in range(20):
        try:
            s = socket.create_connection(("127.0.0.1", PORT), timeout=2); break
        except ConnectionRefusedError:
            time.sleep(1)
    s.settimeout(3)
    def qmp(obj):
        s.sendall((json.dumps(obj) + "\r\n").encode()); time.sleep(0.4)
        try:
            while True:
                if not s.recv(65536): break
        except socket.timeout: pass
    qmp({"execute": "qmp_capabilities"})
    # wait for desktop
    t0 = time.time()
    while time.time() - t0 < 60:
        time.sleep(2)
        t = open(SER, "rb").read().decode(errors="replace")
        if "Entering main loop" in t: break
    time.sleep(5)
    # back buffer is kernel-heap allocated; screendump gives the LFB view.
    qmp({"execute": "screendump", "arguments": {"filename": SHOT}})
    time.sleep(1)
    # dump the sprach chrome BSS to verify the pixels exist there
    # taskbar_buf @ 0x17252c0 (readelf), size 0x38000
    qmp({"execute": "pmemsave",
         "arguments": {"val": 0x17252c0, "size": 0x38000, "filename": MEM}})
    time.sleep(1)
finally:
    qemu.terminate(); qemu.wait()

# analyze
mem = open(MEM, "rb").read()
TB_OFF = 0            # taskbar_buf
MB_OFF = 0x25000      # menubar_buf
def argb(off, x, y, w):
    o = off + (y*w+x)*4
    v = int.from_bytes(mem[o:o+4], "little")
    return ((v>>16)&255, (v>>8)&255, v&255)  # xrgb -> rgb
print("taskbar_buf (src of dock):")
for y in [0, 10, 24, 40]:
    print("  y=%d px(8,%d)=%s px(400,%d)=%s" % (
        y, y, argb(TB_OFF, 8, y, 800), y, argb(TB_OFF, 400, y, 800)))
print("menubar_buf (src of bar):")
for y in [0, 5, 12, 20]:
    print("  y=%d px(8,%d)=%s px(400,%d)=%s" % (
        y, y, argb(MB_OFF, 8, y, 800), y, argb(MB_OFF, 400, y, 800)))

d = open(SHOT, "rb").read()
parts = d.split(b"\n", 3)
w, h = map(int, parts[1].split())
pix = parts[3]
def px(x, y):
    o = (y*w+x)*3
    return tuple(pix[o:o+3])
print("screen now: (0,10)", px(0,10), "(400,10)", px(400,10),
      "(400,578)", px(400,578))
