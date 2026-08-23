set pagination off
set confirm off
file /mnt/f/M4KK1/usr/bin/copland
target remote :1234
break *0x601b66
commands 1
silent
printf "SPAWN_WM entry\n"
continue
end
break *0x601b71
commands 2
silent
printf "SPAWN_CPTEST entry\n"
continue
end
break *0x6019f0
commands 3
silent
printf "spawn_wm func entry\n"
continue
end
continue
