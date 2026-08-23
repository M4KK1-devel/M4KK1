import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# free-run attach: count recent forks by watching pid counter + who forks
# simpler: break on mkrn_fork_status return path — too slow. Instead:
# attach AFTER hang, inspect the garbage-EIP process's kernel stack and
# the conn table / cptest state
try:
    cur = gdb.parse_and_eval("(mkrn_process_t*)g_current_process")
    print("CURRENT pid=%d ppid=%d tags=0x%x thread_esp=0x%x"
          % (int(cur["pid"]), int(cur["ppid"]), int(cur["state_tags"]),
             int(cur["thread_esp"])))
    # dump words around thread_esp (the frame the asm would jump to)
    te = int(cur["thread_esp"])
    for off in range(0, 24, 4):
        v = int(gdb.parse_and_eval("*(unsigned int*)(%d)" % (te + off)))
        print("  [te+0x%02x] = 0x%08x" % (off, v))
    print("EIP=0x%x ESP=0x%x EAX=0x%x" % (
        int(gdb.parse_and_eval("$eip")), int(gdb.parse_and_eval("$esp")),
        int(gdb.parse_and_eval("$eax"))))
except Exception as e:
    print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
