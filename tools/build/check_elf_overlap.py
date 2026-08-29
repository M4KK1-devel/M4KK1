#!/usr/bin/env python3
"""Scan all M4KK1 user ELFs for flat-address-space LOAD overlaps.

The kernel has no MMU: every user ELF runs at its link-time address
and two live processes whose LOAD segments overlap WILL corrupt each
other (see the fm@c0xD00000 vs cptest@0xE00000 .bss smear).  This
guard parses every ELF listed below with readelf and fails (exit 1)
when any two LOAD ranges [vaddr, vaddr+memsz) intersect.

Also checks each ELF stays inside the user window
[0x400000, 0x3000000) and does not touch the reserved ramdisk
[0x2000000, 0x3000000).
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# (display name, path, concurrent) — every ELF that can be exec'd.
# concurrent=True: the GUI/desktop family — these processes are alive
# at the same time (copland + sprach + terminal + cptest + fm + altr +
# calcg + m4shg), so ANY pairwise LOAD overlap is fatal and fails the
# build.  concurrent=False: legacy / mutually-exclusive programs
# (shell-zone share at 0x800000, pcc's fat console image) — overlaps
# among them or with the concurrent set are reported as warnings only,
# matching the long-standing shipped behaviour.
ELFS = [
    ("copland", "usr/src/cmd/copland.elf", True),
    ("sprach", "usr/src/sprach/sprach_stack", True),
    ("terminal", "usr/src/cmd/terminal.elf", True),
    ("cptest", "usr/src/cmd/cptest.elf", True),
    ("fm", "usr/src/cmd/fm.elf", True),
    ("altr", "usr/src/cmd/altr.elf", True),
    ("calcg", "usr/src/cmd/calc_gui.elf", True),
    ("m4shg", "m4sh/m4shg.elf", True),
    ("m4sh", "m4sh/m4sh.elf", False),
    ("m4sht", "m4sh/m4sht.elf", False),
    ("login", "usr/src/cmd/login.elf", False),
    ("mdm", "usr/src/cmd/mdm.elf", False),
    ("mdm_mini", "usr/src/cmd/mdm_mini.elf", False),
    ("pcc", "usr/src/tools/pcc/pcc.elf", False),
]

USER_WIN_LO = 0x400000
USER_WIN_HI = 0x2000000   # ramdisk starts here — user ELFs must stay below
ABS_HI = 0x3000000


def load_segments(path: Path):
    """Return [(vaddr, memsz), ...] for PT_LOAD segments."""
    out = subprocess.run(
        ["readelf", "-lW", str(path)],
        capture_output=True, text=True, check=True).stdout
    segs = []
    for line in out.splitlines():
        m = re.match(r"\s*LOAD\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)"
                     r"\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)"
                     r"\s+(0x[0-9a-fA-F]+)", line)
        if m:
            vaddr = int(m.group(2), 16)
            memsz = int(m.group(5), 16)
            segs.append((vaddr, memsz))
    return segs


def main():
    spans = []          # (name, concurrent, lo, hi)
    errors = []
    warnings = []

    for name, rel, conc in ELFS:
        p = ROOT / rel
        if not p.exists():
            continue                    # optional ELFs (m4sht etc.)
        try:
            segs = load_segments(p)
        except subprocess.CalledProcessError:
            errors.append(f"{name}: readelf failed")
            continue
        if not segs:
            errors.append(f"{name}: no PT_LOAD (broken ELF?)")
            continue
        for lo, sz in segs:
            hi = lo + sz
            spans.append((name, conc, lo, hi))
            if lo < USER_WIN_LO:
                errors.append(
                    f"{name}: segment 0x{lo:x} below user window"
                    f" (kernel space)")
            if hi > USER_WIN_HI and lo < ABS_HI:
                errors.append(
                    f"{name}: segment [0x{lo:x},0x{hi:x}) reaches into"
                    f" reserved ramdisk [0x2000000,0x3000000)")

    # pairwise overlap check
    for i in range(len(spans)):
        for j in range(i + 1, len(spans)):
            n1, c1, lo1, hi1 = spans[i]
            n2, c2, lo2, hi2 = spans[j]
            if n1 == n2:
                continue                 # same ELF's own segments
            if lo1 < hi2 and lo2 < hi1:
                msg = (f"OVERLAP {n1} [0x{lo1:x},0x{hi1:x}) X "
                       f"{n2} [0x{lo2:x},0x{hi2:x})")
                if c1 and c2:
                    errors.append(msg + "  [concurrent — FATAL]")
                else:
                    warnings.append(msg + "  [legacy/mutex — tolerated]")

    for w in warnings:
        print("  WARN " + w)
    if errors:
        print("=== ELF OVERLAP CHECK: FAIL ===")
        for e in errors:
            print("  " + e)
        return 1
    print("=== ELF overlap check: OK "
          f"({len(spans)} segments, {len(set(n for n, _, _, _ in spans))}"
          f" ELFs, {len(warnings)} tolerated legacy overlaps)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
