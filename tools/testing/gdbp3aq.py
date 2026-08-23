import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# watch the free-list node at 0x1FDE0000 (pfn 0x1fde0):
# prev at +0, next at +4. Catch WHO writes {0,0}.
gdb.execute("watch *(unsigned int*)0x1FDE0000")
hits = 0
while hits < 10:
    gdb.execute("continue", to_string=False)
    try:
        eip = int(gdb.parse_and_eval("$eip"))
        v = int(gdb.parse_and_eval("*(unsigned int*)0x1FDE0000"))
        print("W eip=0x%x val=0x%x" % (eip, v))
    except Exception as e:
        print("ERR %s" % e)
        break
    hits += 1
gdb.execute("detach")
gdb.execute("quit")
