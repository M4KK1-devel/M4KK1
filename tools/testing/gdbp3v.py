import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break proc_registry_add")
hits = 0
names = []
while hits < 6:
    gdb.execute("continue", to_string=False)
    try:
        g = gdb.parse_and_eval("(mkrn_process_t*)$eax") if False else None
        # first arg on stack for -m32 cdecl... check debug: use 'p'
        name = gdb.execute("info args", to_string=True)
        pid = int(gdb.parse_and_eval("(unsigned int)current->pid"))
        print("REG_ADD hit cur_pid=%d" % pid)
    except Exception as e:
        print("ERR %s" % e)
        break
    hits += 1
gdb.execute("detach")
gdb.execute("quit")
