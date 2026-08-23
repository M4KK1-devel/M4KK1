import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# Wait until sprach is stuck (serial shows banner done).
import time
gdb.execute("break *0x1101d8c")   # sprach_proto_connect (never hit yet)
gdb.execute("break mkrn_sched")   # any schedule
gdb.execute("continue")

# After a few seconds of running, interrupt and inspect sprach state.
time.sleep(8)
gdb.execute("interrupt", to_string=True)

# Find all threads' PCs: M4KK1 is single CPU; walk the process list via
# the scheduler.  Simpler: read the shm ready flag directly.
ready = int(gdb.parse_and_eval("*(unsigned char*)0x70000C"))
print("SHM_READY=%d" % ready)
magic = int(gdb.parse_and_eval("*(unsigned int*)0x700000"))
print("SHM_MAGIC=0x%x" % magic)

# Conn table state
t_magic = int(gdb.parse_and_eval("*(unsigned int*)0x720000"))
print("CONN_T_MAGIC=0x%x" % t_magic)
slot0 = int(gdb.parse_and_eval("*(unsigned int*)0x720010"))
print("SLOT0_inuse=%d" % slot0)
caddr = int(gdb.parse_and_eval("*(unsigned int*)0x720018"))
print("SLOT0_conn_addr=0x%x" % caddr)

gdb.execute("detach")
gdb.execute("quit")
