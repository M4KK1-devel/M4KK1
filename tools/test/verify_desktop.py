#!/usr/bin/env python3
"""Full desktop verification flow: MDM login → desktop → interactions.

Usage (inside WSL):  python3 tools/test/verify_desktop.py [stage]
Stages: login | desk | all   (default all)
"""
import sys
import time

sys.path.insert(0, "tools/test")
from qmp_client import QMP

q = QMP("/tmp/qmp.sock")
q.cmd("qmp_capabilities")


def wait_log(pat, timeout=30):
    import subprocess
    t0 = time.time()
    while time.time() - t0 < timeout:
        r = subprocess.run(["grep", "-ac", pat, "qemu_run.log"],
                           capture_output=True, text=True)
        if r.stdout.strip() not in ("", "0"):
            return True
        time.sleep(1)
    return False


def tap(key):
    q.cmd("input-send-event", {"events": [
        {"type": "key", "data": {"key": {"type": "qcode", "data": key},
                                 "down": True}}]})
    q.cmd("input-send-event", {"events": [
        {"type": "key", "data": {"key": {"type": "qcode", "data": key},
                                 "down": False}}]})


def stage_login():
    print("[1] MDM login: root/123456 ...")
    q.type_text("root"); time.sleep(0.5)
    tap("tab"); time.sleep(0.5)
    q.type_text("123456"); time.sleep(0.5)
    tap("ret")
    if wait_log("login OK", 15):
        print("    PASS: login OK logged")
    else:
        print("    FAIL: no login OK (check bad password path)")
        return False
    if wait_log("Copland ready", 20):
        print("    PASS: desktop session up")
    else:
        print("    FAIL: copland not ready")
        return False
    return True


def stage_desktop():
    print("[2] Desktop checks (serial evidence) ...")
    if wait_log("SPRACH. clock", 150):
        print("    PASS: sprach clock ticking")
        return True
    print("    FAIL: no sprach clock")
    return False


if __name__ == "__main__":
    stage = sys.argv[1] if len(sys.argv) > 1 else "all"
    ok = True
    if stage in ("login", "all"):
        ok = stage_login()
    if ok and stage in ("desk", "all"):
        ok = stage_desktop()
    sys.exit(0 if ok else 1)
