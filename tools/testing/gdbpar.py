import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/usr/bin/copland")
gdb.execute("target remote :1234")

gdb.execute("break *0x60054f")
gdb.execute("continue")
ebp = int(gdb.parse_and_eval("$ebp"))
esp = int(gdb.parse_and_eval("$esp"))
v_ebp = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % ebp))
v_ebp4 = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % (ebp + 4)))
print("PARENT at int80: ebp=0x%X esp=0x%X" % (ebp, esp), flush=True)
print("[ebp]=0x%X [ebp+4]=0x%X" % (v_ebp, v_ebp4), flush=True)
gdb.execute("x/8wx $ebp")
gdb.execute("quit")
