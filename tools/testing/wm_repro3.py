#!/usr/bin/env python3
"""WM restart repro v3: REAL input events via input-send-event.
Move cursor to specific UI spots, click dock/menubar/desktop, drag
windows, type keys — mimic actual desktop interaction."""
import json, socket, subprocess, sys, time

ISO = "/tmp/mtM.iso"
SER = "/tmp/ms29_run.log"

qemu = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
     "-vga", "std", "-display", "none",
     "-serial", "file:" + SER,
     "-qmp", "tcp:127.0.0.1:4448,server,nowait"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    time.sleep(2)
    s = socket.create_connection(("127.0.0.1", 4448))
    f = s.makefile("rw")

    def qmp(cmd, **args):
        f.write(json.dumps({"execute": cmd, "arguments": args}) + "\n")
        f.flush()
        while True:
            line = f.readline()
            if not line:
                return None
            msg = json.loads(line)
            if "return" in msg or "error" in msg:
                return msg

    def ev(events):
        qmp("input-send-event", events=events)

    def move_rel(dx, dy):
        ev([{"type": "rel", "data": {"axis": "x", "value": dx}},
            {"type": "rel", "data": {"axis": "y", "value": dy}}])

    def click():
        ev([{"type": "btn", "data": {"down": True, "button": "left"}}])
        time.sleep(0.05)
        ev([{"type": "btn", "data": {"down": False, "button": "left"}}])

    def key(name):
        ev([{"type": "key", "data": {"down": True,
             "key": {"type": "qemu", "data": {"name": name}}}}])
        ev([{"type": "key", "data": {"down": False,
             "key": {"type": "qemu", "data": {"name": name}}}}])

    qmp("qmp_capabilities")
    print("QMP up; boot 35s...", file=sys.stderr)
    time.sleep(35)

    # The guest cursor starts centered (400,300) with screen 800x600.
    # Drive it by REL deltas like a real mouse.
    def goto(tx, ty, x=[400], y=[300]):
        dx = max(-80, min(80, tx - x[0]))
        dy = max(-80, min(80, ty - y[0]))
        while x[0] != tx or y[0] != ty:
            dx = max(-50, min(50, tx - x[0]))
            dy = max(-50, min(50, ty - y[0]))
            move_rel(dx, dy)
            x[0] += dx; y[0] += dy
            time.sleep(0.005)

    t0 = time.time()
    seq = 0
    while time.time() - t0 < 60:
        seq += 1
        # menubar clock (top-right)
        goto(700, 10); click(); time.sleep(0.3)
        # close clock (Esc)
        key("esc"); time.sleep(0.3)
        # dock area (bottom center-left)
        goto(300, 575); click(); time.sleep(0.3)
        # desktop body
        goto(400, 300); click(); time.sleep(0.2)
        # drag across a window
        goto(300, 200)
        ev([{"type": "btn", "data": {"down": True, "button": "left"}}])
        for i in range(10):
            move_rel(15, 8); time.sleep(0.02)
        ev([{"type": "btn", "data": {"down": False, "button": "left"}}])
        time.sleep(0.3)
        # type something
        for c in "hello":
            key(c); time.sleep(0.05)
        time.sleep(0.5)
    print("workload sequences: %d" % seq, file=sys.stderr)
    time.sleep(10)
finally:
    qemu.terminate()
    try:
        qemu.wait(5)
    except Exception:
        qemu.kill()

log = open(SER, errors="replace").read()
print("WM starts:", log.count("Window manager starting"))
print("stalls:", log.count("heartbeat stalled"))
print("stale30:", log.count("stale=30"))
for l in [l for l in log.splitlines()
          if "stale" in l or "stalled" in l or "exiting" in l][:12]:
    print(l)
print("---- tail ----")
for l in [l for l in log.splitlines() if l.strip()][-6:]:
    print(l)
