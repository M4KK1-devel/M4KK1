import subprocess
out = subprocess.run(["wsl","-d","archlinux","nm","/mnt/f/M4KK1/output/m4kk1.krn"],
                     capture_output=True, text=True).stdout
syms = []
for line in out.splitlines():
    parts = line.split()
    if len(parts) == 3:
        try:
            syms.append((int(parts[0],16), parts[2]))
        except ValueError:
            pass
syms.sort()
def sym(a):
    lo, hi = 0, len(syms)-1
    best = None
    for s, n in syms:
        if s <= a:
            best = (s, n)
        else:
            break
    return "%s+0x%x" % (best[1], a-best[0]) if best else "?"

for a in [0x00103814, 0x00801181, 0x00103a6a, 0x00117d61, 0x00119559, 0x008025bf]:
    print(hex(a), sym(a))
