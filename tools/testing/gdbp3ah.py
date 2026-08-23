import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# sprach user-space: base 0x1100000; ser_puts=0x90c, proto_connect=0x1d8c,
# sp_wait_evt=0xebc, cp_client_connect call site inside proto_connect
gdb.execute("break *0x1101d8c")           # sprach_proto_connect
gdb.execute("continue", to_string=False)
try:
    esp = int(gdb.parse_and_eval("$esp"))
    eip = int(gdb.parse_and_eval("$eip"))
    print("HIT_PROTO_CONNECT eip=0x%x esp=0x%x" % (eip, esp))
except Exception as e:
    print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
