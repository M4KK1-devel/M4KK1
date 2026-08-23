import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break *0x600554")
for i in range(4):
    gdb.execute("continue", to_string=False)
    eax = int(gdb.parse_and_eval("$eax"))
    pid = int(gdb.parse_and_eval("(unsigned int)current->pid"))
    ppid = int(gdb.parse_and_eval("(unsigned int)current->ppid"))
    eip = int(gdb.parse_and_eval("$pc"))
    print("HIT%d cur_pid=%d ppid=%d eax=0x%x pc=0x%x"
          % (i, pid, ppid, eax, eip))
gdb.execute("detach")
gdb.execute("quit")
