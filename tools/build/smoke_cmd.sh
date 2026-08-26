#!/bin/bash
# Boot + inject ifconfig/ping via serial unix socket, capture output.
cd /mnt/f/M4KK1 || exit 1
ISO=$(ls -t output/m4kk1_*full-test.iso | head -1)
SOCK=/tmp/m4k_net.sock
OUT=logs/smoke_cmd_$(date +%H%M%S).log
rm -f "$SOCK"
qemu-system-i386 -cdrom "$ISO" -m 512 -vga std \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  -serial unix:"$SOCK",server=on,wait=off \
  -display none -no-reboot &
QPID=$!
# reader: dump socket -> log
socat UNIX-CONNECT:"$SOCK" - > "$OUT" 2>/dev/null &
RPID=$!
sleep 30
printf 'ifconfig\r' | socat - UNIX-CONNECT:"$SOCK" >/dev/null 2>&1
sleep 5
printf 'ping 10.0.2.2\r' | socat - UNIX-CONNECT:"$SOCK" >/dev/null 2>&1
sleep 12
printf 'help\r' | socat - UNIX-CONNECT:"$SOCK" >/dev/null 2>&1
sleep 5
kill $QPID $RPID 2>/dev/null
echo "--- output ---"
sed -n '1,20p' "$OUT"
echo "..."
grep -n 'e1000\|Network up\|ifconfig\|PING\|reply\|eth0' "$OUT" | head -20
