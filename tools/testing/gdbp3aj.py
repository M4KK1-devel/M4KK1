import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# copland user-space fork return point: int 0x80 at 0x60054f, next insn 0x600551
gdb.execute("break *0x600551")
gdb.execute("continue", to_string=False)
try:
    eax = int(gdb.parse_and_eval("$eax"))
    esp = int(gdb.parse_and_eval("$esp"))
    eip = int(gdb.parse_and_eval("$eip"))
    print("FORKRET eip=0x%x eax=%d(0x%x) esp=0x%x" % (eip, eax, eax, esp))
except Exception as e:
    print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
