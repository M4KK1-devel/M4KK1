import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("target remote :1234")

state = {"n": 0}

class W(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*(unsigned int*)0x807104",
                         gdb.BP_WATCHPOINT, gdb.WP_WRITE)

    def stop(self):
        try:
            v = int(gdb.parse_and_eval("*(unsigned int*)0x807104"))
        except gdb.error:
            return False
        state["n"] += 1
        print("WRITE#%d -> 0x%08X" % (state["n"], v), flush=True)
        if (v & 0xFFFF0000) != 0xDEAD0000 and state["n"] > 6:
            print("=== CORRUPTION WRITE ===", flush=True)
            gdb.execute("info registers eip esp ebp edi esi ecx eax")
            gdb.execute("info symbol $eip")
            return True
        return False

W()
gdb.execute("continue")
gdb.execute("x/6i $eip-12")
gdb.execute("quit")
