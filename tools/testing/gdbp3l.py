import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break *0x1037d4")
gdb.execute("continue", to_string=False)
print("=== BACKTRACE AT CONSOLE_SCROLL ===")
gdb.execute("bt")
pid = int(gdb.parse_and_eval("(unsigned int)current->pid"))
print("cur_pid=%d" % pid)
gdb.execute("detach")
gdb.execute("quit")
