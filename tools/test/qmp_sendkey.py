#!/usr/bin/env python3
"""Send a Ctrl+Alt+T chord to a QEMU instance via QMP.

Usage:  ( sleep 14 && python3 qmp_sendkey.py ctrl-alt-t ) | qemu ... -qmp stdio

The parenthesised group's stdout is piped to QEMU's stdin, so QMP
commands are written to stdout; QEMU's replies go to its own stdout
(usually /dev/null in a test harness) and are not needed for the
fire-and-forget key chord.
"""
import json
import sys
import time

def qmp_send(obj):
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()

if __name__ == "__main__":
    chord = sys.argv[1] if len(sys.argv) > 1 else "ctrl-alt-t"
    keys = chord.split("-")
    qmp_send({"execute": "qmp_capabilities"})
    time.sleep(0.2)
    events = []
    for k in keys:
        events.append({"type": "key", "data": {"key": k, "down": True}})
    for k in reversed(keys):
        events.append({"type": "key", "data": {"key": k, "down": False}})
    qmp_send({"execute": "input-send-event",
              "arguments": {"events": events}})
    time.sleep(0.2)
    qmp_send({"execute": "quit"})
