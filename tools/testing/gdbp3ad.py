import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break mkrn_fork_status")
gdb.execute("continue", to_string=False)   # fork#1
gdb.execute("continue", to_string=False)   # fork#2
gdb.execute("delete")
gdb.execute("break process.c:938")
for i in range(2):
    gdb.execute("continue", to_string=False)
    try:
        ustack = int(gdb.parse_and_eval("(unsigned int)ustack"))
        base = int(gdb.parse_and_eval("(unsigned int)parent->user_stack_base"))
        uesp = int(gdb.parse_and_eval("(unsigned int)uesp"))
        uebp = int(gdb.parse_and_eval("(unsigned int)uebp"))
        upages = int(gdb.parse_and_eval("(unsigned int)upages"))
        print("FORKCPY%d ustack=0x%x base=0x%x upages=0x%x uesp=0x%x uebp=0x%x" % (
            i, ustack, base, upages, uesp, uebp))
    except Exception as e:
        print("ERR %s" % e)
        break
gdb.execute("detach")
gdb.execute("quit")
