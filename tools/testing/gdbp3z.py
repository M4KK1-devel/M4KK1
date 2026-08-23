import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break mkrn_fork_status")
gdb.execute("continue", to_string=False)   # fork#1
gdb.execute("continue", to_string=False)   # fork#2
print("AT_FORK2")
gdb.execute("delete")
gdb.execute("break mkrn_memory_alloc_page")
gdb.execute("break buddy_alloc")
n = 0
while n < 6:
    gdb.execute("continue", to_string=False)
    try:
        eip = int(gdb.parse_and_eval("$eip"))
        sym = gdb.execute("info symbol $eip", to_string=True).strip()
        esp = int(gdb.parse_and_eval("$esp"))
        arg = int(gdb.parse_and_eval("*(unsigned int*)%d" % (esp+4)))
        print("ALLOC%d %s arg=0x%x(%d)" % (n, sym, arg, arg))
    except Exception as e:
        print("ERR %s" % e)
        break
    n += 1
gdb.execute("detach")
gdb.execute("quit")
