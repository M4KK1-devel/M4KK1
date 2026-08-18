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

    s.sendall(b"screendump /tmp/layout.ppm\n")
    time.sleep(1.2)
    proc.kill()

    with open("/tmp/layout.ppm", "rb") as f:
        w, h, px = parse_ppm(f.read())

    # Scan: for each row, count pixels matching each title color
    T0, T1, T2 = (128, 48, 48), (48, 128, 48), (48, 48, 128)
    BODY = (208, 208, 208)
    print("row: T0count T1count T2count bodycount  (sample at x=240)")
    for y in range(0, 600):
        c0 = c1 = c2 = cb = 0
        for x in range(0, 800, 4):
            i = (y * 800 + x) * 3
            p = px[i], px[i + 1], px[i + 2]
            if p == T0: c0 += 1
            elif p == T1: c1 += 1
            elif p == T2: c2 += 1
            elif p == BODY: cb += 1
        if c0 or c1 or c2 or cb:
            i = (y * 800 + 240) * 3
            print("%3d: %3d %3d %3d %3d  x240=%s" % (y, c0, c1, c2, cb, px[i:i + 3]))


if __name__ == "__main__":
    main()
