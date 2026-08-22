import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/usr/bin/copland")
gdb.execute("target remote :1234")

gdb.execute("break *0x60054f")
gdb.execute("continue")   # copland fork point

gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("break *0x10294c")   # just after mkrn_memcpy returns
gdb.execute("continue")

# eax at 102921 was ustack; re-read from saved slot 0x20(%esp)? easier: read child frame region
# child resume esp = ustack + 0xFF7C; dump that region now (after copy)
base = 0x3205C4  # will re-derive: read [esp+0x20]?? use known offset instead
# safer: scan the kernel stack for the memcpy args is overkill; probe both candidate slots:
for addr in (0x330540, ):
    for off in range(0, 0x40, 4):
        v = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % (addr + off)))
        print("0x%X: 0x%08X" % (addr + off, v), flush=True)
gdb.execute("quit")
