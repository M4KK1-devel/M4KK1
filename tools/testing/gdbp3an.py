import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

try:
    # buddy_zone symbol
    bz = gdb.parse_and_eval("buddy_zone")
    base = int(bz["base_pfn"])
    nr = int(bz["nr_pages"])
    print("ZONE base_pfn=0x%x nr=%d (0x%x..0x%x)"
          % (base, nr, base << 12, (base + nr) << 12))
    for order in range(11):
        cnt = int(bz["free_count"][order])
        if cnt:
            print("  order %2d (%3d pages): count=%d head_pfn=0x%x"
                  % (order, 1 << order, cnt,
                     int(bz["free_head"][order])))
    # walk order-4 lists (16-page blocks) and check for the live pages
    for order in (4, 5):
        head = int(bz["free_head"][order])
        if head == 0xFFFFFFFF:
            continue
        print("ORDER %d list walk:" % order)
        p = head
        n = 0
        seen = set()
        while p != 0xFFFFFFFF and n < 40:
            node = gdb.parse_and_eval(
                "(struct mkrn_free_page *)%d" % (p << 12))
            nxt = int(node["next"])
            print("  pfn=0x%x addr=0x%x next=0x%x"
                  % (p, p << 12, nxt))
            if p in seen:
                print("  DUP pfn 0x%x" % p)
                break
            seen.add(p)
            p = nxt
            n += 1
except Exception as e:
    print("ERR %s" % e)
gdb.execute("detach")
gdb.execute("quit")
