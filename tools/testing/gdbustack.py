import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# 3rd mkrn_alloc in fork = ustack (10291c); check return at 102921
gdb.execute("break *0x102921")
gdb.execute("continue")
eax = int(gdb.parse_and_eval("$eax"))
print("fork ustack alloc returned: 0x%X" % eax, flush=True)
if eax == 0:
    print(">>> CONFIRMED: ustack alloc FAILED — child gets no stack copy <<<", flush=True)
gdb.execute("quit")
