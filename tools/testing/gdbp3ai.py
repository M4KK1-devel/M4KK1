import gdb

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# copland _start=0x601a8c region; spawn_wm ser_puts return site unknown.
# Instead: hardware watch the serial output? Simpler: break on kernel
# mkrn_console_write_dec (used by print_u32 in "Sprach WM started")
gdb.execute("break mkrn_console_write_dec")
hits = 0
while hits < 8:
    gdb.execute("continue", to_string=False)
    try:
        cur = gdb.parse_and_eval("(mkrn_process_t*)g_current_process")
        print("WDEC cur_pid=%d name=%s" % (int(cur["pid"]), cur["name"].string()))
    except Exception as e:
        print("ERR %s" % e)
        break
    hits += 1
gdb.execute("detach")
gdb.execute("quit")
