import socket
import subprocess
import time

MON = "/tmp/m4k_mon.sock"


def parse_ppm(data):
    parts = data.split(b"\n", 3)
    w, h = parts[1].split()
    w, h = int(w), int(h)
    off = len(parts[0]) + 1 + len(parts[1]) + 1 + len(parts[2]) + 1
    return w, h, data[off: off + w * h * 3]


def diff_regions(a, b):
    w = 800
    changed = []
    for y in range(0, 600, 2):
        for x in range(0, 800, 2):
            i = (y * w + x) * 3
            if (a[i], a[i + 1], a[i + 2]) != (b[i], b[i + 1], b[i + 2]):
                changed.append((x, y))
    return changed


def main():
    print("S0 start", flush=True)
    subprocess.run(["rm", "-f", MON], check=False)
    print("S1 rm", flush=True)
    proc = subprocess.Popen(
        [
            "qemu-system-i386",
            "-boot", "d",
            "-cdrom", "output/m4kk1_0.0.1_build1-alpha1.iso",
            "-m", "512",
            "-display", "none",
            "-serial", "file:/tmp/sprach_serial.log",
            "-monitor", "unix:%s,server=on,nowait" % MON,
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    time.sleep(14)
    print("S2 boot wait done", flush=True)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(2)
    for i in range(30):
        try:
            s.connect(MON)
            print("S3 connected", flush=True)
            break
        except OSError:
            time.sleep(0.5)
    else:
        print("S3 CONNECT FAILED", flush=True)
        return

    def cmd(line, wait=0.9):
        s.sendall(line.encode() + b"\n")
        time.sleep(wait)

    def shot():
        cmd("screendump /tmp/sp_cur.ppm", 0.8)
        with open("/tmp/sp_cur.ppm", "rb") as f:
            return parse_ppm(f.read())

    print("S4 first shot", flush=True)
    a = shot()
    print("S5 got first shot", flush=True)
    cmd("mouse_move 100 0", 1.5)
    print("S6 moved", flush=True)
    b = shot()
    print("S7 got second shot", flush=True)
    d1 = diff_regions(a[2], b[2])
    print("after +100,0: changed region around", d1[:30], "... total", len(d1), flush=True)
    cmd("mouse_move 0 100", 1.5)
    c = shot()
    d2 = diff_regions(b[2], c[2])
    print("after +0,100: changed region around", d2[:30], "... total", len(d2))
    proc.kill()


if __name__ == "__main__":
    main()
