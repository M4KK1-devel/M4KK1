import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# system already hung; inspect scheduler state
try:
    cur = gdb.parse_and_eval("(mkrn_process_t*)g_current_process")
    print("CURRENT pid=%d tags=0x%x" % (int(cur["pid"]), int(cur["state_tags"])))
    eip = int(gdb.parse_and_eval("$eip"))
    esp = int(gdb.parse_and_eval("$esp"))
    print("EIP=0x%x ESP=0x%x" % (eip, esp))
    # walk all_procs with cycle guard
    head = gdb.parse_and_eval("(mkrn_process_t*)0")
    # all_procs is static; read via symbol
    try:
        head = gdb.parse_and_eval("all_procs")
    except Exception as e2:
        print("NOPRIV all_procs: %s" % e2)
    seen = []
    p = head
    n = 0
    while p and n < 16:
        pidv = int(p["pid"])
        ppidv = int(p["ppid"])
        tags = int(p["state_tags"])
        addr = int(p)
        print("PROC[%d] addr=0x%x pid=%d ppid=%d tags=0x%x next=0x%x"
              % (n, addr, pidv, ppidv, tags, int(p["next"])))
        if addr in seen:
            print("CYCLE at 0x%x" % addr)
            break
        seen.append(addr)
        p = p["next"]
        n += 1
except Exception as e:
    print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
