#!/usr/bin/env python3
"""Diagnostic: boot, ps, spawn info, watch serial for 30s."""
import glob, os, socket, subprocess, sys, threading, time

SER_SOCK = '/tmp/diag_ser.sock'
QMP_SOCK = '/tmp/diag_qmp.sock'

class Serial:
    def __init__(self, path):
        self.buf = []
        self.lock = threading.Lock()
        for _ in range(60):
            try:
                self.s = socket.socket(socket.AF_UNIX)
                self.s.connect(path)
                break
            except (FileNotFoundError, ConnectionRefusedError):
                time.sleep(0.5)
        else:
            raise RuntimeError('serial socket never came up')
        self.f = self.s.makefile('rwb', buffering=0)
        threading.Thread(target=self._reader, daemon=True).start()
    def _reader(self):
        while True:
            d = self.s.recv(4096)
            if not d: break
            with self.lock:
                self.buf.append(d)
    def text(self):
        with self.lock:
            return b''.join(self.buf).decode('utf-8', errors='replace')
    def send(self, s):
        self.f.write(s.encode())

iso = sys.argv[1] if len(sys.argv) > 1 else \
    sorted(glob.glob('output/m4kk1_*full*.iso'))[-1]

for f in (SER_SOCK, QMP_SOCK):
    try: os.unlink(f)
    except FileNotFoundError: pass

qemu = subprocess.Popen(
    ['qemu-system-i386', '-cdrom', iso, '-m', '512', '-vga', 'std',
     '-display', 'none',
     '-serial', f'unix:{SER_SOCK},server=on,wait=off',
     '-qmp', f'unix:{QMP_SOCK},server=on,wait=off', '-no-reboot'],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(60):
        if os.path.exists(QMP_SOCK): break
        time.sleep(0.5)
    time.sleep(1)
    ser = Serial(SER_SOCK)
    t0 = time.time()
    while time.time() - t0 < 60 and 'Entering main loop' not in ser.text():
        time.sleep(0.5)
    t0 = time.time()
    while time.time() - t0 < 30 and 'available commands' not in ser.text():
        time.sleep(0.5)
    time.sleep(3)
    print('--- sending ps ---', flush=True)
    ser.send('ps\r')
    time.sleep(5)
    print('--- sending spawn /bin/info ---', flush=True)
    ser.send('spawn /bin/info\r')
    time.sleep(30)
    txt = ser.text()
    with open('/tmp/diag_serial.log', 'w') as f:
        f.write(txt)
    print('=== last 60 lines ===')
    print('\n'.join(txt.splitlines()[-60:]))
finally:
    qemu.terminate()
    try: qemu.wait(timeout=5)
    except subprocess.TimeoutExpired: qemu.kill()
