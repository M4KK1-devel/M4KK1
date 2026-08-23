import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

try:
    cur = gdb.parse_and_eval("(mkrn_process_t*)g_current_process")
    print("CURRENT pid=%d name=%s" % (int(cur["pid"]),
          cur["name"].string()))
    print("  kernel_stack=0x%x user_stack_base=0x%x thread_esp=0x%x"
          % (int(cur["kernel_stack"]), int(cur["user_stack_base"]),
             int(cur["thread_esp"])))
    p = gdb.parse_and_eval("all_procs")
    n = 0
    while p and n < 8:
        print("PROC addr=0x%x pid=%d name=%s kstk=0x%x ustk=0x%x te=0x%x tags=0x%x"
              % (int(p), int(p["pid"]), p["name"].string(),
                 int(p["kernel_stack"]), int(p["user_stack_base"]),
                 int(p["thread_esp"]), int(p["state_tags"])))
        p = p["next"]
        n += 1
    print("EIP=0x%x ESP=0x%x" % (int(gdb.parse_and_eval("$eip")),
                                  int(gdb.parse_and_eval("$esp"))))
except Exception as e:
    print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
