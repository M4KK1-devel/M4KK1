set pagination off
set confirm off
file /mnt/f/M4KK1/usr/bin/mdm
target remote :1234
break *0x8028c0
commands 1
silent
printf "gui_flip ENTERED\n"
continue
end
break *0x8016b5
commands 2
silent
printf "B1 form drawn: draw_login_form RETURNED\n"
continue
end
continue
