import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# break on the fork#2 child trampoline restore point and report pid
gdb.execute("break *(0x101480)")
gdb.execute("continue", to_string=False)
try:
    cur = gdb.parse_and_eval("(mkrn_process_t*)g_current_process")
    print("RESTORE0 cur_pid=%d name=%s" % (int(cur["pid"]), cur["name"].string()))
except Exception as e:
    print("ERR0 %s" % e)
gdb.execute("delete")
# now watch PCB list after restore
gdb.execute("break mkrn_process_switch_pick")
for i in range(3):
    gdb.execute("continue", to_string=False)
    try:
        g0 = gdb.parse_and_eval("(mkrn_process_t*)all_procs")
        p = int(g0)
        names = []
        j = 0
        while p and j < 6:
            g = gdb.parse_and_eval("(mkrn_process_t*)%d" % p)
            names.append("%d:%s(t=0x%x)" % (int(g["pid"]), g["name"].string(),
                                            int(g["state_tags"])))
            p = int(g["next"])
            j += 1
        cur = gdb.parse_and_eval("(mkrn_process_t*)g_current_process")
        print("PICK%d cur=%d procs=[%s]" % (i, int(cur["pid"]), ",".join(names)))
    except Exception as e:
        print("ERR %s" % e)
        break
gdb.execute("detach")
gdb.execute("quit")
