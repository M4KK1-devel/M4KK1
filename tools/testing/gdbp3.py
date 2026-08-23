import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# Break where copland has just become ready, then watch sprach's
# proto_connect run.  sprach loads at 0x1100000 (flat).
gdb.execute("break *0x1101d8c")   # sprach_proto_connect entry
gdb.execute("continue")           # first hit = sprach calling it

# single-step a bounded number of instructions, recording EIP range
inf = gdb.selected_inferior()
last_pc = 0
for i in range(400000):
    gdb.execute("stepi", to_string=True)
    pc = int(gdb.parse_and_eval("$pc"))
    if pc != last_pc:
        if i % 50000 == 0 or pc in (0x1100ebc, 0x1100dc4, 0x1100d4c):
            print("STEP pc=0x%x i=%d" % (pc, i))
        last_pc = pc
    # if we left the sprach text range into weird territory, report
    if pc < 0x1100000 or pc > 0x1110000:
        print("LEFT sprach text: pc=0x%x i=%d" % (pc, i))
        break

print("FINAL pc=0x%x" % int(gdb.parse_and_eval("$pc")))
gdb.execute("detach")
gdb.execute("quit")
