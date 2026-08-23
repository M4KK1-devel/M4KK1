set pagination off
set confirm off
file /mnt/f/M4KK1/output/m4kk1.krn
target remote :1234
# Let MDM run 30s, then interrupt and inspect
shell sleep 30
interrupt
x/wx 0x807104
x/8wx 0x807100
x/8wx 0x800000
info registers
quit
