#!/usr/bin/env python3
"""Diag: measure glide wall-time + sample screen every 100 ms."""
import os, re, socket, subprocess, sys, time

os.chdir("/mnt/f/M4KK1")
iso = "/tmp/m4kk1_anim_probe.iso"
print("using", iso)

sock, mon = "/tmp/animd_ser.sock", "/tmp/animd_mon.sock"
for p in (sock, mon):
    if os.path.exists(p):
        os.unlink(p)

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-serial", "unix:%s,server=on,wait=off" % sock,
    "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-no-reboot",
], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

time.sleep(2)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); s.connect(sock); s.setblocking(False)
m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); m.connect(mon); m.setblocking(False)

acc = b""
events = []          # (wall_time, token)

def drain(t):
    global acc
    end = time.time() + t
    while time.time() < end:
        for sk, is_mon in ((s, False), (m, True)):
            try:
                d = sk.recv(65536)
                if d and not is_mon:
                    acc += d
                    for tok in (b"ANIM MIN", b"ANIM SHOW", b"ANIM END", b"MIN "):
                        if tok in d:
                            events.append((time.time(), tok.decode()))
            except BlockingIOError:
                pass
        time.sleep(0.01)

def hmp(cmd):
    m.sendall((cmd + "\n").encode()); drain(0.1)

gx, gy = 400, 300
def move_to(tx, ty, step=8):
    global gx, gy
    while gx != tx or gy != ty:
        dx = max(-step, min(step, tx - gx)); dy = max(-step, min(step, ty - gy))
        hmp("mouse_move %d %d" % (dx, dy)); gx += dx; gy += dy

def right_click():
    hmp("mouse_button 2"); drain(0.3); hmp("mouse_button 0"); drain(0.3)

def screendump(tag):
    path = "/tmp/animd_%s.ppm" % tag
    m.sendall(("screendump " + path + "\n").encode()); drain(0.2)
    try:
        data = open(path, "rb").read()
    except OSError:
        return None
    mo = re.match(rb"P6\n(\d+) (\d+)\n255\n", data)
    if not mo:
        return None
    w, h = int(mo.group(1)), int(mo.group(2))
    return w, h, data[mo.end():]

def red_stats(img):
    if img is None:
        return "none"
    w, h, pix = img
    n = 0; y0, y1 = 1 << 30, -1
    for y in range(0, h, 2):
        row = y * w * 3
        for x in range(0, w, 2):
            o = row + x * 3
            r, g, b = pix[o], pix[o+1], pix[o+2]
            if 110 <= r <= 146 and 32 <= g <= 64 and 32 <= b <= 64:
                n += 1
                if y < y0: y0 = y
                if y > y1: y1 = y
    return "n=%d y=%d..%d" % (n, y0 if n else -1, y1 if n else -1)

# boot
drain(0.5)
end = time.time() + 60
while time.time() < end:
    if b"painting initial scene" in acc:
        break
    drain(0.3)
drain(3)
print("boot done")

# PHASE A: pre-click scene check
img = screendump('pre')
def windowcount(img):
    w,h,pix = img
    cols = {'red':0,'green':0,'blue':0}
    for y in range(24, 552, 2):
        row = y*w*3
        for x in range(0, w, 2):
            o = row + x*3
            r,g,b = pix[o],pix[o+1],pix[o+2]
            if 110<=r<=146 and 32<=g<=64 and 32<=b<=64: cols['red']+=1
            elif 32<=r<=64 and 110<=g<=146 and 32<=b<=64: cols['green']+=1
            elif 32<=r<=64 and 32<=g<=64 and 110<=b<=146: cols['blue']+=1
    return cols
img0 = screendump('booted')
print('BOOTED windows:', windowcount(img0))
print('PRE-CLICK windows:', windowcount(img))
open('/tmp/animd_pre2.ppm','wb').write(open('/tmp/animd_pre.ppm','rb').read())

move_to(200, 98); drain(0.3)
right_click(); drain(1.5)
img = screendump('menuopen')
print('MENU-OPEN windows:', windowcount(img))
move_to(240, 111); drain(0.3)
t_click = time.time()
m.sendall(b"mouse_button 1\n"); drain(0.03)
m.sendall(b"mouse_button 0\n")

# sample synchronized with ANIMD k= lines
for i in range(80):
    drain(0.05)
    txt = acc.decode("utf-8", "replace")
    if "[ANIMD] k=8 " in txt and i < 3:
        img = screendump("sync8")
        print("SYNC k=8:", red_stats(img))
    if "[ANIMD] k=16 " in txt and 3 <= i < 6:
        img = screendump("sync16")
        print("SYNC k=16:", red_stats(img))
    if "[ANIMD] k=24 " in txt and 6 <= i < 9:
        img = screendump("sync24")
        print("SYNC k=24:", red_stats(img))
    if i % 10 == 0:
        img = screendump("s%02d" % i)
        print("t=%.2fs %s" % (time.time() - t_click, red_stats(img)))

open("/tmp/animd_serial.log","wb").write(acc)
print("---- serial events ----")
for ts, tok in events:
    print("%.3f  %s" % (ts - t_click, tok))

qemu.kill()
