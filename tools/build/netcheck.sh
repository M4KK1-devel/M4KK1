#!/bin/bash
# Quick syntax/semantic check of the net stack files (host gcc, i386).
cd /mnt/f/M4KK1 || exit 1
CF="-Wall -Wextra -O2 -g -ffreestanding -nostdlib -nostdinc -m32 -mno-sse -mno-sse2 -mno-mmx -std=gnu99 -fno-stack-protector -fno-pic -fno-pie -Isys/src/include -Iinclude -Isys/src/arch/m4kk1/include -Isys/src/kernel/drivers -Isys/src/net"
rc=0
for f in e1000 net arp tcp; do
    echo "--- $f.c"
    gcc $CF -c "sys/src/net/$f.c" -o "/tmp/$f.o" || rc=1
done
echo "NETCHECK_RC=$rc"
exit $rc
