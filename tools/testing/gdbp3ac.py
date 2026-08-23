import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break mkrn_fork_status")
gdb.execute("continue", to_string=False)   # fork#1
gdb.execute("continue", to_string=False)   # fork#2
gdb.execute("delete")
gdb.execute("break mkrn_memcpy")
n = 0
while n < 4:
    gdb.execute("continue", to_string=False)
    try:
        esp = int(gdb.parse_and_eval("$esp"))
        dst = int(gdb.parse_and_eval("*(unsigned int*)%d" % esp))
        src = int(gdb.parse_and_eval("*(unsigned int*)%d" % (esp+4)))
        cnt = int(gdb.parse_and_eval("*(unsigned int*)%d" % (esp+8)))
        print("CPY%d dst=0x%x src=0x%x cnt=0x%x(%d)" % (n, dst, src, cnt, cnt))
    except Exception as e:
        print("ERR %s" % e)
        break
    n += 1
gdb.execute("detach")
gdb.execute("quit")
