import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/usr/bin/copland")
gdb.execute("target remote :1234")

gdb.execute("break *0x600551")     # fork return (child hits with eax=0)
gdb.execute("continue")
eax = int(gdb.parse_and_eval("$eax"))
ebp = int(gdb.parse_and_eval("$ebp"))
esp = int(gdb.parse_and_eval("$esp"))
print("FORK-RETURN: eax=0x%X ebp=0x%X esp=0x%X" % (eax, ebp, esp), flush=True)
if eax == 0:
    # child: inspect its saved return address slot
    v = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % (ebp + 4)))
    v0 = int(gdb.parse_and_eval("*(unsigned int*)0x%X" % ebp))
    print("child [ebp]=0x%X [ebp+4]=0x%X" % (v0, v), flush=True)
    # also dump around esp
    gdb.execute("x/16wx $esp")
gdb.execute("quit")
