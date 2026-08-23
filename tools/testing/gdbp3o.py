import gdb, time

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("file /mnt/f/M4KK1/output/m4kk1.krn")
gdb.execute("target remote :1234")

# Run free until the guest takes the #GP (delivered to gdb as SIGSEGV/SIGTRAP)
gdb.execute("handle SIGSEGV stop nopass")
gdb.execute("handle SIGBUS stop nopass")
for i in range(40):
    try:
        out = gdb.execute("continue", to_string=True)
    except gdb.error as e:
        out = str(e)
    if "SIGSEGV" in out or "SIGBUS" in out or "signal" in out.lower():
        print("=== CAUGHT FAULT (iter %d) ===" % i)
        print(out.strip()[-300:])
        gdb.execute("info registers eip esp ebp eax")
        try:
            gdb.execute("x/8i $eip-16")
        except Exception:
            pass
        gdb.execute("x/16wx $esp")
        try:
            print("current->pid=%d" % int(gdb.parse_and_eval("(unsigned int)current->pid")))
        except Exception as e:
            print("pid read err %s" % e)
        break
gdb.execute("detach")
gdb.execute("quit")
