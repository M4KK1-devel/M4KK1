import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# hit fork #2 (pid=2), then single-inspect what happens after return
gdb.execute("break mkrn_fork_status")
gdb.execute("continue", to_string=False)  # fork#1 MDM
gdb.execute("continue", to_string=False)  # fork#2 copland
pid = int(gdb.parse_and_eval("(unsigned int)current->pid"))
print("AT fork#2 cur_pid=%d" % pid)
# break on child enqueue and on child_restore
gdb.execute("break process.c:974")        # ready_enqueue(child)
gdb.execute("break mkrn_fork_child_restore")
import time
for i in range(3):
    gdb.execute("continue", to_string=False)
    try:
        eip = int(gdb.parse_and_eval("$eip"))
        pid2 = int(gdb.parse_and_eval("(unsigned int)current->pid"))
        print("HIT%d eip=0x%x cur_pid=%d" % (i, eip, pid2))
    except Exception as e:
        print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
