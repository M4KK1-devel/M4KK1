import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break m4k_syscall_spawn_impl")
hits = 0
while hits < 5:
    gdb.execute("continue", to_string=False)
    try:
        pid = int(gdb.parse_and_eval("(unsigned int)current->pid"))
        eip = int(gdb.parse_and_eval("$eip"))
        print("SPAWN_IN cur_pid=%d" % pid)
    except Exception as e:
        print("ERR %s" % e)
        break
    hits += 1
gdb.execute("detach")
gdb.execute("quit")
