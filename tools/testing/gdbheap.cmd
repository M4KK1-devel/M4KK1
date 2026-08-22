set pagination off
set confirm off
file /mnt/f/M4KK1/output/m4kk1.krn
target remote :1234
break *0x119b30
continue
printf "u32KernelHeapStart=0x%x\n", *(unsigned int*)0x24e070
printf "u32KernelHeapEnd=0x%x\n", *(unsigned int*)0x24e06c
printf "u32UsedMemory=0x%x\n", *(unsigned int*)0x24e074
printf "u32FreeMemory=0x%x\n", *(unsigned int*)0x24e078
quit
