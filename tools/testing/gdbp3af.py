import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break mkrn_fork_status")
gdb.execute("continue", to_string=False)   # fork#1
gdb.execute("continue", to_string=False)   # fork#2
gdb.execute("delete")
gdb.execute("break process.c:936")         # alloc_page call line
gdb.execute("continue", to_string=False)
gdb.execute("finish", to_string=True)
try:
    cur = gdb.parse_and_eval("(mkrn_process_t*)g_current_process")
    ck = int(gdb.parse_and_eval("(unsigned int)child->kernel_stack"))
    print("CHK parent(cur)=%d pk=0x%x ck=0x%x" % (
        int(cur["pid"]),
        int(cur["kernel_stack"]), ck))
except Exception as e:
    print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
