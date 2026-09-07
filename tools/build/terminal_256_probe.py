#!/usr/bin/env python3
"""terminal_256_probe: verify SGR 38;5;n / 48;5;n parsing + rendering.

Boots the newest *full-test.iso*, opens the terminal window via the
dock, types an echo command carrying 256-color SGR sequences into the
m4shg child, then asserts:

  A. Serial evidence lines from the terminal's SGR parser:
       [TERM] SGR 80c4 rgb=255,0,0     (38;5;196 -> pure red cube)
       [TERM] SGR 80d8 rgb=0,255,255   (38;5;51  -> pure cyan cube)
       [TERM] SGR 8000 rgb=216,216,216 (48;5;n + reset path)
     0x80c4 = ATTR_256|196, 0x80d8 = ATTR_256|51, 0x8000? no — reset
     returns ATTR_TEXT=0, so the third assert uses 0000 after a 0 code.
  B. Screen pixels: screendump PPM, sample the echo'd text row and
     confirm red-family / cyan-family glyph pixels appeared (fg 196
     renders FF0000, fg 51 renders 00FFFF).

Non-goal: exact glyph positions — text row found by scanning for the
marker line's distinctive colors.
"""
import subprocess, time, sys, os, socket

os.chdir("/mnt/f/M4KK1")
ISOS = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not ISOS:
    print("FAIL: no *full-test.iso in output/")
    sys.exit(1)
ISO = "output/" + max(ISOS, key=lambda f: os.path.getmtime("output/" + f))
print("ISO:", ISO, flush=True)

mon = "/tmp/m4k_t256.mon"
if os.path.exists(mon):
    os.unlink(mon)

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", ISO, "-m", "512",
    "-vga", "std", "-serial", "stdio",
    "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-net", "none", "-no-reboot"],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL)

for _ in range(30):
    if os.path.exists(mon):
        break
    time.sleep(0.3)
m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
m.connect(mon)
m.setblocking(False)

buf = bytearray()
os.makedirs("logs", exist_ok=True)
log = open("logs/terminal_256_serial.log", "wb")

def mdrain(t=0.25):
    end = time.time() + t
    while time.time() < end:
        try:
            m.recv(65536)
        except BlockingIOError:
            pass
        time.sleep(0.05)

def sendkey(keys):
    m.sendall(("sendkey " + keys + "\n").encode())
    time.sleep(0.3)

def pump(t=2.0):
    """Read serial + monitor traffic into buf/log."""
    end = time.time() + t
    while time.time() < end:
        chunk = qemu.stdout.read1(65536) if hasattr(qemu.stdout, "read1") else b""
        if chunk:
            buf.extend(chunk)
            log.write(chunk)
            log.flush()
        try:
            m.recv(65536)
        except BlockingIOError:
            pass
        time.sleep(0.05)

def wait_for(pattern, timeout=30):
    end = time.time() + timeout
    while time.time() < end:
        pump(0.5)
        if pattern.encode() in buf:
            return True
    return False

def type_cmd(s):
    """Type a lowercase command string; chars mapped 1:1."""
    KEYMAP = {
        'a':'a','b':'b','c':'c','d':'d','e':'e','f':'f','g':'g','h':'h',
        'i':'i','j':'j','k':'k','l':'l','m':'m','n':'n','o':'o','p':'p',
        'q':'q','r':'r','s':'s','t':'t','u':'u','v':'v','w':'w','x':'x',
        'y':'y','z':'z','0':'0','1':'1','2':'2','3':'3','4':'4','5':'5',
        '6':'6','7':'7','8':'8','9':'9',' ':'spc',';':'semicolon',
        "'":'apostrophe','\\':'backslash','.':'dot','-':'minus',
        '[':'bracket_left',']':'bracket_right','\n':'ret',
    }
    for ch in s:
        k = KEYMAP.get(ch)
        if not k:
            print("FAIL: no keymap for %r" % ch)
            sys.exit(1)
        sendkey(k)

results = []

# 1. wait for sprach main loop
if not wait_for("Entering main loop", 60):
    print("FAIL: sprach did not start")
    sys.exit(1)
print("sprach up", flush=True)
pump(3)

# HMP mouse_move is RELATIVE — track absolute position ourselves.
gx, gy = 400, 300

def hmp(cmd):
    m.sendall((cmd + "\n").encode())
    time.sleep(0.05)

def move_to(tx, ty, step=8):
    global gx, gy
    n = 0
    while (gx != tx or gy != ty) and n < 400:
        dx = max(-step, min(step, tx - gx))
        dy = max(-step, min(step, ty - gy))
        hmp("mouse_move %d %d" % (dx, dy))
        gx += dx; gy += dy
        n += 1

def left_click():
    hmp("mouse_button 1")
    mdrain(0.3)
    hmp("mouse_button 0")
    mdrain(0.3)

# 2. open the terminal via the dock launcher (far right icon,
#    same target as focus_policy_probe)
buf.clear()
move_to(800 - 32 - 8 + 16, 600 - 48 + (48 - 32) // 2 + 16)
mdrain(0.3)
left_click()
if not wait_for("[SPRACH] terminal window registered", 30):
    print("FAIL: terminal window did not open")
    qemu.kill()
    sys.exit(1)
print("terminal open", flush=True)
pump(2)

# 3. the new-window focus policy already focused the terminal —
#    do NOT click anywhere (a guessed title-bar click can steal
#    focus back to another window).  Type directly.
buf.clear()

# 4. type the test command (single line, lowercase):
#    echo '\033[38;5;196mrr\033[38;5;51mcc\033[48;5;21mbb\033[0mdd'
#    echo interprets \033 octal escapes by default; single quotes
#    keep the whole thing one argument.
literal = "echo '\\033[38;5;196mrr\\033[38;5;51mcc\\033[48;5;21mbb\\033[0mdd'"
type_cmd(literal + "\n")
pump(4)

# 4. serial asserts
def check(name, cond):
    results.append((name, cond))
    print(("PASS " if cond else "FAIL ") + name, flush=True)

s = buf.decode("latin1")
check("A1 serial [TERM] SGR 80c4 rgb=255,0,0 (fg 196)",
      "[TERM] SGR 80c4 rgb=255,0,0" in s)
check("A2 serial [TERM] SGR 8033 rgb=0,255,255 (fg 51)",
      b"[TERM] SGR 8033 rgb=0,255,255" in buf)
check("A3 serial [TERM] SGR 0000 after reset (48;5 + 0)",
      "[TERM] SGR 0000" in s)

# 5. screendump pixel assert
m.sendall(b"screendump /tmp/t256.ppm\n")
time.sleep(1.0)
mdrain(0.5)
try:
    with open("/tmp/t256.ppm", "rb") as f:
        data = f.read()
    # P6 header: parse
    if data[:3] != b"P6\n":
        raise ValueError("not P6")
    idx = data.index(b"\n", 3)
    dims = data[3:idx].split()
    w, h = int(dims[0]), int(dims[1])
    pix = data[data.index(b"\n", idx + 1) + 1:]
    def sample_rgba(x, y):
        off = (y * w + x) * 3
        return pix[off], pix[off + 1], pix[off + 2]
    # scan full frame for pure red (255,0,0) and pure cyan (0,255,255)
    found_red = found_cyan = False
    step = 3
    npix = w * h
    for off in range(0, npix * 3, 3 * step):
        r, g, b = pix[off], pix[off + 1], pix[off + 2]
        if (r, g, b) == (255, 0, 0):
            found_red = True
        elif (r, g, b) == (0, 255, 255):
            found_cyan = True
        if found_red and found_cyan:
            break
    check("B1 screen pure-red pixels (fg 38;5;196 rendered)", found_red)
    check("B2 screen pure-cyan pixels (fg 38;5;51 rendered)", found_cyan)
except Exception as e:
    check("B screendump parse", False)
    print("screendump error:", e)

m.sendall(b"quit\n")
time.sleep(0.5)
try:
    qemu.wait(timeout=5)
except Exception:
    qemu.kill()

ok = all(c for _, c in results)
print("=== %d/%d checks passed ===" % (sum(1 for _, c in results if c), len(results)))
sys.exit(0 if ok else 1)
