import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break syscall_m4k.c:586")
gdb.execute("continue")
try:
    buf = int(gdb.parse_and_eval("elf_buf"))
    total = int(gdb.parse_and_eval("total"))
    n = int(gdb.parse_and_eval("n"))
    inf = gdb.selected_inferior()
    head = bytes(inf.read_memory(buf, 16))
    print("AFTER-READ1 elf_buf=0x%X total=%d n=%d" % (buf, total, n))
    print("head: " + " ".join("%02x" % b for b in head))
except gdb.error as e:
    print("gdb error: %s" % e)
gdb.execute("detach")
