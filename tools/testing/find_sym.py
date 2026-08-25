import subprocess
out = subprocess.run(["readelf","-sW","usr/src/sprach/sprach_stack"],capture_output=True,text=True).stdout
target = 0x14713272
for line in out.splitlines():
    parts = line.split()
    if len(parts) >= 8 and parts[3] == "OBJECT":
        try:
            addr = int(parts[1], 16); size = int(parts[2])
        except ValueError:
            continue
        if addr <= target < addr + size:
            print("CONTAINS:", parts[1], parts[2], parts[7], "offset", hex(target-addr))
# also cptest
out2 = subprocess.run(["readelf","-sW","usr/src/cmd/cptest.elf"],capture_output=True,text=True).stdout
for line in out2.splitlines():
    parts = line.split()
    if len(parts) >= 8 and parts[3] == "OBJECT":
        try:
            addr = int(parts[1], 16); size = int(parts[2])
        except ValueError:
            continue
        if addr <= target < addr + size:
            print("CPTEST CONTAINS:", parts[1], parts[2], parts[7], "offset", hex(target-addr))
