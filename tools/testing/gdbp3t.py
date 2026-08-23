import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break m4k_syscall_spawn_impl")
gdb.execute("continue", to_string=False)   # spawn#1
gdb.execute("continue", to_string=False)   # spawn#2 at work
gdb.execute("break mkrn_execve")
gdb.execute("continue", to_string=False)   # STAGE2: inside execve
print("INSIDE execve, walking to return")
# step until we leave execve or hit the info print
gdb.execute("break execve.c:214")   # "init process ready" print site
try:
    gdb.execute("continue", to_string=False)
    eip = int(gdb.parse_and_eval("$eip"))
    pid = int(gdb.parse_and_eval("(unsigned int)current->pid"))
    print("HIT_READY_PRINT pid=%d eip=0x%x" % (pid, eip))
except Exception as e:
    print("NO_HIT %s" % e)
# now the handoff: break on scheduler pick to see who runs next
gdb.execute("break mkrn_process_switch_pick")
gdb.execute("continue", to_string=False)
pid = int(gdb.parse_and_eval("(unsigned int)current->pid"))
eip = int(gdb.parse_and_eval("$eip"))
print("PICK_AFTER cur_pid=%d eip=0x%x" % (pid, eip))
gdb.execute("detach")
gdb.execute("quit")
