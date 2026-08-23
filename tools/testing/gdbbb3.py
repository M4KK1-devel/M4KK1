import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

class FG(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*0x119b30")

    def stop(self):
        try:
            bb = int(gdb.parse_and_eval("*(unsigned int*)0x28b780"))
            print("back_buffer = 0x%X" % bb, flush=True)
            print("in buddy zone (>0x3000000):", bb >= 0x3000000, flush=True)
            with open("/tmp/gdb_bb2.txt", "w") as f:
                f.write("0x%X\n" % bb)
            return True
        except gdb.error as e:
            print("err", e, flush=True)
            return False

FG()
gdb.execute("continue")
gdb.execute("quit")
