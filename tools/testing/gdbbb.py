import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# static back_buffer is at some fixed address; find via symbol table
sym = gdb.lookup_symbol("back_buffer")[0]
print("sym back_buffer:", sym)
if sym:
    addr = int(sym.value().cast(gdb.lookup_type("unsigned int")))
    print("back_buffer addr=0x%X" % addr)
else:
    # nm fallback baked in: find via maintenance info
    addr = None

gdb.execute("break *0x119b30")
gdb.execute("continue")   # stop at fill_gradient entry

if addr:
    bb = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % addr))
    bbsym = gdb.lookup_symbol("back_buffer_size")[0]
    print("READ back_buffer ptr = 0x%X" % bb)
    if bbsym:
        bs = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % int(bbsym.value().cast(gdb.lookup_type("unsigned int")))))
        print("back_buffer_size = 0x%X" % bs)
        print("buffer range: 0x%X .. 0x%X" % (bb, bb + bs))
        print("overlaps user ELF window (0x600000+):", bb + bs > 0x600000)

gdb.execute("info registers eax ebx ecx edx esi edi esp")
gdb.execute("quit")
