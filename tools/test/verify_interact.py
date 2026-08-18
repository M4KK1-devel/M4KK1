#!/usr/bin/env python3
"""Full interactive suite: terminal, virtual desktops, launchpad, menu.

Run inside WSL after the desktop is up (sprach clock ticking).
"""
import os
import sys
import time
import subprocess

sys.path.insert(0, "tools/test")
from qmp_client import QMP

q = QMP("/tmp/qmp.sock")
q.cmd("qmp_capabilities")


def hold(k, d):
    q.cmd("input-send-event", {"events": [
        {"type": "key", "data": {"key": {"type": "qcode", "data": k},
                                 "down": d}}]})


def tap(k):
    hold(k, True); time.sleep(0.15); hold(k, False)


LOG = os.environ.get("QEMU_LOG", "qemu_run.log")


def wait_log(pat, timeout=60):
    t0 = time.time()
    while time.time() - t0 < timeout:
        r = subprocess.run(["grep", "-ac", pat, LOG],
                           capture_output=True, text=True)
        if r.stdout.strip() not in ("", "0"):
            return True
        time.sleep(2)
    return False


def shot(p):
    q.cmd("screendump", {"filename": "/tmp/" + p})


def results():
    print("---- RESULTS ----")
    for k, v in OUT.items():
        print(("PASS " if v else "FAIL ") + k)


OUT = {}

def mmove(dx, dy):
    """QEMU wants both axes merged in ONE events array (split cmd
    events are dropped); steps stay <=100 so PS/2 never overflows."""
    while dx or dy:
        sx = max(-100, min(100, dx))
        sy = max(-100, min(100, dy))
        q.cmd("input-send-event", {"events": [
            {"type": "rel", "data": {"axis": "x", "value": sx}},
            {"type": "rel", "data": {"axis": "y", "value": sy}}]})
        dx -= sx
        dy -= sy
        time.sleep(0.05)


def click():
    q.cmd("input-send-event", {"events": [
        {"type": "btn", "data": {"down": True, "button": "left"}}]})
    q.cmd("input-send-event", {"events": [
        {"type": "btn", "data": {"down": False, "button": "left"}}]})


def goto(x, y):
    """Park at top-left then one precise move (1:1 delta, no clamp)."""
    for _ in range(6):
        mmove(-200, -150)
        time.sleep(0.3)
    mmove(x, y)
    time.sleep(0.5)


# [A] Ctrl+Alt+T terminal
print("[A] Ctrl+Alt+T ...")
hold("ctrl", True); time.sleep(0.3)
hold("alt", True); time.sleep(0.3)
tap("t"); time.sleep(0.3)
hold("alt", False); hold("ctrl", False)
OUT["A_terminal_spawn"] = wait_log("terminal window registered", 60)

# wait for TERM ready then click title to activate
wait_log("TERM. terminal ready", 90)
shot("ia.ppm")
goto(100, 48)
click()
time.sleep(3)

# type ls / and Enter, wait for render
q.type_text("ls /"); time.sleep(1)
tap("ret")
time.sleep(30)
shot("ib.ppm")


def text_rows(p, x0=80, x1=740, y0=64, y1=460):
    d = open("/tmp/" + p, "rb").read()
    parts = d.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    pix = parts[3]
    rows = 0
    for row in range(0, 25):
        y = y0 + row * 16
        if y >= y1:
            break
        c = sum(1 for x in range(x0, x1)
                if pix[(y * w + x) * 3] > 140
                and pix[(y * w + x) * 3 + 1] > 140
                and pix[(y * w + x) * 3 + 2] > 140)
        if c > 3:
            rows += 1
    return rows


OUT["A_terminal_text"] = text_rows("ib.ppm") >= 3

# [B] virtual desktop Alt+Shift+2
print("[B] Alt+Shift+2 ...")
shot("ba.ppm")
hold("alt", True); hold("shift", True); time.sleep(0.3)
tap("2"); time.sleep(0.3)
hold("shift", False); hold("alt", False)
OUT["B_desktop_switch_log"] = wait_log("SPRACH. desktop 2", 30)
time.sleep(5)
shot("bb.ppm")


def diff(a, b):
    def load(p):
        d = open("/tmp/" + p, "rb").read()
        return d.split(b"\n", 3)[3]
    p1, p2 = load(a), load(b)
    return sum(1 for i in range(0, len(p1), 3) if p1[i:i+3] != p2[i:i+3])


OUT["B_menubar_updated"] = diff("ba.ppm", "bb.ppm") > 200

# switch back to 1
hold("alt", True); hold("shift", True); time.sleep(0.3)
tap("1"); time.sleep(0.3)
hold("shift", False); hold("alt", False)
time.sleep(5)

# [C] launchpad via dock grid icon
print("[C] launchpad ...")
shot("ca.ppm")
goto(22, 585)
click()
OUT["C_launchpad_open"] = wait_log("launchpad open", 30)
time.sleep(5)
shot("cb.ppm")

# Esc closes (release stray modifiers first)
hold("shift", False); hold("alt", False); hold("ctrl", False)
time.sleep(0.5)
tap("esc")
OUT["C_launchpad_esc"] = wait_log("launchpad closed", 30)
time.sleep(3)

# [D] app menu: click brand "M4KK1" at (25,12)
print("[D] app menu ...")
goto(25, 12)
click()
OUT["D_menu_open"] = wait_log("app menu open", 30)
time.sleep(4)
shot("db.ppm")
# close by Esc
tap("esc")
time.sleep(2)

results()
sys.exit(0 if all(OUT.values()) else 1)
