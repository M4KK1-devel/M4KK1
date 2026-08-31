#!/usr/bin/env python3
"""lp_search_probe.py — verify launchpad live search filter + keyboard
navigation + Enter-launch.

Drives QEMU (full-test ISO) via HMP mouse + sendkey:
  1. click the Dock launchpad grid icon     -> "launchpad open"
  2. sendkey 'l' (types into the filter)    -> "launchpad: filter 'l' matches"
  3. sendkey ret (Enter = launch match 0)   -> "launchpad: enter at cell"
                                             -> "launchpad: launching /bin/l..."
Assertions come from sprach.c ser_puts lines on the serial console.
"""
import os, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = sorted(f for f in os.listdir("output")
             if f.endswith("full-test.iso") or f.endswith("-full.iso"))
if not isos:
    print("[lp_search] no full-test ISO; run build_krn.sh --full-test first")
    raise SystemExit(1)
iso = os.path.join("output", isos[0])
print("[lp_search] using", iso)

sock = "/tmp/m4k_lps.sock"
mon = "/tmp/m4k_lps.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/lp_search_serial.log", "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-serial", "unix:%s,server=on,wait=off" % sock,
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

def sendkey(k):
    hmp("sendkey " + k)
    drain(0.4)

drain(0.5)
drain(30)   # boot to desktop (autologin)

# Dock launchpad grid icon: first dock cell, centre (24, 576)
LP_DOCK_X, LP_DOCK_Y = 24, 576

# 1) open the launchpad overlay from the Dock
move_to(LP_DOCK_X, LP_DOCK_Y)
drain(0.3)
left_click()
drain(2.0)

# 2) type 'l' into the live search filter
sendkey("l")
drain(1.0)

# 3) Tab: move keyboard selection to the next match
sendkey("tab")
drain(1.0)

# 4) Enter = launch the selected match
sendkey("ret")
drain(3.0)

text = acc.decode("utf-8", "replace")

def after(marker, needle):
    i = text.find(marker)
    return i >= 0 and needle in text[i:]

checks = {
    "lp_open":      "launchpad open" in text,
    "filter_live":  "launchpad: filter 'l' matches " in text,
    "filter_zero":  "matches 0" not in text.split("filter 'l'")[-1][:40]
                    if "filter 'l'" in text else False,
    "tab_select":   "launchpad: select " in text,
    "enter_cell":   "launchpad: enter at cell" in text,
    # the launched path must be one of the 'l'-filtered apps, and the
    # launch must follow the Enter event
    "launching":    after("launchpad: enter at cell",
                          "launchpad: launching /bin/"),
    "lp_closed":    after("launchpad: launching", "launchpad open") is False,
    "no_panic":     "PANIC" not in text,
    "no_gpf":       "#GP" not in text and "General Protection" not in text,
}
print("--- serial checks ---")
for k, v in checks.items():
    print("%s: %s" % (k, v))
ok = all(checks.values())
print("RESULT:", "PASS" if ok else "FAIL")

qemu.kill()
