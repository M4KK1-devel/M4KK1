import struct, sys
# parse pools[] and buffers[] from a cp_compositor dump.
# struct cp_compositor (verify offsets from the ELF symbol sizes):
#   clients[8]: struct cp_client {struct cp_conn *conn; struct cp_objmap map(388+...); uint32; uint8}
# Use the known addresses: pools addr 0x60b2cc-? from previous probe found
# pool addr VALUES at abs 0x60b2cc (cptest) and 0x60b2dc (sprach).
# cp_pool_state = {obj_id, client_id, addr(uintptr 4 bytes), size} = 16 bytes
# If pools[] starts at X, entries: X+0 cptest{.. addr@X+8}, so X = 0x60b2cc-8 = 0x60b2c4
# sprach entry would start at 0x60b2d4: obj_id@0x60b2d4, client@+4, addr@0x60b2dc ✓
POOLS = 0x60b2c4
# buffers follow after 8 pools * 16 = 0x80 => 0x60b344
BUFFERS = POOLS + 8*16
data = open("/tmp/cp_comp.bin","rb").read()
BASE = 0x60a024 - 0x24   # dump start = 0x60a000
def off(absaddr): return absaddr - BASE
print("== pools ==")
for i in range(8):
    o = off(POOLS + i*16)
    obj, cl, addr, size = struct.unpack_from("<IIII", data, o)
    if obj: print(f"pool[{i}] obj={obj} client={cl} addr=0x{addr:x} size=0x{size:x}")
print("== buffers ==")
for i in range(16):
    o = off(BUFFERS + i*28)
    obj, cl, pool, o2, w, h, stride = struct.unpack_from("<IIIIiii", data, o)
    if obj: print(f"buf[{i}] obj={obj} client={cl} pool={pool} off={o2} {w}x{h} stride={stride}")
