#!/usr/bin/env python3
"""Print exact 6-line context for each candidate site for manual
ternary review (same file list as ternary_scan)."""
import re, os, glob

ROOT = r'F:\M4KK1'
TARGETS = []
for pat in ('m4sh/core/*.c', 'usr/src/cmd/*.c', 'usr/src/cmd/altr/*.c',
            'usr/src/sprach/*.c'):
    TARGETS += glob.glob(os.path.join(ROOT, pat))
TARGETS = sorted(set(TARGETS))

for path in TARGETS:
    lines = open(path, 'rb').read().decode('utf-8', 'replace').splitlines()
    for i in range(len(lines)):
        if re.match(r'\s*else\b', lines[i]):
            # look back for "X = ...;" within 4 lines and matching
            # "X = ...;" in the else within 3 lines
            for back in range(1, 5):
                j = i - back
                if j < 0: break
                m = re.search(r'([A-Za-z_]\w*) = ([^=;][^;]*);',
                              lines[j])
                if not m: continue
                var = m.group(1)
                for fwd in range(1, 4):
                    k = i + fwd
                    if k >= len(lines): break
                    if re.match(r'\s*else\b|\s*\}', lines[k]): continue
                    if re.search(rf'\b{var} = ', lines[k]):
                        print(f'--- {os.path.relpath(path, ROOT)} '
                              f'{j+1}-{k+1} var={var}')
                        for t in range(max(0, j-2), min(len(lines), k+2)):
                            print(f'{t+1:5}| {lines[t]}')
                        print()
                        break
                    break
                break
