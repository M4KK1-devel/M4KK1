import gdb, time

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# Fixed timer breakpoint instead of async interrupt: break on timer tick
gdb.execute("break mkrn_process_switch_pick")
gdb.execute("continue", to_string=False)
gdb.execute("delete")
print("=== PCB SNAPSHOT (timer stop) ===")
try:
    g0 = gdb.parse_and_eval("(mkrn_process_t*)all_procs")
    p = int(g0)
    i = 0
    while p and i < 8:
        g = gdb.parse_and_eval("(mkrn_process_t*)%d" % p)
        print("PROC pid=%d ppid=%d tags=0x%x esp=0x%x kstk=0x%x ustk=0x%x name=%s" % (
            int(g["pid"]), int(g["ppid"]), int(g["state_tags"]),
            int(g["thread_esp"]), int(g["kernel_stack"]),
            int(g["user_stack_base"]),
            g["name"].string()))
        p = int(g["next"])
        i += 1
    cur = gdb.parse_and_eval("(mkrn_process_t*)g_current_process")
    print("CURRENT pid=%d name=%s" % (int(cur["pid"]), cur["name"].string()))
except Exception as e:
    print("SNAP_ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
