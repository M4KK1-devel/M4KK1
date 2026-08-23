import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break mkrn_fork_status")
gdb.execute("continue", to_string=False)   # fork#1
gdb.execute("continue", to_string=False)   # fork#2 entry
print("AT_FORK2")
gdb.execute("delete")
gdb.execute("break process.c:951")         # rebase loop body: v = *(uint32_t*)p
gdb.execute("break process.c:938")         # memcpy(ustack,...)
gdb.execute("break process.c:962")         # frame build
for i in range(10):
    gdb.execute("continue", to_string=False)
    try:
        eip = int(gdb.parse_and_eval("$eip"))
        line = gdb.execute("info line *$eip", to_string=True)
        ln = line.split("Line ")[1].split(" of")[0] if "Line " in line else "?"
        p = int(gdb.parse_and_eval("p")) if ln == "951" else -1
        v = int(gdb.parse_and_eval("v")) if ln == "951" else -1
        print("STEP%d line=%s p=0x%x v=0x%x" % (i, ln, p, v))
    except Exception as e:
        print("ERR %s" % e)
        break
gdb.execute("detach")
gdb.execute("quit")
