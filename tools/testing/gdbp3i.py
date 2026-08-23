import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# After copland(pid2) forks sprach(pid3), does pid2 ever run again?
# Break at scheduler pick; filter for pid2 picks after we see pid3 exist.
gdb.execute("break mkrn_process_switch_pick")
saw_fork2 = False
p2_picks = 0
n = 0
while n < 3000:
    gdb.execute("continue", to_string=False)
    n += 1
    try:
        pid = int(gdb.parse_and_eval("(unsigned int)current->pid"))
    except Exception:
        continue
    if pid == 2:
        # check queue state
        cnt = int(gdb.parse_and_eval("(unsigned int)ready_queue_count"))
        print("PICK_IN pid2 n=%d rq_count=%d" % (n, cnt))
        p2_picks += 1
        if p2_picks > 3:
            break
gdb.execute("detach")
gdb.execute("quit")
