#!/usr/bin/env python3
"""Black-box repro: double-click the desktop Files icon and watch the
serial log for the FM lifecycle ([FM] lines / execve / any EXC)."""

import os, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = sorted([f for f in os.listdir("output") if "full-test" in f],
              key=lambda f: os.path.getmtime(os.path.join("output", f)))
iso = os.path.join("output", isos[-1])
sock = "/tmp/m4k_fm.sock"
mon = "/tmp/m4k_fm.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/fm_repro.log", "wb")

qemu = subprocess.Popen(
    ["qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
     "-display", "none",
     "-serial", f"unix:{sock},server,nowait",
     "-monitor", f"unix:{mon},server,nowait"],
    stdout=log, stderr=log)

# wait for the monitor socket to appear (QEMU startup)
for _ in range(40):
    if os.path.exists(mon) and os.path.exists(sock):
        break
    if qemu.poll() is not None:
        print("QEMU died early; log tail:")
        print(open("logs/fm_repro.log", "rb").read()[-500:].decode("utf-8", "replace"))
        raise SystemExit(1)
    time.sleep(0.5)
else:
    print("monitor socket never appeared")
    qemu.kill()
    raise SystemExit(1)
s = socket.socket(socket.AF_UNIX)
s.connect(sock)
s.setblocking(False)
mon_s = socket.socket(socket.AF_UNIX)
mon_s.connect(mon)
mon_s.setblocking(False)


def drain(sk):
    out = b""
    try:
        while True:
            out += sk.recv(65536)
    except BlockingIOError:
        pass
    return out


def hmp(cmd):
    mon_s.sendall((cmd + "\n").encode())
    time.sleep(0.25)
    return drain(mon_s)


def serial_wait(token, timeout=60):
    end = time.time() + timeout
    acc = b""
    while time.time() < end:
        acc += drain(s)
        if token.encode() in acc:
            return True, acc
        time.sleep(0.3)
    return False, acc


# wait for desktop
ok, acc = serial_wait("painting initial scene", 60)
print("boot:", ok)
time.sleep(3)
acc += drain(s)

# Move to the Files desktop icon.  Desktop grid (from sprach.c):
# icons at DESK_GRID_X + col*DESK_CELL_W, DESK_GRID_Y + col rows.
# First icon cell centre is around (40, 40+MENUBAR_H) — use HMP
# relative mouse_move from (400,300) guest default.
tx, ty = 40, 60            # first desktop icon centre-ish
dx, dy = tx - 400, ty - 300
hmp(f"mouse_move {dx} {dy}")
time.sleep(1.0)
acc += drain(s)
# double click: two press/release pairs, fast
for _ in range(2):
    hmp("mouse_button 1")
    time.sleep(0.15)
    hmp("mouse_button 0")
    time.sleep(0.15)
acc += drain(s)
time.sleep(4)

end = time.time() + 15
while time.time() < end:
    acc += drain(s)
    time.sleep(0.3)

text = acc.decode("utf-8", "replace")
with open("logs/fm_serial_full.log", "w") as f:
    f.write(text)
fm_lines = [l for l in text.splitlines() if "FM" in l or "EXC" in l or "execve" in l]
for l in fm_lines[-15:]:
    print(l)
print("HAS_FM_START:", "[FM] file manager starting" in text)
print("HAS_FM_SLOT:", "surface ready" in text)
print("HAS_EXC:", "EXC" in text)
# Is the rest of the system alive after the FM freeze?  Any serial
# chatter (scheduler/cp debug lines) after the last [FM] line means
# only fm died; total silence means the whole guest locked up.
tail = text[text.rfind("[FM]"):] if "[FM]" in text else text[-500:]
print("ALIVE_AFTER_FM:", len([l for l in tail.splitlines() if l.strip()]) > 1)
# After the freeze window, grab the CPU state via HMP before killing.
hmp("stop")
time.sleep(0.5)
regs = hmp("info registers")
time.sleep(1.0)
hmp("cont")
time.sleep(1.0)
hmp("stop")
regs2 = hmp("info registers")
with open("logs/fm_regs.log", "w") as f:
    f.write(regs.decode("utf-8", "replace"))
    f.write("\n===== 2nd sample =====\n")
    f.write(regs2.decode("utf-8", "replace"))
print("REGS_SAVED:", len(regs), len(regs2))
qemu.kill()
