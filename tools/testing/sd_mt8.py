import json, socket, subprocess, sys, time

ISO = "/tmp/mt8.iso"
qemu = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:/tmp/sd_serial.log",
     "-qmp", "unix:/tmp/sdqmp.sock,server,nowait"],
    cwd="/tmp")
time.sleep(30)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect("/tmp/sdqmp.sock")
f = s.makefile("rw")
json.loads(f.readline())  # greeting
def cmd(d):
    f.write(json.dumps(d) + "\n"); f.flush()
    while True:
        r = json.loads(f.readline())
        if "return" in r or "error" in r:
            return r
cmd({"execute": "qmp_capabilities"})
cmd({"execute": "screendump", "arguments": {"filename": "/tmp/sd_screen.ppm"}})
time.sleep(1)
cmd({"execute": "quit"})
qemu.wait(timeout=10)
print("serial lines:", sum(1 for _ in open("/tmp/sd_serial.log")))
print("done")
