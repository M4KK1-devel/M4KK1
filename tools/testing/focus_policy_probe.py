#!/usr/bin/env python3
"""Focus-policy probe: drive QEMU (serial socket + QMP mouse), assert
on the serial stream.  Validates the sprach focus policy round:

  P1  new-window focus:  'spawn /bin/info' in the serial shell ->
      "[SPRACH] FOCUS NEW CLIENT 380x240"
  P2  click-to-focus (non-client area): QMP-click info's title bar ->
      "[SPRACH] FOCUS CLICK app 380x240"
  P3  close-foreground fallback: click info's red close box ->
      "[SPRACH] APP CLOSE" then "[SPRACH] FOCUS FALLBACK ..."
  P4  self-exit fallback: re-spawn info, press q (PS/2 keyboard ->
      sprach -> ga mailbox; the app quits and frees its slot) ->
      "[SPRACH] FOCUS FG GONE" then "[SPRACH] FOCUS FALLBACK ..."
  P5  demo-window minimize fallback: click Win1's minimize box ->
      "[SPRACH] MIN 0" then "[SPRACH] FOCUS FALLBACK ..."

Serial wiring: the full-test ISO autologs into m4sht, a shell that
polls COM1 directly.  -serial file: is output-only, so the probe uses
a unix-socket chardev: serial output is read from the socket AND typed
bytes are written back into it (guest COM1 RX).  QMP carries only the
mouse (rel moves + clicks) and the PS/2 'q' keypress for P4.

Run inside WSL, from the repo root:
    python3 tools/testing/focus_policy_probe.py [iso]
"""
import glob
import json
import os
import socket
import subprocess
import sys
import threading
import time

SER_SOCK = '/tmp/focus_ser.sock'
QMP_SOCK = '/tmp/focus_qmp.sock'


class Serial:
    """Reader thread over the serial unix socket; log tail via .text."""

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
        t = threading.Thread(target=self._reader, daemon=True)
        t.start()

    def _reader(self):
        while True:
            d = self.s.recv(4096)
            if not d:
                break
            with self.lock:
                self.buf.append(d)

    def text(self):
        with self.lock:
            return b''.join(self.buf).decode('utf-8', errors='replace')

    def count(self, marker):
        return self.text().count(marker)

    def wait(self, marker, timeout=25, min_count=1):
        t0 = time.time()
        while time.time() - t0 < timeout:
            if self.count(marker) >= min_count:
                return True
            time.sleep(0.5)
        return False

    def send(self, s):
        self.f.write(s.encode())


class QMP:
    def __init__(self, sock):
        self.s = socket.socket(socket.AF_UNIX)
        self.s.connect(sock)
        self.f = self.s.makefile('rw')
        self.f.readline()
        self.cmd('qmp_capabilities')

    def cmd(self, execute, arguments=None):
        o = {'execute': execute}
        if arguments:
            o['arguments'] = arguments
        self.f.write(json.dumps(o) + '\n')
        self.f.flush()
        while True:
            r = json.loads(self.f.readline())
            if 'return' in r or 'error' in r:
                return r

    def ev(self, evs):
        return self.cmd('input-send-event', {'events': evs})

    def key(self, k, down=True):
        return self.ev([{'type': 'key',
                         'data': {'key': {'type': 'qcode', 'data': k},
                                  'down': down}}])

    def rel(self, dx, dy):
        self.ev([{'type': 'rel', 'data': {'axis': 'x', 'value': dx}}])
        self.ev([{'type': 'rel', 'data': {'axis': 'y', 'value': dy}}])

    def click(self):
        self.ev([{'type': 'btn',
                  'data': {'down': True, 'button': 'left'}}])
        time.sleep(0.12)
        self.ev([{'type': 'btn',
                  'data': {'down': False, 'button': 'left'}}])


def main():
    iso = sys.argv[1] if len(sys.argv) > 1 else \
        sorted(glob.glob('output/m4kk1_*full*.iso') or
               glob.glob('output/m4kk1_*.iso'))[-1]

    for f in (SER_SOCK, QMP_SOCK):
        try:
            os.unlink(f)
        except FileNotFoundError:
            pass

    print(f'[boot] {iso}')
    qemu = subprocess.Popen(
        ['qemu-system-i386', '-cdrom', iso, '-m', '512', '-vga', 'std',
         '-display', 'none',
         '-serial', f'unix:{SER_SOCK},server=on,wait=off',
         '-qmp', f'unix:{QMP_SOCK},server=on,wait=off', '-no-reboot'],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(60):
            if os.path.exists(QMP_SOCK):
                break
            time.sleep(0.5)
        else:
            print('FAIL: QMP socket never appeared')
            return 1
        time.sleep(1)
        ser = Serial(SER_SOCK)
        q = QMP(QMP_SOCK)

        print('[0] wait for sprach main loop ...')
        if not ser.wait('[SPRACH] Entering main loop', 60):
            print('FAIL: sprach did not start')
            return 1
        print('[0b] wait for the m4sht prompt ...')
        if not ser.wait('available commands', 30):
            print('FAIL: serial shell never started')
            return 1
        time.sleep(3)

        # ── P1: new-window focus (spawn a foreign client) ──
        base_new = ser.count('[SPRACH] FOCUS NEW CLIENT')
        ser.send('spawn /bin/info\r')
        if not ser.wait('[SPRACH] FOCUS NEW CLIENT', 25,
                        min_count=base_new + 1):
            print('FAIL P1: no FOCUS NEW CLIENT after spawn /bin/info')
            return 1
        print('P1 new-window focus: PASS')

        # info window is created at (60,60), 380x240; title band rows
        # 60..77.  Title-bar centre ≈ (250,68); red close box ≈ (431,69).
        # ── P2: click the non-client area (title bar) → focus+raise ──
        base_click = ser.count('[SPRACH] FOCUS CLICK app')
        q.rel(250, 68)          # rel move: 1:1 at speed 1
        time.sleep(0.4)
        q.click()
        if not ser.wait('[SPRACH] FOCUS CLICK app', 10,
                        min_count=base_click + 1):
            print('FAIL P2: no FOCUS CLICK app after title-bar click')
            return 1
        print('P2 click-to-focus (title bar): PASS')

        # ── P3: close the foreground client → focus fallback ──
        base_close = ser.count('[SPRACH] APP CLOSE')
        base_fb = ser.count('[SPRACH] FOCUS FALLBACK')
        q.rel(431 - 250, 0)     # onto the red close box
        time.sleep(0.4)
        q.click()
        if not ser.wait('[SPRACH] APP CLOSE', 10,
                        min_count=base_close + 1):
            print('FAIL P3: no APP CLOSE after close-box click')
            return 1
        if not ser.wait('[SPRACH] FOCUS FALLBACK', 10,
                        min_count=base_fb + 1):
            print('FAIL P3: no FOCUS FALLBACK after APP CLOSE')
            return 1
        print('P3 close-foreground fallback: PASS')

        # ── P4: foreground client exits on its own (q key) ──
        ser.send('spawn /bin/info\r')
        if not ser.wait('[SPRACH] FOCUS NEW CLIENT', 25,
                        min_count=base_new + 2):
            print('FAIL P4: info did not re-spawn')
            return 1
        time.sleep(1.5)
        base_gone = ser.count('[SPRACH] FOCUS FG GONE')
        q.key('q', True)        # PS/2 → sprach → ga mailbox → info quits
        q.key('q', False)
        if not ser.wait('[SPRACH] FOCUS FG GONE', 15,
                        min_count=base_gone + 1):
            print('FAIL P4: no FOCUS FG GONE after self-exit')
            return 1
        if not ser.wait('[SPRACH] FOCUS FALLBACK', 15,
                        min_count=base_fb + 2):
            print('FAIL P4: no FOCUS FALLBACK after FG GONE')
            return 1
        print('P4 self-exit fallback: PASS')

        # ── P5: minimize the active demo window → focus fallback ──
        # The P4 fallback activated Win3 (active=2).  Win1's title bar
        # at (268,99) is covered by Win1 only → FOCUS CLICK window 0;
        # its MIN box sits at (167,99).
        base_min = ser.count('[SPRACH] FOCUS CLICK window')
        q.rel(268, 99)
        time.sleep(0.4)
        q.click()
        if not ser.wait('[SPRACH] FOCUS CLICK window', 10,
                        min_count=base_min + 1):
            print('FAIL P5: could not activate Win1')
            return 1
        base_minfb = ser.count('[SPRACH] FOCUS FALLBACK')
        q.rel(167 - 268, 0)     # onto Win1's MIN box
        time.sleep(0.4)
        q.click()
        if not ser.wait('[SPRACH] MIN 0', 10):
            print('FAIL P5: no MIN 0 after minimize-box click')
            return 1
        if not ser.wait('[SPRACH] FOCUS FALLBACK', 10,
                        min_count=base_minfb + 1):
            print('FAIL P5: no FOCUS FALLBACK after MIN')
            return 1
        print('P5 minimize fallback: PASS')

        print('ALL FOCUS PROBES PASS')
        return 0
    finally:
        # always archive the serial stream for post-mortem
        try:
            with open('/tmp/focus_probe_serial.log', 'w') as f:
                f.write(ser.text())
        except Exception:
            pass
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()


if __name__ == '__main__':
    sys.exit(main())
