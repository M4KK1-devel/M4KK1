import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

try:
    bz = gdb.parse_and_eval("&buddy_zone")
    # free_head[4] offset: struct layout — heads after counters; use
    # the symbol directly
    gdb.execute("watch buddy_zone.free_head[4]")
    hits = 0
    while hits < 6:
        gdb.execute("continue", to_string=False)
        eip = int(gdb.parse_and_eval("$eip"))
        val = int(gdb.parse_and_eval("buddy_zone.free_head[4]"))
        cur = int(gdb.parse_and_eval(
            "(unsigned int)g_current_process->pid"))
        print("FH4 eip=0x%x val=0x%x cur_pid=%d" % (eip, val, cur))
        hits += 1
except Exception as e:
    print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
