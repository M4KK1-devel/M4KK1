set pagination off
set confirm off
file /mnt/f/M4KK1/usr/bin/copland
target remote :1234
break *0x601b5d
commands 1
silent
printf "WM_CALLSITE\n"
continue
end
break *0x6019e4
commands 2
silent
printf "SPAWN_WM_ENTRY\n"
continue
end
continue
