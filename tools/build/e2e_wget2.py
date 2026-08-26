#!/usr/bin/env python3
"""E2E wget with pcap; graceful QEMU shutdown so the dump flushes."""
import http.server, os, socket, socketserver, subprocess, sys, threading, time

os.chdir("/mnt/f/M4KK1")
isos = sorted(f for f in os.listdir("output") if f.endswith("full-test.iso"))
iso = os.path.join("output", isos[-1])
pcap = "/tmp/wget2.pcap"
try:
    os.unlink(pcap)
except OSError:
    pass

PAGE = b"Hello from host! M4KK1 wget E2E test page.\n" * 16

class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(PAGE)))
        self.end_headers()
        self.wfile.write(PAGE)
    def log_message(self, *a):
        pass

srv = socketserver.TCPServer(("127.0.0.1", 0), H)
port = srv.server_address[1]
threading.Thread(target=srv.serve_forever, daemon=True).start()
print("http server on 127.0.0.1:%d" % port)

sock_path = "/tmp/m4k_wget.sock"
try:
    os.unlink(sock_path)
except OSError:
    pass

qemu = subprocess.Popen(
    ["qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
     "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
     "-object", "filter-dump,id=f1,netdev=n0,file=%s" % pcap,
     "-serial", "unix:%s,server=on,wait=off" % sock_path,
     "-display", "none"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print("qemu pid", qemu.pid)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
for _ in range(50):
    try:
        s.connect(sock_path)
        break
    except OSError:
        time.sleep(0.3)
else:
    print("serial connect failed"); qemu.terminate(); sys.exit(1)
print("serial connected")

buf = b""
def pump():
    global buf
    while True:
        try:
            d = s.recv(4096)
        except OSError:
            return
        if not d:
            return
        buf += d

threading.Thread(target=pump, daemon=True).start()

def wait_for(pat, timeout=60):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if pat.encode() in buf:
            return True
        time.sleep(0.5)
    return False

wait_for("Entering main loop", 90)
time.sleep(8)
s.send(b"\r")
time.sleep(1)
s.send(b"ping 10.0.2.2\r")
time.sleep(8)
s.send(b"wget http://10.0.2.2:%d/hello.txt out.txt\r" % port)
time.sleep(3)
s.send(b"cat out.txt\r")
ok1 = wait_for("wget: saved", 45)
ok2 = wait_for("Hello from host", 30)
print("wget-saved-line:", ok1)
print("file-content:", ok2)
text = buf.decode("utf-8", "replace")
i = text.find("ping 10.0.2.2")
print("--- from ping ---")
print(text[i:i+600] if i >= 0 else "(ping output not found)")
qemu.terminate()
try:
    qemu.wait(timeout=10)
except subprocess.TimeoutExpired:
    qemu.kill()
srv.shutdown()
print("pcap size:", os.path.getsize(pcap))
sys.exit(0 if (ok1 and ok2) else 1)
