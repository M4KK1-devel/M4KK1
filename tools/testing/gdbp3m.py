import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# Let it reach the hang first: run 15s free, then break on console_write.
import time
gdb.execute("continue&", to_string=True)
time.sleep(15)
gdb.execute("interrupt", to_string=True)
gdb.execute("break mkrn_console_write")
for i in range(3):
    gdb.execute("continue", to_string=False)
    try:
        s = gdb.execute("x/s *(char **)$esp+4", to_string=True)
    except Exception:
        try:
            arg = int(gdb.parse_and_eval("(unsigned int)msg"))
            s = gdb.execute("x/s 0x%x" % arg, to_string=True)
        except Exception as e:
            s = "ERR %s" % e
    print("CW%d: %s" % (i, s))
    # also try reading via frame
try:
    gdb.execute("info registers eip esp ebp")
except Exception:
    pass
gdb.execute("detach")
gdb.execute("quit")
