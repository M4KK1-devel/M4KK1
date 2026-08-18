#!/usr/bin/env python3
"""Full interactive verification flow for the M4KK1 desktop.

Run inside WSL:  python3 tools/test/verify_flow.py
Requires: QEMU already running with -qmp unix:/tmp/qmp.sock and the
guest fully booted (Sprach clock ticking).
"""
import json
import socket
import subprocess
import sys
import time


class QMP:
    def __init__(self, sock='/tmp/qmp.sock'):
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

    def chord(self, *keys):
        for k in keys:
            self.key(k, True)
            time.sleep(0.15)
        for k in reversed(keys):
            self.key(k, False)
            time.sleep(0.05)

    def type_str(self, s):
        for ch in s:
            k = ch
            if ch == ' ':
                k = 'spc'
            elif ch == '/':
                k = 'slash'
            elif ch == '.':
                k = 'dot'
            elif ch == '-':
                k = 'minus'
            shift = ch.isupper()
            if shift:
                self.key('shift', True)
            self.key(k, True)
            self.key(k, False)
            if shift:
                self.key('shift', False)
            time.sleep(0.05)

    def enter(self):
        self.key('ret', True)
        self.key('ret', False)

    def mouse_move(self, dx, dy):
        self.ev([{'type': 'rel', 'data': {'axis': 'x', 'value': dx}}])
        self.ev([{'type': 'rel', 'data': {'axis': 'y', 'value': dy}}])

    def mouse_click(self):
        self.ev([{'type': 'btn', 'data': {'down': True, 'button': 'left'}}])
        time.sleep(0.1)
        self.ev([{'type': 'btn', 'data': {'down': False, 'button': 'left'}}])

    def dump(self, path):
        return self.cmd('screendump', {'filename': path})


def wait_log(pattern, timeout=60, log='qemu_run.log'):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            out = subprocess.run(['grep', '-ac', pattern, log],
                                 capture_output=True, text=True)
            if int(out.stdout.strip() or 0) > 0:
                return True
        except Exception:
            pass
        time.sleep(2)
    return False


def main():
    q = QMP()

    print('[1] wait for Sprach clock ...')
    if not wait_log('SPRACH] clock'):
        print('FAIL: sprach clock not ticking')
        return 1

    print('[2] Ctrl+Alt+T ...')
    q.chord('ctrl', 'alt', 't')
    if not wait_log('terminal window registered', 30):
        print('FAIL: terminal did not register')
        return 1
    print('    terminal registered OK')

    time.sleep(2)

    print('[2b] click terminal title bar to focus ...')
    # move mouse to terminal title bar (60..740, 40..58): rel moves
    # from current (400,300) to ~(150, 46)
    q.mouse_move(-250, -254)
    time.sleep(0.5)
    q.mouse_click()
    time.sleep(1)

    print('[3] type "ls /" + Enter ...')
    q.type_str('ls /')
    time.sleep(0.5)
    q.enter()
    time.sleep(4)   # let m4shg list the root

    print('[4] type "echo VERIFIED_42" + Enter ...')
    q.type_str('echo VERIFIED_42')
    time.sleep(0.5)
    q.enter()
    time.sleep(4)

    print('[5] screendump + OCR ...')
    q.dump('/mnt/f/M4KK1/verify.ppm')
    time.sleep(1)
    out = subprocess.run(
        ['python3', 'tools/test/term_ocr.py', 'verify.ppm'],
        capture_output=True, text=True)
    print(out.stdout)
    open('verify_ocr.txt', 'w').write(out.stdout)
    if 'VERIFIED' in out.stdout and ('bin' in out.stdout or 'etc' in out.stdout):
        print('PASS: shell echo + ls output visible in terminal')
        return 0
    print('FAIL: expected text not found')
    return 1


if __name__ == '__main__':
    sys.exit(main())
