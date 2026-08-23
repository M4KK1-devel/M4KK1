import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break proc_registry_remove")
gdb.execute("continue", to_string=False)
# first hit: inspect who is being removed and by whom
try:
    # static void proc_registry_remove(mkrn_process_t *p) — arg via stack (cdecl)
    esp = int(gdb.parse_and_eval("$esp"))
    arg = int(gdb.parse_and_eval("*(unsigned int*)%d" % (esp+4)))
    g = gdb.parse_and_eval("(mkrn_process_t*)%d" % arg)
    cur = gdb.parse_and_eval("(mkrn_process_t*)g_current_process")
    bt = gdb.execute("bt 6", to_string=True)
    print("REMOVE pid=%d ppid=%d tags=0x%x name=%s | cur=%d" % (
        int(g["pid"]), int(g["ppid"]), int(g["state_tags"]),
        g["name"].string(), int(cur["pid"])))
    print(bt)
except Exception as e:
    print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
