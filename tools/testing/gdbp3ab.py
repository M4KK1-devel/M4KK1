import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

gdb.execute("break mkrn_fork_status")
gdb.execute("continue", to_string=False)   # fork#1
gdb.execute("continue", to_string=False)   # fork#2
gdb.execute("delete")
gdb.execute("break process.c:938")         # memcpy call site
gdb.execute("continue", to_string=False)

# stepi through memcpy + rebase loop, sample periodically
import collections
hist = collections.Counter()
for i in range(30000):
    gdb.execute("stepi", to_string=True)
    eip = int(gdb.parse_and_eval("$eip"))
    hist[eip] += 1
    if i % 5000 == 0:
        sym = gdb.execute("info symbol 0x%x" % eip, to_string=True).strip()
        print("AT%05d eip=0x%x %s" % (i, eip, sym))
# final: where are we stuck?
eip = int(gdb.parse_and_eval("$eip"))
sym = gdb.execute("info symbol 0x%x" % eip, to_string=True).strip()
print("FINAL eip=0x%x %s" % (eip, sym))
top = hist.most_common(5)
for a, c in top:
    s = gdb.execute("info symbol 0x%x" % a, to_string=True).strip()
    print("HOT 0x%x x%d %s" % (a, c, s))
gdb.execute("detach")
gdb.execute("quit")
