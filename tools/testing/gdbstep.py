import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break *0x101480")   # mkrn_fork_child_restore
gdb.execute("continue")
print("=== child at trampoline ===", flush=True)
for i in range(80):
    pc = int(gdb.parse_and_eval("$pc"))
    sp = int(gdb.parse_and_eval("$esp"))
    try:
        eax = int(gdb.parse_and_eval("$eax"))
    except gdb.error:
        eax = -1
    print("step %02d: pc=0x%X esp=0x%X eax=0x%X" % (i, pc, sp, eax), flush=True)
    if pc == 3 or pc < 0x1000:
        print("=== FAULT PC REACHED ===", flush=True)
        break
    gdb.execute("stepi", to_string=True)
gdb.execute("quit")
