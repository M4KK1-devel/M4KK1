set pagination off
set confirm off
file /mnt/f/M4KK1/output/m4kk1.krn
target remote :1234
break *0x117c00
commands
silent
set $dst = *(unsigned int*)($esp+4)
if $dst >= 0x700000 && $dst < 0x1300000
printf "HIT dst=%x len=%x caller=%x\n", $dst, *(unsigned int*)($esp+12), *(unsigned int*)($esp)
end
continue
end
continue
