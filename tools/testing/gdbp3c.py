import time

import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("continue&", to_string=True)
time.sleep(12)
# remote-serial target: force stop by sending \x03 through the gdb
# event loop is unavailable in batch; instead kill -INT the gdb
# externally.  Fallback: use QMP-style "monitor system_reset"? no.
# Portable approach: osascript no.  Use gdb.events wait loop with
# KeyboardInterrupt via signal.
import signal


def handler(sig, frame):
    raise KeyboardInterrupt


signal.signal(signal.SIGINT, handler)
try:
    # busy-wait allows the SIGINT to interrupt
    t0 = time.time()
    while time.time() - t0 < 0.5:
        pass
except KeyboardInterrupt:
    pass
print("STOPPED pc=0x%x" % int(gdb.parse_and_eval("$pc")))
