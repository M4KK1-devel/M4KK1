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


ACCENT = (255, 208, 96)
TBG = (32, 32, 48)


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

    def active():
        w, h, px = shot()
        return [px_at(w, px, 52 + i * 104, 580) == ACCENT for i in range(3)]

    def click(x, y):
        cmd("mouse_move %d %d" % (x - 400, y - 300), 1.5)
        cmd("mouse_button 1", 0.4)
        cmd("mouse_button 0", 1.5)

    print("initial active btn:", active(), flush=True)
    click(52, 585)     # btn0 center
    print("after btn0:", active(), flush=True)
    click(106, 585)    # 4px gap between btn0 (4..100) and btn1 (108..204)
    print("after gap 106:", active(), flush=True)
    click(108, 585)    # btn1 left edge
    print("after btn1 edge 108:", active(), flush=True)
    click(260, 585)    # btn2 center
    print("after btn2:", active(), flush=True)
    proc.kill()


if __name__ == "__main__":
    main()
