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

    # 1. keyboard 'q' test
    open("/tmp/sprach_serial.log", "wb").close()  # fresh log marker
    time.sleep(2)
    cmd("sendkey q", 2.0)
    time.sleep(2)
    with open("/tmp/sprach_serial.log", "rb") as f:
        log = f.read()
    print("after sendkey q: 'quitting' in log:", b"quitting" in log, flush=True)
    time.sleep(8)  # allow Copland watchdog restart

    # 2. taskbar active-switch click: btn0 center (4+48, 585) = (52,585)
    w, h, px = shot()
    print("btn0 px:", px_at(w, px, 52, 585), " btn2 px:", px_at(w, px, 260, 585), flush=True)
    cmd("mouse_move -348 -285", 1.5)  # from (400,300) to (52,585)
    cmd("mouse_button 1", 0.5)
    cmd("mouse_button 0", 1.5)
    w, h, px = shot()
    print("after click btn0 -> btn0 px:", px_at(w, px, 52, 585), " btn2 px:", px_at(w, px, 260, 585), flush=True)

    proc.kill()


if __name__ == "__main__":
    main()
