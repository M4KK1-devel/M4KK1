import gdb

class MemcpyWatch(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*0x117c00")
        self.hits = 0

    def stop(self):
        try:
            edx = int(gdb.parse_and_eval("$edx"))
            esp = int(gdb.parse_and_eval("$esp"))
            if 0x700000 <= edx < 0x1300000:
                self.hits += 1
                ret = int(gdb.parse_and_eval("*(unsigned int*)($esp+8)"))
                eax = int(gdb.parse_and_eval("$eax"))
                print("HIT dst=0x%X len=0x%X caller=0x%X" %
                      (edx, eax, ret))
                with open("/tmp/gdb_hits.txt", "a") as f:
                    f.write("dst=0x%X len=0x%X caller=0x%X\n" %
                            (edx, eax, ret))
                if self.hits >= 8:
                    return True
        except gdb.error:
            pass
        return False

# attach: target may not be stopped yet at script eval time
gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

MemcpyWatch()
gdb.execute("continue")

print("=== stopped ===")
try:
    with open("/tmp/gdb_hits.txt") as f:
        print(f.read())
except OSError:
    print("no hits file")
gdb.execute("info registers eip")
gdb.execute("quit")
