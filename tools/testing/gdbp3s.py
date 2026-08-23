import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break m4k_syscall_spawn_impl")
gdb.execute("continue", to_string=False)   # spawn#1 (MDM->copland)
gdb.execute("continue", to_string=False)   # spawn#2 (copland child -> sprach)
print("AT spawn#2")
# now step over the major stages: break on vfs_read, execve entry
gdb.execute("break mkrn_execve")
gdb.execute("break mkrn_vfs_read")
for i in range(8):
    gdb.execute("continue", to_string=False)
    try:
        eip = int(gdb.parse_and_eval("$eip"))
        pid = int(gdb.parse_and_eval("(unsigned int)current->pid"))
        name = gdb.execute("x/2i $eip", to_string=True).strip().splitlines()[0]
        print("STAGE%d pid=%d %s" % (i, pid, name))
    except Exception as e:
        print("ERR %s" % e)
        break
gdb.execute("detach")
gdb.execute("quit")
