import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

GF = 0x235b20  # g_syscall_user_frame

state = {"fork_seen": False, "n": 0}

class W(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*0x235b20", gdb.BP_WATCHPOINT, gdb.WP_WRITE)
        self.val = None

    def stop(self):
        try:
            v = int(gdb.parse_and_eval("*(unsigned int*)0x235b20"))
        except gdb.error:
            return False
        if self.val is None:
            self.val = v
            print("INIT frame[0]=0x%X" % v, flush=True)
            return False
        if v != self.val:
            self.val = v
            state["n"] += 1
            pc = int(gdb.parse_and_eval("$pc"))
            print("CHANGE #%d: frame[0]=0x%X pc=0x%X" % (state["n"], v, pc), flush=True)
            if state["n"] > 30:
                return True
        return False

W()
gdb.execute("continue")
gdb.execute("quit")
