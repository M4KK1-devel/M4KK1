import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/usr/bin/copland")
gdb.execute("target remote :1234")

gdb.execute("break *0x60054f")
gdb.execute("continue")   # copland fork

gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
# step until we reach the call site 0x102947 (bounded)
gdb.execute("break *0x102947")
gdb.execute("continue")
esp = int(gdb.parse_and_eval("$esp"))
a1 = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % esp))
a2 = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % (esp + 4)))
a3 = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % (esp + 8)))
print("at callsite: [esp]=0x%X [esp+4]=0x%X [esp+8]=0x%X" % (a1, a2, a3), flush=True)
# after the three pushes, args are [esp]=dst? no: push n, push src, push dst → [esp]=dst, +4=src, +8=n
gdb.execute("quit")
