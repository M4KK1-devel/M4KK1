import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/usr/bin/copland")
gdb.execute("target remote :1234")

gdb.execute("break *0x60054f")
gdb.execute("continue")

gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("break *0x117cc0")   # mkrn_memcpy entry
gdb.execute("continue")
# at function entry (before push ebp): args at [esp+4],[esp+8],[esp+12]
esp = int(gdb.parse_and_eval("$esp"))
a1 = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % (esp + 4)))
a2 = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % (esp + 8)))
a3 = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % (esp + 12)))
print("memcpy(dst=0x%X, src=0x%X, n=0x%X)" % (a1, a2, a3), flush=True)
gdb.execute("quit")
