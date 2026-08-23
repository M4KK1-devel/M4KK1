import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# fork#2 walk: break at fork entry twice, then binary-search the hang
gdb.execute("break mkrn_fork_status")
gdb.execute("continue", to_string=False)   # fork#1 (MDM->copland)
gdb.execute("continue", to_string=False)   # fork#2 (copland->sprach)
pid = int(gdb.parse_and_eval("(unsigned int)current->pid"))
print("AT_FORK2 cur_pid=%d" % pid)

gdb.execute("delete")
# probes: ustack copy start / rebase loop / frame build / registry add
gdb.execute("break process.c:911")   # memcpy into ustack
gdb.execute("break process.c:935")   # frame build start
gdb.execute("break process.c:953")   # thread_esp assigned
gdb.execute("break process.c:984")   # registry add
for i in range(4):
    gdb.execute("continue", to_string=False)
    try:
        eip = int(gdb.parse_and_eval("$eip"))
        line = gdb.execute("info line *$eip", to_string=True).strip()
        print("PROBE%d 0x%x %s" % (i, eip, line))
    except Exception as e:
        print("ERR %s" % e)
        break
gdb.execute("detach")
gdb.execute("quit")
