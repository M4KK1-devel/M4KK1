import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break mkrn_fork_status")
hits = 0
while hits < 4:
    gdb.execute("continue", to_string=False)
    pid = int(gdb.parse_and_eval("(unsigned int)current->pid"))
    print("FORK_IN cur_pid=%d" % pid)
    hits += 1
    try:
        gdb.execute("finish", to_string=False)
        eax = int(gdb.parse_and_eval("$eax"))
        pid2 = int(gdb.parse_and_eval("(unsigned int)current->pid"))
        print("FORK_OUT cur_pid=%d ret=%d" % (pid2, eax))
    except Exception as e:
        print("FINISH_ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
