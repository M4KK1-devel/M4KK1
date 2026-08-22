import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# Watch from the copland fork: break at user int80 0x60054f first,
# then follow the kernel fork to see which ustack the child gets.
gdb.execute("file /mnt/f/M4KK1/usr/bin/copland")
gdb.execute("break *0x60054f")
gdb.execute("continue")          # copland about to fork
print("COPLAND fork entered", flush=True)

gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("break *0x102921")   # ustack alloc return inside THIS fork
gdb.execute("continue")
eax = int(gdb.parse_and_eval("$eax"))
print("THIS fork ustack = 0x%X  (range ..0x%X)" % (eax, eax + 0x10000), flush=True)

# child resume esp will be ustack + 0xFFF7C if remap is right
expect = eax + 0xFFF7C
print("expected child resume esp = 0x%X" % expect, flush=True)
print("actual child esp observed earlier = 0x330540", flush=True)
gdb.execute("quit")
