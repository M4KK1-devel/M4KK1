import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/usr/bin/copland")
gdb.execute("target remote :1234")

# break at fork int80 entry, then read kernel current->user_stack_base
gdb.execute("break *0x60054f")
gdb.execute("continue")

gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
cur = int(gdb.parse_and_eval("(unsigned int)current"))
print("current PCB = 0x%X" % cur, flush=True)
# find user_stack_base offset in PCB via gdb type info
try:
    v = int(gdb.parse_and_eval("((mkrn_process_t*)0x%X)->user_stack_base" % cur))
    print("current->user_stack_base = 0x%X" % v, flush=True)
except gdb.error as e:
    print("type err:", e, flush=True)
gdb.execute("quit")
