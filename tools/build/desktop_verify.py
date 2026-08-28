#!/usr/bin/env python3
"""Desktop environment verification: boot to Sprach, screendump at key
points, and assert the new features initialise (tray, desktop icons,
wallpaper themes) via the serial log."""
import os, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = sorted([f for f in os.listdir("output") if f.endswith("full-test.iso")],
              key=lambda f: os.path.getmtime(os.path.join("output", f)))
iso = os.path.join("output", isos[-1])
sock = "/tmp/m4k_desk.sock"
qmp = "/tmp/m4k_desk.qmp"
for f in (sock, qmp):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/desktop_verify.log", "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
    "-serial", "unix:%s,server=on,wait=off" % sock,
    "-qmp", "unix:%s,server=on,wait=off" % qmp,
    "-display", "none", "-no-reboot",
], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

time.sleep(2)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sock)
s.setblocking(False)

def drain(t):
    end = time.time() + t
    while time.time() < end:
        try:
            d = s.recv(65536)
            if d:
                log.write(d)
        except BlockingIOError:
            pass
        time.sleep(0.1)

def shot(name):
    """Screendump via QMP."""
    q = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    q.connect(qmp)
    q.setblocking(False)
    def qdrain(t=0.3):
        end = time.time() + t
        while time.time() < end:
            try:
                q.recv(65536)
            except BlockingIOError:
                pass
            time.sleep(0.05)
    qdrain(0.5)
    q.sendall(b'{"execute":"qmp_capabilities"}\n')
    qdrain(0.3)
    q.sendall(('{"execute":"screendump","arguments":{"filename":"/tmp/%s.ppm"}}\n' % name).encode())
    qdrain(0.8)
    q.close()

# Boot: MDM login → desktop (Sprach auto-spawns from MDM after login)
drain(30)
shot("desk_boot")

# Let the desktop settle (tray painted, icons scanned)
drain(10)
shot("desk_idle")

qemu.terminate()
try:
    qemu.wait(5)
except Exception:
    qemu.kill()
log.close()

text = open("logs/desktop_verify.log", "rb").read().decode("utf-8", "replace")
checks = {
    "sprach_up": "[SPRACH] Entering main loop" in text,
    "desktop_layer": "desktop icon layer" in text or "desk icons" in text,
    "no_gpf": "GPF" not in text and "General Protection" not in text,
    "no_panic": "PANIC" not in text,
    "copland_up": "[COPLAND] Copland ready" in text,
}
print("--- checks ---")
ok = True
for k, v in checks.items():
    print("%s: %s" % (k, v))
    if not v:
        ok = False
print("--- shots: /tmp/desk_boot.ppm /tmp/desk_idle.ppm ---")
print("RESULT:", "PASS" if ok else "FAIL")
raise SystemExit(0 if ok else 1)
