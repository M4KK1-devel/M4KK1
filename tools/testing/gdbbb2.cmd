set pagination off
set confirm off
file /mnt/f/M4KK1/output/m4kk1.krn
target remote :1234
break *0x119b30
continue
printf "back_buffer ptr = 0x%x\n", *(unsigned int*)0x28b780
printf "expected range 0x280000..0x454c00\n"
printf "fb phys=%x w=%d h=%d\n", *(unsigned int*)0x28b784, *(unsigned int*)(0x28b784+4), *(unsigned int*)(0x28b784+8)
quit
