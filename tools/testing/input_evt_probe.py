import json, os, socket, subprocess, time

ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build4-alpha1-full.iso"
QMP = "/tmp/ie_qmp.sock"
INT = "/tmp/ie_int.log"

subprocess.run(["rm", "-f", QMP, INT], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:/dev/null",
     "-d", "int", "-D", INT,
     "-qmp", "unix:%s,server=on,nowait" % QMP],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print("booting...", flush=True)
time.sleep(30)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(QMP)
f = s.makefile("rw", encoding="utf-8", newline="\n")
json.loads(f.readline())
f.write(json.dumps({"execute": "qmp_capabilities"}) + "\n"); f.flush()
json.loads(f.readline())

# raw PS/2 key press via input layer
evt = {"execute": "input-send-event", "arguments": {"events": [
    {"type": "key", "data": {"down": True,
      "key": {"type": "qcode", "data": "a"}}}]}}
print(f.write(json.dumps(evt) + "\n")); f.flush()
time.sleep(2)
print("resp:", f.readline())
proc.kill()

d = open(INT, errors="replace").read()
print("v=21 in tail:", d[-30000:].count("v=21 "))
