#!/usr/bin/env python3
"""Spawn /bin/fm from the serial shell (m4sht) instead of via the
Sprach desktop double-click, to isolate the crash cause."""

import os, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = sorted([f for f in os.listdir("output") if "full-test" in f],
              key=lambda f: os.path.getmtime(os.path.join("output", f)))
iso = os.path.join("output", isos[-1])
sock = "/tmp/m4k_fm2.sock"
for f in (sock,):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/fm_shell_spawn.log", "wb")

qemu = subprocess.Popen(
    ["qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
     "-display", "none",
     "-serial", f"unix:{sock},server,nowait"],
    stdout=log, stderr=log)

for _ in range(40):
    if os.path.exists(sock):
        break
    time.sleep(0.5)

s = socket.socket(socket.AF_UNIX)
s.connect(sock)
s.setblocking(False)


def drain():
    out = b""
    try:
        while True:
            out += s.recv(65536)
    except BlockingIOError:
        pass
    return out


# wait for the m4s prompt (MDM autologin spawns m4sht on serial)
end = time.time() + 60
acc = b""
while time.time() < end:
    acc += drain()
    if b"m4sh" in acc and b"#~" in acc or acc.count(b"~>") >= 1:
        break
    time.sleep(0.4)

time.sleep(2)
drain()
s.sendall(b"fm &\r")
time.sleep(6)
end = time.time() + 8
while time.time() < end:
    acc += drain()
    time.sleep(0.3)

text = acc.decode("utf-8", "replace")
with open("logs/fm_shell_spawn_full.log", "w") as f:
    f.write(text)
for l in text.splitlines():
    if "FM" in l or "EXC" in l or "execve" in l:
        print(l)
print("HAS_FM_START:", "[FM] file manager starting" in text)
print("HAS_FM_SLOT:", "surface ready" in text)
print("HAS_EXC:", "EXC" in text)
qemu.kill()
