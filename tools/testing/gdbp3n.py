import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break process.c:924")
gdb.execute("continue", to_string=False)
nb = int(gdb.parse_and_eval("(unsigned int)new_base"))
ob = int(gdb.parse_and_eval("(unsigned int)orig_base"))
ue = int(gdb.parse_and_eval("(unsigned int)uesp"))
p = int(gdb.parse_and_eval("(unsigned int)p"))
print("orig_base=0x%x new_base=0x%x uesp=0x%x p=0x%x" % (ob, nb, ue, p))
gdb.execute("detach")
gdb.execute("quit")
