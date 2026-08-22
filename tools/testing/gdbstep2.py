import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/usr/bin/copland")
gdb.execute("target remote :1234")

# 1) catch copland's fork return: int80 site 0x60054f, next insn 0x600551
gdb.execute("break *0x600551")
gdb.execute("continue")          # first hit = parent resume or child resume
pc = int(gdb.parse_and_eval("$pc"))
print("HIT pc=0x%X esp=0x%X eax=0x%X" % (pc, int(gdb.parse_and_eval("$esp")), int(gdb.parse_and_eval("$eax"))), flush=True)

# step forward to see whether this is parent (eax=pid) or child (eax=0)
for i in range(25):
    gdb.execute("stepi", to_string=True)
    pc = int(gdb.parse_and_eval("$pc"))
    eax = int(gdb.parse_and_eval("$eax"))
    esp = int(gdb.parse_and_eval("$esp"))
    print("s%02d pc=0x%X esp=0x%X eax=0x%X" % (i, pc, esp, eax), flush=True)
    if pc == 3:
        print("=== FAULT ===", flush=True)
        break
gdb.execute("quit")
