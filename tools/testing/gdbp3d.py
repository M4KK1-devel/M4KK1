import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# Hardware breakpoint inside sprach's shm->ready wait loop.
gdb.execute("hbreak *0x1106e60")   # cmpl $0, 0x8(%eax)  (ready check)
hits = 0
while hits < 3:
    gdb.execute("continue", to_string=False)
    eax = int(gdb.parse_and_eval("$eax"))
    ready = int(gdb.parse_and_eval("*(unsigned char*)(%d + 8)" % eax)) if eax else -1
    print("HIT%d eax(shm)=0x%x ready=%d" % (hits, eax, ready))
    hits += 1
    if ready == 1:
        break

# also dump what copland's init actually wrote:
m = int(gdb.parse_and_eval("*(unsigned int*)0x700000"))
r = int(gdb.parse_and_eval("*(unsigned char*)0x70000C"))
print("AT0x700000: magic=0x%x ready@C=%d" % (m, r))
gdb.execute("detach")
gdb.execute("quit")
