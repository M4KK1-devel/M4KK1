import socket
import time

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.settimeout(3)
try:
    s.connect("/tmp/m4k_mon.sock")
    print("connected", flush=True)
    s.sendall(b"info status\n")
    time.sleep(1)
    data = s.recv(4096)
    print("recv:", data[:200], flush=True)
except Exception as e:
    print("ERR", e, flush=True)
