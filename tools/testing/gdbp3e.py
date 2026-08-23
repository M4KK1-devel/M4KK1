import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# Break on copland's post-fork parent path: find the instruction
# after the musr_sc_fork call in copland_spawn_wm.  First get the
# address from the binary at runtime — break on m4k_waitpid's
# caller is complex; instead break copland's serial puts of
# "Sprach WM started".
# Simpler: break on kernel mkrn_fork_status return to parent.
gdb.execute("break mkrn_fork_status")
for i in range(3):
    gdb.execute("continue", to_string=False)
    pid = int(gdb.parse_and_eval("$eax")) if False else None
    print("FORK_HIT%d pc=0x%x" % (i, int(gdb.parse_and_eval("$pc"))))
gdb.execute("detach")
gdb.execute("quit")
