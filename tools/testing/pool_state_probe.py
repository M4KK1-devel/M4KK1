#!/usr/bin/env python3
"""Dump the cp_compositor state from guest memory at ~45 s uptime:
- pools[], buffers[], surfaces[] as the daemon sees them.
cp_comp lives in copland .bss (0x609000..0x60b7f8, size 6100 @0x60a024).
"""
import glob, json, socket, subprocess, sys, time, struct

ISO = (sorted(glob.glob("/mnt/f/M4KK1/output/m4kk1_*-full-test.iso")) or [""])[-1]
QMP = "/tmp/pool_qmp.sock"
SER = "/tmp/pool_serial.log"
DUMP = "/tmp/cp_comp.bin"
CP_COMP = 0x60a024

subprocess.run(["rm","-f",QMP,SER,DUMP],check=False)
proc = subprocess.Popen(["qemu-system-i386","-boot","d","-cdrom",ISO,"-m","512",
  "-display","none","-serial","file:"+SER,"-qmp","unix:%s,server=on,nowait"%QMP],
  stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
print("booting...",flush=True)
for _ in range(40):
    time.sleep(2)
    try:
        t=open(SER,"rb").read().decode(errors="replace")
        if "Entering main loop" in t: break
    except FileNotFoundError: pass
time.sleep(8)
s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); s.connect(QMP)
f=s.makefile("rw",encoding="utf-8",newline="\n")
json.loads(f.readline())
def q(o):
    f.write(json.dumps(o)+"\n");f.flush()
    while True:
        l=f.readline()
        if not l: break
        d=json.loads(l)
        if "return" in d or "error" in d: return d
q({"execute":"qmp_capabilities"})
q({"execute":"pmemsave","arguments":{"val":CP_COMP-0x24,"size":0x1800,"filename":DUMP}})
time.sleep(1); proc.kill()
data=open(DUMP,"rb").read()
print("dumped",len(data),"bytes")
# parse struct: hard. Instead print raw u32s of first pools region heuristically.
# struct layout unknown-precisely; print hints:
# pools: 8 * {obj_id,client_id,addr,size} -> scan for taskbar_buf addr 0x17252c0
for off in range(0,len(data)-4,4):
    v=struct.unpack_from("<I",data,off)[0]
    if v in (0x17252c0,0xe081b8,0x174aac0):
        print("found 0x%x at dump off 0x%x (abs 0x%x)"%(v,off,CP_COMP-0x24+off))
