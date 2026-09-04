#!/usr/bin/env python3
"""logview_filter_probe.py — verify the logview filter mode.

Drives the full-test ISO via serial shell + HMP sendkey:
  1. mkdir /var /var/log; cp /export/cfg/passwd.db /var/log/messages
     (deterministic 3-line content: root/testuser have /bin/m4sh,
     nobody has /sbin/nologin)
  2. spawn /bin/logview, wait for "[LOGVIEW] surface ready"
  3. '/' + "m4sh" + Enter  -> expect '[LOGVIEW] FILTER "m4sh" 2/3 lines'
  4. F -> expect "[LOGVIEW] JUMP #1 line 0"
  5. F -> expect "[LOGVIEW] JUMP #2 line 1"
  6. Esc -> expect "[LOGVIEW] FILTER off"
Assertions come from logview.c ser_puts evidence lines.
"""
import os, socket, subprocess, time, re, sys

os.chdir("/mnt/f/M4KK1")
isos = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not isos:
    print("[lvfilter] no full-test ISO")
    sys.exit(1)
iso = os.path.join("output", isos[0])
print("[lvfilter] using", iso)

sock = "/tmp/m4k_lvfilter.sock"
mon = "/tmp/m4k_lvfilter.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/logview_filter_serial.log", "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-serial", "unix:%s,server=on,wait=on" % sock,
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

def send(line):
    s.sendall((line + "\n").encode())
    drain(0.1)

def sendkey(k):
    hmp("sendkey " + k)

def plain(b):
    return re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", b)

def count(pat):
    return len(re.findall(pat, plain(acc)))

def wait_count_inc(pat, sec):
    before = count(pat)
    end = time.time() + sec
    while time.time() < end:
        drain(1)
        if count(pat) > before:
            return True
    return False

rc = 1
try:
    # 1. shell prompt (ANSI-coloured — strip first)
    got_prompt = False
    end = time.time() + 120
    while time.time() < end and not got_prompt:
        drain(1)
        got_prompt = re.search(rb"m4sh ~>", plain(acc)) is not None
    if not got_prompt:
        print("[lvfilter] FAIL: no shell prompt")
    else:
        time.sleep(3)
        # 2. prepare /var/log/messages (mkdir on existing dir just
        # prints "failed" — harmless)
        send("mkdir /var")
        send("mkdir /var/log")
        send("cp /export/cfg/passwd.db /var/log/messages")
        drain(1)
        # 3. spawn logview
        send("spawn /bin/logview")
        if not wait_count_inc(rb"\[LOGVIEW\] surface ready", 15):
            print("[lvfilter] FAIL: logview did not start")
        else:
            print("[lvfilter] logview started")
            drain(2)
            # NOTE: no minimize step — the desktop terminal window is
            # not open in this boot, so keystrokes fall straight
            # through sprach_ga_key to the top GUI client.
            # 4. filter: / m4 s h  Enter  -> expect 2/3 lines
            sendkey("slash")
            drain(0.3)
            sendkey("m"); sendkey("4")
            sendkey("s"); sendkey("h")
            drain(0.3)
            sendkey("ret")
            filt = wait_count_inc(
                rb"\[LOGVIEW\] FILTER \"m4sh\" 2/3 lines", 10)
            print("[lvfilter] FILTER 2/3 lines:",
                  "OK" if filt else "FAIL")
            # 5. F jumps to next match
            sendkey("f")
            jump1 = wait_count_inc(rb"\[LOGVIEW\] JUMP #1 line 0", 8)
            print("[lvfilter] JUMP #1:", "OK" if jump1 else "FAIL")
            sendkey("f")
            jump2 = wait_count_inc(rb"\[LOGVIEW\] JUMP #2 line 1", 8)
            print("[lvfilter] JUMP #2:", "OK" if jump2 else "FAIL")
            # 6. Esc clears the filter
            sendkey("esc")
            off = wait_count_inc(rb"\[LOGVIEW\] FILTER off", 8)
            print("[lvfilter] FILTER off:", "OK" if off else "FAIL")
            ok = filt and jump1 and jump2 and off
            ok = ok and b"panic" not in acc.lower()
            print("[lvfilter] no_panic:",
                  b"panic" not in acc.lower())
            print("RESULT:", "PASS" if ok else "FAIL")
            rc = 0 if ok else 1
finally:
    log.close()
    qemu.kill()
sys.exit(rc)
