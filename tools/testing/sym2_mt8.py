import subprocess
for img, addrs in [("usr/src/cmd/mdm.elf", [0x801181, 0x8025bf, 0x80118a]),
                   ("usr/src/cmd/copland.elf", [0x6019e4])]:
    out = subprocess.run(["wsl","-d","archlinux","nm","/mnt/f/M4KK1/"+img],
                         capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3:
            try: syms.append((int(p[0],16), p[2]))
            except ValueError: pass
    syms.sort()
    for a in addrs:
        best = None
        for s, n in syms:
            if s <= a: best = (s, n)
            else: break
        print(img.split("/")[-1], hex(a), best[1]+"+0x%x"%(a-best[0]) if best else "?")
