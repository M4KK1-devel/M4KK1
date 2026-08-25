#!/usr/bin/env python3
"""m4sh enhancement verification (serial-socket drive).

Boot the latest full-test (autologin) ISO.  MDM spawns /bin/m4sht
(serial shell, login gate off) beside the desktop.  The serial port
is a unix socket chardev, so we drive the shell by writing plain
ASCII to the socket — no QMP keyboard (input-send-event goes to the
PS/2 keyboard / graphics stack, never to the serial console).

Assertions:
  eval "echo hello"   -> hello
  cd /export; cd -    -> prints /export (OLDPWD swap)
  cd ~; pwd           -> /export/root (root's home)
  trap 'echo CAUGHT' INT ; Ctrl-C -> CAUGHT
  shift               -> builtin exists, no error
  help                -> eval/shift/trap listed
"""
import glob, socket, subprocess, sys, time

ISO = (sorted(glob.glob("/mnt/f/M4KK1/output/m4kk1_*-full-test.iso")) or [""])[-1]
if not ISO:
    sys.exit("no full-test ISO")
SER_SOCK = "/tmp/m4sh_ser.sock"
LOG = "/tmp/m4sh_ver2.log"

subprocess.run(["rm", "-f", SER_SOCK, LOG], check=False)
proc = subprocess.Popen(["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
    "-m", "512", "-display", "none",
    "-serial", f"unix:{SER_SOCK},server=on,wait=off"],
    stdout=open(LOG, "wb"), stderr=subprocess.STDOUT)
print("booting", ISO, flush=True)

# wait for the QEMU serial server socket
for _ in range(60):
    if glob.glob(SER_SOCK):
        break
    time.sleep(0.5)
time.sleep(1)
ser = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
ser.connect(SER_SOCK)
ser.settimeout(0.2)

buf = ""
def drain():
    global buf
    try:
        while True:
            d = ser.recv(65536)
            if not d:
                break
            buf += d.decode(errors="replace")
    except socket.timeout:
        pass

def wait_for(needle, timeout=150):
    t0 = time.time()
    while time.time() - t0 < timeout:
        drain()
        if needle in buf:
            return True
        time.sleep(0.5)
    return False

def mark():
    return len(buf)

def run(line, wait=1.2, ctrlc=False):
    off = mark()
    ser.sendall(line.encode() + b"\r")
    time.sleep(wait)
    drain()
    if ctrlc:
        ser.sendall(b"\x03")
        time.sleep(1.2)
        drain()
    return buf[off:]

if not wait_for("~>", 150):
    proc.kill(); sys.exit("FAIL: no shell prompt on serial")
print("shell prompt up", flush=True)
time.sleep(1)

results = {}

out = run('eval "echo hello"')
results["eval_hello"] = "hello" in out
print("eval:", repr(out[-160:]), flush=True)

run("cd /export", wait=1.5)
run("cd ~", wait=1.5)              # cwd now differs from OLDPWD=/export
out2 = run("cd -", wait=1.5)       # Zsh: back to /export, prints it
results["cd_minus"] = "/export" in out2
print("cd -:", repr(out2[-160:]), flush=True)

out = run("cd ~")
out2 = run("pwd")
results["cd_tilde"] = ("/export/root" in out2) or ("/root" in out2)
print("cd ~ + pwd:", repr(out2[-160:]), flush=True)

out = run("trap \"echo CAUGHT\" INT")
out2 = run("", ctrlc=True)
results["trap_int"] = "CAUGHT" in out2
print("trap INT:", repr(out2[-200:]), flush=True)

out = run("shift")
results["shift_cmd"] = "invalid" not in out and "not found" not in out
print("shift:", repr(out[-160:]), flush=True)

out = run("help", wait=1.5)
results["new_cmds_listed"] = ("eval" in out and "shift" in out and "trap" in out)
print("help lists:", results["new_cmds_listed"], flush=True)

proc.kill()
print("\n=== RESULTS ===")
ok = True
for k, v in results.items():
    print(("PASS: " if v else "FAIL: ") + k)
    ok = ok and v
sys.exit(0 if ok else 1)
