import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break mkrn_fork_status")
gdb.execute("continue", to_string=False)   # fork#1
gdb.execute("continue", to_string=False)   # fork#2 entry — read parent state NOW
try:
    cur = gdb.parse_and_eval("(mkrn_process_t*)g_current_process")
    print("F2ENTRY cur_pid=%d name=%s kstk=0x%x ustk=0x%x tesp=0x%x" % (
        int(cur["pid"]), cur["name"].string(),
        int(cur["kernel_stack"]), int(cur["user_stack_base"]),
        int(cur["thread_esp"])))
except Exception as e:
    print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
