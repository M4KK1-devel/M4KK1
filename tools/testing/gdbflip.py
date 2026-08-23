import gdb, time

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/usr/bin/mdm")
gdb.execute("target remote :1234")
gdb.execute("break *0x8028c0")
gdb.execute("continue")
gdb.execute("delete 1")
print("=== running free 25s ===", flush=True)
gdb.post_event(lambda: None)
# async continue via CLI in background thread-safe way:
gdb.execute("continue&")   # may not exist; fallback below
