#!/usr/bin/env python3
"""wmenu_kbd_probe.py — context-menu KEYBOARD navigation probe.

Arrows never reach sprach (keymap drops scancodes without ASCII),
so navigation is Tab / Shift+Tab (kernel chord 0x06) + Enter + Esc.
Drives the full-test ISO via HMP sendkey + mouse:

  1. wallpaper right-click (mode 1) -> tab x3 cycles 0,1,2 (wrap)
  2. shift-tab walks back 1,0,2 (wrap on 0x06 backward)
  3. Esc -> "rmenu kbd close" (mode 1-3 used to ignore Esc)
  4. re-open, tab, Enter -> "rmenu kact 0" spawns a terminal
  5. right-click the terminal title bar (mode 4), shift-tab
     selects item 2 from nothing, Enter -> TERMINAL CLOSE
  6. wallpaper menu: tab tab Enter (Change Wallpaper) opens the
     mode-3 theme submenu, shift-tab -> ksel 5, Esc closes
Assertions come from sprach.c ser_puts lines on the serial console.
"""
import os, re, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not isos:
    print("[wmenu-kbd] no full-test ISO, run build_krn.sh --full-test first")
    raise SystemExit(2)
iso = os.path.join("output", isos[0])
print("[wmenu-kbd] using", iso)

sock = "/tmp/m4k_wmk.sock"
mon = "/tmp/m4k_wmk.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/wmenu_kbd_serial.log", "wb")

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

acc = ""

def drain(t):
    global acc
    end = time.time() + t
    while time.time() < end:
        for sk, is_mon in ((s, False), (m, True)):
            try:
                d = sk.recv(65536)
                if d and not is_mon:
                    acc += d.decode("utf-8", "replace")
                    log.write(d)
                    log.flush()
            except BlockingIOError:
                pass
        time.sleep(0.05)

def hmp(cmd):
    m.sendall((cmd + "\n").encode())
    drain(0.15)

def key(k):
    hmp("sendkey " + k)
    drain(0.4)

gx, gy = 400, 300

def move_to(tx, ty, step=8):
    global gx, gy
    while gx != tx or gy != ty:
        dx = max(-step, min(step, tx - gx))
        dy = max(-step, min(step, ty - gy))
        hmp("mouse_move %d %d" % (dx, dy))
        gx += dx
        gy += dy

def right_click():
    # HMP "mouse_button 2" reads as RIGHT in the guest (PS/2 bit1).
    hmp("mouse_button 2")
    drain(0.3)
    hmp("mouse_button 0")
    drain(0.3)

def wait_token(tok, t):
    end = time.time() + t
    while time.time() < end:
        drain(0.3)
        if tok in acc:
            return True
    return False

drain(0.5)
ok_boot = wait_token("painting initial scene", 60)
drain(3)
print("[wmenu-kbd] boot:", ok_boot)

# 1) wallpaper right-click -> mode 1 menu, Tab cycling + wrap
BARE_X, BARE_Y = 500, 200
move_to(BARE_X, BARE_Y)
drain(0.3)
right_click()
drain(1.0)
key("tab")          # ksel 0
key("tab")          # ksel 1
key("tab")          # ksel 2
key("tab")          # ksel 0 (wrap forward)

# 2) Shift+Tab (kernel chord 0x06) walks backward + wrap
key("shift-tab")    # ksel 2
key("shift-tab")    # ksel 1
key("shift-tab")    # ksel 0
key("shift-tab")    # ksel 2 (wrap backward)

# 3) Esc closes the mode-1 overlay menu
key("esc")
drain(0.8)

# 4) re-open, select item 0, Enter -> New Terminal
right_click()
drain(1.0)
key("tab")          # ksel 0
key("ret")          # kact 0 -> spawn terminal
got_term = wait_token("terminal window registered", 40)
drain(2)
mo = re.search(r"terminal window registered \(slot=\d+\) @ (\d+),(\d+)", acc)
if mo:
    tx, ty = int(mo.group(1)), int(mo.group(2))
    print("[wmenu-kbd] terminal @", tx, ty)
else:
    tx, ty = 60, 40
TBX, TBY = tx + 100, ty + 9   # title bar centre-left, clear of buttons

# 5) terminal title-bar menu (mode 4): shift-tab selects item 2
#    straight from nothing, Enter closes the terminal
move_to(TBX, TBY)
drain(0.3)
right_click()
drain(1.5)
key("shift-tab")    # ksel 2 (backward from -1 -> n-1)
key("ret")          # kact 2 -> TERMINAL CLOSE
drain(2.0)

# 6) wallpaper menu -> Change Wallpaper (kbd) -> mode 3 submenu,
#    backward wrap reaches item 5, Esc closes
move_to(BARE_X, BARE_Y)
drain(0.3)
right_click()
drain(1.0)
key("tab")          # ksel 0
key("tab")          # ksel 1
key("ret")          # kact 1 -> theme submenu (mode 3)
drain(1.0)
key("shift-tab")    # ksel 5 (backward from -1 in a 6-item menu)
key("esc")          # kbd close
drain(0.8)

text = acc
checks = {
    "boot":        ok_boot,
    "term_spawned": got_term,
    "ksel_fwd":    "rmenu ksel 0" in text and "rmenu ksel 1" in text
                   and "rmenu ksel 2" in text,
    "ksel_back":   text.count("rmenu ksel 2") >= 3,  # fwd wrap + back x2
    "kbd_close":   "rmenu kbd close" in text,
    "kact_spawn":  "rmenu kact 0" in text,
    "term_close":  "[SPRACH] TERMINAL CLOSE" in text,
    "kact_theme":  "rmenu kact 1" in text and "theme menu" in text,
    "theme_ksel5": "rmenu ksel 5" in text,
    "no_panic":    "PANIC" not in text,
    "no_gpf":      "#GP" not in text and "General Protection" not in text,
}
print("--- serial checks ---")
for k, v in checks.items():
    print("%s: %s" % (k, v))
ok = all(checks.values())
print("RESULT:", "PASS" if ok else "FAIL")

qemu.kill()
