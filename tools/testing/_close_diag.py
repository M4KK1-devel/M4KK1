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


def px_at(w, px, x, y):
    i = (y * w + x) * 3
    return px[i], px[i + 1], px[i + 2]


def diff_cluster(a, b, near=None):
    """Find changed pixel bounding box vs shot b."""
    box = []
    for y in range(0, 600, 2):
        for x in range(0, 800, 2):
            i = (y * 800 + x) * 3
            if (a[i], a[i + 1], a[i + 2]) != (b[i], b[i + 1], b[i + 2]):
                box.append((x, y))
    if not box:
        return None
    xs = [p[0] for p in box]
    ys = [p[1] for p in box]
    return (min(xs), min(ys), max(xs), max(ys), len(box))


def main():
    subprocess.run(["rm", "-f", MON], check=False)
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
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    for _ in range(30):
        try:
            s.connect(MON)
            break
        except OSError:
            time.sleep(0.5)

    def cmd(line, wait=0.8):
        s.sendall(line.encode() + b"\n")
        time.sleep(wait)

    def shot():
        cmd("screendump /tmp/sp_cur.ppm", 0.8)
        with open("/tmp/sp_cur.ppm", "rb") as f:
            return parse_ppm(f.read())

    # Baseline
    a = shot()
    print("t0 win2 title px(240,176):", px_at(a[0], a[2], 240, 176), " btn2:", px_at(a[0], a[2], 260, 585), flush=True)

    # Move cursor in two steps to close button of win2 (233,179)
    cmd("mouse_move -100 -100", 1.5)   # (300,200)
    b = shot()
    print("t1 cursor cluster (diff a->b):", diff_cluster(a[2], b[2]), flush=True)
    cmd("mouse_move -67 -21", 1.5)     # (233,179)
    c = shot()
    print("t2 cursor cluster (diff b->c):", diff_cluster(b[2], c[2]), flush=True)
    print("t2 win2 title px(240,176):", px_at(c[0], c[2], 240, 176), flush=True)

    # Click close
    cmd("mouse_button 1", 0.5)
    d = shot()
    print("t3 while-pressed px(240,176):", px_at(d[0], d[2], 240, 176), flush=True)
    cmd("mouse_button 0", 1.5)
    e = shot()
    print("t4 after release px(240,176):", px_at(e[0], e[2], 240, 176), " btn2:", px_at(e[0], e[2], 260, 585), flush=True)
    print("t4 changed cluster (a->e):", diff_cluster(a[2], e[2]), flush=True)

    proc.kill()


if __name__ == "__main__":
    main()
