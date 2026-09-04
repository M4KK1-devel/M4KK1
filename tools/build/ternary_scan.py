#!/usr/bin/env python3
"""Loose scan: count if/else same-variable assignment sites per file."""
import re, os, glob

ROOT = r'F:\M4KK1'
TARGETS = []
for pat in ('usr/src/cmd/*.c', 'usr/src/cmd/altr/*.c',
            'usr/src/sprach/*.c', 'm4sh/core/*.c', 'm4sh/*.c',
            'sys/src/kernel/*.c', 'sys/src/mm/*.c',
            'sys/src/fs/yafs/core/yafs/*.c'):
    TARGETS += glob.glob(os.path.join(ROOT, pat))
TARGETS = sorted(set(TARGETS))

# just count "X = A; ... else ... X = B" proximity lines, manual review
total = 0
for path in TARGETS:
    try:
        lines = open(path, 'rb').read().decode('utf-8', 'replace').splitlines()
    except Exception:
        continue
    for i in range(len(lines) - 6):
        seg = ' '.join(lines[i:i+7])
        m1 = re.search(r'\b([A-Za-z_]\w*) = [^=][^;]*;', seg)
        if not m1:
            continue
        if re.search(r'\belse\b', seg) and \
           re.search(rf'\b{re.escape(m1.group(1))} = ', seg[m1.end():]):
            total += 1
            print(f'{os.path.relpath(path, ROOT)}:{i+1}: {m1.group(1)}')
print('TOTAL:', total)
