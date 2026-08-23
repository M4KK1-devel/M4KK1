import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# break at the ebp-rebase loop body (process.c ~924) — need line addr.
gdb.execute("break process.c:924")
hits = 0
last_p = -1
same = 0
while hits < 40:
    gdb.execute("continue", to_string=False)
    try:
        p = int(gdb.parse_and_eval("(unsigned int)p"))
        v = int(gdb.parse_and_eval("(unsigned int)*(unsigned int *)p"))
    except Exception as e:
        print("ERR %s" % e)
        break
    print("REBASE p=0x%x -> v=0x%x (new_base+0x%x)" % (p, v, 0))
    hits += 1
    if p == last_p:
        same += 1
        if same > 3:
            print("LOOPING at p=0x%x" % p)
            break
    else:
        same = 0
    last_p = p
gdb.execute("detach")
gdb.execute("quit")
