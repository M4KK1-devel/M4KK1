#!/bin/bash
cd /mnt/f/M4KK1
CFLAGS="-ffreestanding -nostdlib -nostdinc -m32 -mno-sse -std=gnu99 -fno-stack-protector -fno-pic -fno-pie -Isys/src/include -Im4sh -Iusr/src/cmd -Iusr/src/lib/m4k_libc/include"
fail=0
for m in main draw buf files syntax keys; do
    echo "== $m.c"
    gcc $CFLAGS -c usr/src/cmd/altr/$m.c -o /tmp/altr_$m.o || fail=1
done
echo "FAIL=$fail"
