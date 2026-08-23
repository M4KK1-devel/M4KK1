import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# fork wrapper in copland: musr_sc_fork -> sc0 -> int 0x80.
# Break at the instruction after 'int $0x80' in musr_sc0 of the
# COPland image (0x6xxxxx).  Find it: copland's musr_sc0.
# From nm: find symbol first.
out = gdb.execute("info symbol 0x60054f", to_string=True)
print("SYM0x60054f:", out.strip())
# break after the int80 return in the second fork (copland->sprach)
gdb.execute("break *0x600554")   # guess: post-int80 in sc0
gdb.execute("continue", to_string=False)
eax = int(gdb.parse_and_eval("$eax"))
print("POST_INT80 eax=0x%x (%d)" % (eax, eax))
gdb.execute("detach")
gdb.execute("quit")
