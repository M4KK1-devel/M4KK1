#!/usr/bin/env python3
"""clock_alarm_probe.py — verify the clock alarm feature.

Drives the full-test ISO via serial shell + HMP:
  1. spawn /bin/clock, wait for "[CLOCK] surface ready"
  2. minimize the desktop terminal (HMP click on its title-bar min
     button) so keystrokes fall through to the top GUI app
  3. sendkey 'a'  -> expect "[CLOCK] ALARM ADD" serial line
  4. sendkey 'd'  -> expect "[CLOCK] ALARM DEL" serial line
  5. re-add, then wait up to 75 s for "[CLOCK] ALARM RING"
Assertions come from clock_gui.c ser_puts evidence lines.
"""
import os, socket, subprocess, time, re, sys

os.chdir("/mnt/f/M4KK1")
isos = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not isos:
    print("[clkalarm] no full-test ISO")
    sys.exit(1)
iso = os.path.join("output", isos[0])
print("[clkalarm] using", iso)

sock = "/tmp/m4k_clkalarm.sock"
mon = "/tmp/m4k_clkalarm.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/clock_alarm_serial.log", "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-serial", "unix:%s,server=on,wait=on" % sock,
    "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-no-reboot",
], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

time.sleep(2)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sock)
s.setblocking(False)
m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
m.connect(mon)
m.setblocking(False)

acc = b""
def drain(t):
    global acc
    end = time.time() + t
    while time.time() < end:
        for sk, is_mon in ((s, False), (m, True)):
            try:
                d = sk.recv(65536)
                if d and not is_mon:
                    acc += d
                    log.write(d)
                    log.flush()
            except BlockingIOError:
                pass
        time.sleep(0.05)

def hmp(cmd):
    m.sendall((cmd + "\n").encode())
    drain(0.15)

def send(line):
    s.sendall((line + "\n").encode())
    drain(0.1)

def sendkey(k):
    hmp("sendkey " + k)

def plain(b):
    return re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", b)

gx, gy = 400, 300

def move_to(tx, ty, step=8):
    global gx, gy
    while gx != tx or gy != ty:
        dx = max(-step, min(step, tx - gx))
        dy = max(-step, min(step, ty - gy))
        hmp("mouse_move %d %d" % (dx, dy))
        gx += dx
        gy += dy

def left_click():
    hmp("mouse_button 1")
    drain(0.3)
    hmp("mouse_button 0")
    drain(0.3)

def wait_for(pattern, sec):
    end = time.time() + sec
    while time.time() < end:
        drain(1)
        if re.search(pattern, plain(acc)):
            return True
    return False

rc = 1
try:
    # 1. shell prompt (ANSI-coloured — strip first)
    if not wait_for(rb"m4sh ~>", 120):
        print("[clkalarm] FAIL: no shell prompt")
    else:
        time.sleep(3)
        # 2. spawn the clock
        mark = len(acc)
        send("spawn /bin/clock")
        if not wait_for(rb"\[CLOCK\] surface ready", 15):
            print("[clkalarm] FAIL: clock did not start")
        else:
            print("[clkalarm] clock started")
            drain(2)
            # NOTE: no minimize step — the desktop terminal window is
            # not open in this boot (serial m4sht is the shell), so
            # keystrokes fall straight through sprach_ga_key to the
            # top GUI client.  Clicking around the desktop would only
            # hit a desktop icon and launch FM, stealing top focus.
            # 3. add alarm
            mark = len(acc)
            sendkey("a")
            add1 = wait_for(rb"\[CLOCK\] ALARM ADD", 10)
            print("[clkalarm] ALARM ADD:", "OK" if add1 else "FAIL")
            # 5. delete alarm
            mark = len(acc)
            sendkey("d")
            dele = wait_for(rb"\[CLOCK\] ALARM DEL", 10)
            print("[clkalarm] ALARM DEL:", "OK" if dele else "FAIL")
            # 5. re-add and wait for the ring.  plain() strips ANSI
            # escapes so a raw-offset mark cannot index into it —
            # count occurrences instead.
            before_add2 = len(re.findall(rb"\[CLOCK\] ALARM ADD",
                                         plain(acc)))
            before_ring = len(re.findall(rb"\[CLOCK\] ALARM RING",
                                         plain(acc)))
            sendkey("a")
            add2 = False
            end = time.time() + 10
            while time.time() < end:
                drain(1)
                if len(re.findall(rb"\[CLOCK\] ALARM ADD",
                                  plain(acc))) > before_add2:
                    add2 = True
                    break
            ring = False
            end = time.time() + 75
            while time.time() < end:
                drain(1)
                if len(re.findall(rb"\[CLOCK\] ALARM RING",
                                  plain(acc))) > before_ring:
                    ring = True
                    break
            print("[clkalarm] ALARM RING:", "OK" if ring else "FAIL")
            ok = add1 and dele and add2 and ring
            ok = ok and b"panic" not in acc.lower()
            print("[clkalarm] no_panic:", b"panic" not in acc.lower())
            print("RESULT:", "PASS" if ok else "FAIL")
            rc = 0 if ok else 1
finally:
    log.close()
    qemu.kill()
sys.exit(rc)
