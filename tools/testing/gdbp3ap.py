import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# attach to a free-running (hung) system and inspect final state
try:
    for order in (4, 5):
        h = int(gdb.parse_and_eval("buddy_zone.free_head[%d]" % order))
        c = int(gdb.parse_and_eval("buddy_zone.free_count[%d]" % order))
        print("O%d head=0x%x count=%d" % (order, h, c))
    # order_map for the contested pfns
    base = int(gdb.parse_and_eval("buddy_zone.base_pfn"))
    for pfn in (0x1fde0, 0x1fdf0, 0x1fdd5, 0x1fe00):
        om = int(gdb.parse_and_eval(
            "buddy_zone.order_map[%d]" % (pfn - base)))
        print("OM pfn=0x%x -> 0x%02x" % (pfn, om))
except Exception as e:
    print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
