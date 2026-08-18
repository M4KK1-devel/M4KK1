#!/usr/bin/env python3
"""QMP client for M4KK1 interactive verification.

Usage:
  qmp_client.py <socket> wait-text <pattern> <timeout_s>   (echoes serial? no — just sleep-wait, pattern check done by caller)
  qmp_client.py <socket> key <chord>          e.g. key ctrl-alt-t
  qmp_client.py <socket> type <text>          types text with press/release per char
  qmp_client.py <socket> mouse-move <dx> <dy>
  qmp_client.py <socket> mouse-click          (btn left press+release at current pos)
  qmp_client.py <socket> screendump <path.ppm>

Connects to a QEMU unix-socket QMP, negotiates capabilities, sends the
command, prints replies, exits.
"""
import json
import socket
import sys
import time


class QMP:
    def __init__(self, path):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(path)
        self.f = self.s.makefile("rw", encoding="utf-8", newline="\n")
        # greeting
        self.read_msg()

    def read_msg(self, timeout=5.0):
        self.s.settimeout(timeout)
        line = self.f.readline()
        if not line:
            return None
        try:
            return json.loads(line)
        except ValueError:
            return {"raw": line}

    def cmd(self, execute, arguments=None):
        obj = {"execute": execute}
        if arguments is not None:
            obj["arguments"] = arguments
        self.f.write(json.dumps(obj) + "\n")
        self.f.flush()
        # skip events until we get the command reply
        while True:
            r = self.read_msg()
            if r is None:
                return {"error": "no reply"}
            if "return" in r or "error" in r:
                return r

    def key_events(self, keys, down):
        # QEMU >= 6: key must be a QKeyCode object, not a bare string
        return [{"type": "key",
                 "data": {"key": {"type": "qcode", "data": k},
                          "down": down}}
                for k in keys]

    def chord(self, chord):
        keys = chord.split("-")
        ev = self.key_events(keys, True) + self.key_events(list(reversed(keys)), False)
        return self.cmd("input-send-event", {"events": ev})

    def type_text(self, text):
        for ch in text:
            key = None
            if ch == " ":
                key = "spc"
            elif ch == "\n":
                key = "ret"
            elif ch == "/":
                key = "slash"
            elif ch == "-":
                key = "minus"
            elif ch == ".":
                key = "dot"
            elif ch.isupper():
                key = ch.lower()
            else:
                key = ch
            shift = ch.isupper()
            down = [{"type": "key",
                     "data": {"key": {"type": "qcode", "data": key},
                              "down": True}}]
            up = [{"type": "key",
                   "data": {"key": {"type": "qcode", "data": key},
                            "down": False}}]
            if shift:
                down.insert(0, {"type": "key",
                                "data": {"key": {"type": "qcode", "data": "shift"},
                                         "down": True}})
                up.append({"type": "key",
                           "data": {"key": {"type": "qcode", "data": "shift"},
                                    "down": False}})
            self.cmd("input-send-event", {"events": down})
            time.sleep(0.03)
            self.cmd("input-send-event", {"events": up})
            time.sleep(0.08)

    def mouse_move(self, dx, dy):
        ev = [{"type": "rel", "data": {"axis": "x", "value": dx}},
              {"type": "rel", "data": {"axis": "y", "value": dy}}]
        return self.cmd("input-send-event", {"events": ev})

    def mouse_click(self):
        dn = [{"type": "btn", "data": {"down": True, "button": "left"}}]
        up = [{"type": "btn", "data": {"down": False, "button": "left"}}]
        r1 = self.cmd("input-send-event", {"events": dn})
        time.sleep(0.1)
        r2 = self.cmd("input-send-event", {"events": up})
        return r1, r2


def main():
    sock = sys.argv[1]
    action = sys.argv[2]
    q = QMP(sock)
    q.cmd("qmp_capabilities")
    if action == "key":
        print(q.chord(sys.argv[3]))
    elif action == "type":
        q.type_text(sys.argv[3])
        print("typed")
    elif action == "mouse-move":
        print(q.mouse_move(int(sys.argv[3]), int(sys.argv[4])))
    elif action == "mouse-click":
        print(q.mouse_click())
    elif action == "screendump":
        print(q.cmd("screendump", {"filename": sys.argv[3]}))
    else:
        print("unknown action")
        sys.exit(1)


if __name__ == "__main__":
    main()
