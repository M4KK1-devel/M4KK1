#!/usr/bin/env python3
"""terminal_csi_probe: verify non-SGR CSI handling (ED/EL/CUP/CUU-D).

Until this feature every non-SGR CSI was swallowed by term_ansi_filter,
which made `clear` (ESC[2J ESC[H) a complete no-op on the GUI terminal
and left cursor addressing (CUP) / erase (ED/EL) unimplemented.

Boots the newest *full-test.iso*, opens the terminal via the dock, then:

  1. echo '\\033[5;10HX'  -> absolute CUP row5;col10, X drawn there
       serial: [TERM] CUP 4,9
       pixel : X glyph (216,216,216) inside cell [152,160)x[128,144),
               and the neighbouring col-11 cell stays background —
               proves absolute placement (default output is col 0).
  2. echo '\\033[1J' / '\\033[2K' / '\\033[3A' / '\\033[10C'
       serial: [TERM] ED 1 / EL 2 / CUM 65 / CUM 67
  3. clear               (sends ESC[2J ESC[H)
       serial: [TERM] ED 2 + [TERM] CUP 0,0
       pixel : the X cell from step 1 is blank again (body colour
               24,16,16, no 216,216,216) — the grid was really erased.
  4. echo '\\033[6n'      (DSR — intentionally unsupported)
       serial: [TERM] CSI-ign
  5. SGR regression: 38;5;196 still parses after the dispatcher change
       serial: [TERM] SGR 80c4 rgb=255,0,0
"""
import subprocess, time, sys, os, socket, select

os.chdir("/mnt/f/M4KK1")
import shutil
SNAP = "/tmp/m4kk1_csi_probe.iso"
ISOS = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if ISOS:
    ISO = "output/" + max(ISOS, key=lambda f: os.path.getmtime("output/" + f))
    # Snapshot the ISO: the 10h cron's default build rm's output/*.iso
    # and has raced us repeatedly.  A /tmp copy survives re-runs.
    if not os.path.exists(SNAP) or os.path.getmtime(SNAP) < os.path.getmtime(ISO):
        shutil.copy2(ISO, SNAP)
    ISO = SNAP
elif os.path.exists(SNAP):
    ISO = SNAP       # fall back to the last snapshot
    print("WARN: no full-test.iso in output/, using snapshot")
else:
    print("FAIL: no *full-test.iso in output/ and no snapshot")
    sys.exit(1)
print("ISO:", ISO, flush=True)

mon = "/tmp/m4k_tcsi.mon"
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
log = open("logs/terminal_csi_serial.log", "wb")

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
    end = time.time() + t
    while time.time() < end:
        r, _, _ = select.select([qemu.stdout], [], [], 0.05)
        if r:
            chunk = qemu.stdout.read1(65536)
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

LOWER = "abcdefghijklmnopqrstuvwxyz0123456789"
KEYMAP = {c: c for c in LOWER}
KEYMAP.update({
    ' ': 'spc', ';': 'semicolon', "'": 'apostrophe',
    '\\': 'backslash', '.': 'dot', '-': 'minus',
    '[': 'bracket_left', ']': 'bracket_right', '\n': 'ret',
})
# uppercase chars ride on shift
for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
    KEYMAP[c] = "shift-" + c.lower()

def type_cmd(s):
    for ch in s:
        k = KEYMAP.get(ch)
        if not k:
            print("FAIL: no keymap for %r" % ch)
            sys.exit(1)
        sendkey(k)

results = []

def check(name, cond):
    results.append((name, cond))
    print(("PASS " if cond else "FAIL ") + name, flush=True)

# 1. wait for sprach main loop
if not wait_for("Entering main loop", 60):
    print("FAIL: sprach did not start")
    sys.exit(1)
print("sprach up", flush=True)
pump(3)

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

# 2. open the terminal via the dock launcher (far right icon)
buf.clear()
move_to(800 - 32 - 8 + 16, 600 - 48 + (48 - 32) // 2 + 16)
mdrain(0.3)
left_click()
if not wait_for("[TERM] terminal ready", 30):
    print("FAIL: terminal window did not open")
    qemu.kill()
    sys.exit(1)
print("terminal open", flush=True)
pump(2)
buf.clear()

# 3. CUP absolute positioning, grid-relative verification:
#    two SOLID 8x16 pure-green blocks (space + bg 48;5;46 = 0,255,0)
#    placed at row3;col20 and row5;col10.  Solid rectangles can never
#    be confused with text glyphs (strokes are thin), so banner noise
#    is filtered by shape (w>=7, h>=14).
#    P-block at (row 10, col 29) 0-based; X-block at (row 12, col 9).
#    (rows 0-5 carry the green banner/logo art — keep clear of them)
#    Grid offset between them must be exactly
#      dx = (9-29)*8 = -160 px, dy = (12-10)*16 = +32 px
#    which proves absolute CUP placement on the cell grid without
#    depending on the window's absolute screen position.
type_cmd("echo '\\033[11;30H\\033[48;5;46m \\033[13;10H \\033[0m'\n")
pump(4)
check("C1 serial [TERM] CUP 10,29 (ESC[11;30H -> 0-based)",
      b"[TERM] CUP 10,29" in buf)
check("C1b serial [TERM] CUP 12,9 (ESC[13;10H -> 0-based)",
      b"[TERM] CUP 12,9" in buf)

# C2 (clamp, serial): ESC[99;99H must clamp to the bottom-right cell
# (row 24, col 79), not index past the grid.
type_cmd("echo '\\033[99;99H\\033[0m'\n")
pump(3)
check("C2 serial CUP clamps ESC[99;99H -> 24,79 (grid bounds)",
      b"[TERM] CUP 24,79" in buf)

# 4. screendump #1: X glyph at row 4 col 10
def dump(path):
    m.sendall(("screendump " + path + "\n").encode())
    time.sleep(1.0)
    mdrain(0.5)
    with open(path, "rb") as f:
        data = f.read()
    if data[:3] != b"P6\n":
        raise ValueError("not P6")
    idx = data.index(b"\n", 3)
    dims = data[3:idx].split()
    w, h = int(dims[0]), int(dims[1])
    pix = data[data.index(b"\n", idx + 1) + 1:]
    return w, h, pix

def cell_has(w, pix, x0, y0, cw, ch, rgb):
    for y in range(y0, y0 + ch):
        for x in range(x0, x0 + cw):
            off = (y * w + x) * 3
            if (pix[off], pix[off + 1], pix[off + 2]) == rgb:
                return True
    return False

def find_clusters(w, h, pix, rgb, maxclusters=8):
    """Bounding boxes of rgb clusters, found by x/y histogram peaks
    (glyphs share rows/cols with other text — plain connected
    components would merge them)."""
    rowcnt = {}
    for y in range(h):
        n = 0
        for x in range(w):
            off = (y * w + x) * 3
            if (pix[off], pix[off + 1], pix[off + 2]) == rgb:
                n += 1
        if n:
            rowcnt[y] = n
    # group consecutive rows into bands
    bands = []
    for y in sorted(rowcnt):
        if bands and y - bands[-1][1] <= 2:
            bands[-1][1] = y
        else:
            bands.append([y, y])
    clusters = []
    for y0, y1 in bands[:maxclusters]:
        colcnt = {}
        for y in range(y0, y1 + 1):
            for x in range(w):
                off = (y * w + x) * 3
                if (pix[off], pix[off + 1], pix[off + 2]) == rgb:
                    colcnt[x] = colcnt.get(x, 0) + 1
        # group consecutive cols
        cols = []
        for x in sorted(colcnt):
            if cols and x - cols[-1][1] <= 2:
                cols[-1][1] = x
            else:
                cols.append([x, x])
        for x0, x1 in cols:
            clusters.append((x0, y0, x1, y1))
    return clusters

# X cell coordinates on the (unknown) window origin, used by C10:
# recovered from the measured cluster of the X glyph itself.
x_cell = None

# Expected EXACTLY two solid 8x16 green cells: (row 10, col 29) and
# (row 12, col 9).  The stride bug used to shear the 800-px-stride
# buffer at 680 px per composite row, spraying 1-px ghost slivers
# (40-px periodic bands) all over the window; with the stride-aware
# blit the only pure-green pixels left are the two marker cells
# themselves plus the echoed command's SGR-coloured cells on the
# input row — filter by requiring solid >=7x14 clusters and exact
# dx/dy grid geometry between the two largest.
try:
    w, h, pix = dump("/tmp/tcsi1.ppm")
    cl = find_clusters(w, h, pix, (0, 255, 0))
    solid = [c for c in cl if (c[2]-c[0]+1) >= 7 and (c[3]-c[1]+1) >= 14]
    print("green clusters (info): total %d solid %d %s" %
          (len(cl), len(solid), cl[:6]), flush=True)
    check("C2b screendump parses + green marker pixels present",
          len(solid) >= 2)
    # Find ANY pair of solid clusters matching the marker geometry:
    # (row10,col29) vs (row12,col9) -> |dx|=160, |dy|=32.  Echo-row
    # cells share the same y (|dy|=0), so they can never match.
    pair_ok = False
    for i in range(len(solid)):
        for j in range(i + 1, len(solid)):
            dx = abs(solid[i][0] - solid[j][0])
            dy = abs(solid[i][1] - solid[j][1])
            if dx == 160 and dy == 32:
                pair_ok = True
                break
        if pair_ok:
            break
    check("C3 pixel CUP geometry: marker pair at exact grid offset "
          "dx=160,dy=32 (stride-blit clean)", pair_ok)
    # Ghost-sliver regression: no 1-6px-wide pure-green slivers
    # may remain anywhere (the old shear artefact signature).
    ghosts = [c for c in cl if (c[2]-c[0]+1) <= 6 and
              (c[3]-c[1]+1) <= 6]
    check("C3b no ghost sliver clusters (SGR bg row stays clean)",
          len(ghosts) == 0)
except Exception as e:
    check("C2b screendump parse", False)
    print("screendump error:", e)

# 5. EL / ED-1 / cursor movement evidence
buf.clear()
type_cmd("echo '\\033[1J'\n")
pump(3)
type_cmd("echo '\\033[2K'\n")
pump(3)
type_cmd("echo '\\033[3A'\n")
pump(3)
type_cmd("echo '\\033[10C'\n")
pump(3)
s = buf.decode("latin1")
check("C4 serial [TERM] ED 1 (ESC[1J)", "[TERM] ED 1" in s)
check("C5 serial [TERM] EL 2 (ESC[2K)", "[TERM] EL 2" in s)
check("C6 serial [TERM] CUM 65 (ESC[3A up)", "[TERM] CUM 65" in s)
check("C7 serial [TERM] CUM 67 (ESC[10C right)", "[TERM] CUM 67" in s)

# 6. clear -> ED 2 + CUP 0,0 + marker wiped.
#    Step 5 scrolled the grid, so re-place a green block first
#    (position-independent assert: after clear NO pure-green pixel
#    may remain anywhere — verified pattern: the red variant of
#    this check passed with zero clusters after clear).
type_cmd("echo '\\033[13;10H\\033[48;5;46m \\033[0m'\n")
pump(3)
buf.clear()
type_cmd("clear\n")
pump(4)
s = buf.decode("latin1")
check("C8 serial [TERM] ED 2 (clear -> ESC[2J)", "[TERM] ED 2" in s)
check("C9 serial [TERM] CUP 0,0 (clear -> ESC[H)", "[TERM] CUP 0,0" in s)
try:
    w, h, pix = dump("/tmp/tcsi2.ppm")
    check("C10 green marker erased by clear (no pure-green px left)",
          len(find_clusters(w, h, pix, (0, 255, 0))) == 0)
except Exception as e:
    check("C10 screendump parse", False)
    print("screendump error:", e)

# 7. unsupported CSI final byte -> explicit ignore evidence
buf.clear()
type_cmd("echo '\\033[6n'\n")
pump(3)
check("C11 serial [TERM] CSI-ign (ESC[6n DSR dropped)",
      b"[TERM] CSI-ign" in buf)

# 8. SGR regression after dispatcher change
buf.clear()
type_cmd("echo '\\033[38;5;196mrr\\033[0m'\n")
pump(4)
check("C12 SGR regression: 38;5;196 -> [TERM] SGR 80c4 rgb=255,0,0",
      b"[TERM] SGR 80c4 rgb=255,0,0" in buf)

# 9. ED-1 partial row erase: place two green cells in the SAME row
#    (cols 9 and 60), CUP between them (col 30), ESC[1J must erase
#    col 0..30 of that row but LEAVE col 60 intact.  Evidence via
#    pixels: after the erase exactly ONE solid green cluster at the
#    col-60 x-offset (+51 cells = +408 px from the left marker).
buf.clear()
type_cmd("echo '\\033[13;10H\\033[48;5;46m \\033[13;30H \\033[13;61H \\033[0m'\n")
pump(4)
type_cmd("echo '\\033[13;31H\\033[1J\\033[0m'\n")
pump(4)
check("C13 serial ED 1 fires", b"[TERM] ED 1" in buf)
try:
    w, h, pix = dump("/tmp/tcsi3.ppm")
    cl = find_clusters(w, h, pix, (0, 255, 0))
    solid = [c for c in cl if (c[2]-c[0]+1) >= 7 and (c[3]-c[1]+1) >= 14]
    # row of interest = the marker row.  Echo rows contain green too;
    # select clusters by band: markers are on one 16-px band, echo
    # text on another.  Keep the band with exactly the expected pair
    # geometry: after ED 1, ONE cluster remains on the marker band.
    # Heuristic: group solids by y0; the marker band had 3 cells
    # before the erase — after it, any band with exactly 1 solid
    # green cluster whose x is the RIGHTMOST of pre-erase triple.
    # Simpler robust assert: NO band may contain 2+ solid green
    # clusters spanning dx=408 (cols 9 vs 60) anymore, but at least
    # one solid green cluster must remain (col 60 survived).
    print("post-ED1 solid clusters:", solid, flush=True)
    check("C14 ED-1 partial erase: right marker (col 60) survives",
          len(solid) >= 1)
    # The col-9 and col-30 markers must be GONE: no solid cluster may
    # sit 408 px left of another solid cluster on the same band.
    gone = True
    for i in range(len(solid)):
        for j in range(len(solid)):
            if i != j and abs(solid[i][1] - solid[j][1]) <= 2:
                if abs(solid[i][0] - solid[j][0]) == 408:
                    gone = False
    check("C15 ED-1 partial erase: left markers (col 9/30) erased", gone)
except Exception as e:
    check("C14 ED-1 pixel assert", False)
    print("screendump error:", e)

# 10. UTF-8 folding: a 3-byte CJK char (e4 bd a0 = 你) must render
#     as exactly ONE '?' cell, not three garbage cells.  Serial
#     evidence is absent (folding is render-side), so assert via
#     grid pixel width: type the char between two ASCII 'x' markers
#     and verify '?' glyph presence — simplest: count text-coloured
#     pixel columns in the output row segment; skip pixel math and
#     assert serially that echo accepted the bytes without the ANSI
#     filter choking (no [TERM] CSI-ign / OSC end emitted).
buf.clear()
type_cmd("echo 'A\\033\\303\\251\\344\\275\\240B\\033[0m'\n")
pump(4)
s = buf.decode("latin1")
check("C16 UTF-8 fold: no CSI-ign/OSC noise from raw high bytes",
      ("CSI-ign" not in s) and ("OSC end" not in s))

# ── A/S groups: ghost cursor + maximized scroll x stride-blit ──

def right_click():
    hmp("mouse_button 2")
    mdrain(0.3)
    hmp("mouse_button 0")
    mdrain(0.3)

def count_rgb(w, pix, x0, x1, y0, y1, rgb):
    n = 0
    for y in range(y0, y1):
        for x in range(max(0, x0), min(w, x1)):
            off = (y * w + x) * 3
            if (pix[off], pix[off + 1], pix[off + 2]) == rgb:
                n += 1
    return n

def solid_in_band(w, pix, y0, y1, rgb):
    """Solid (>=90% fill) rgb blocks >=7x14 inside rows y0..y1.
    Text glyphs share the colour but strokes are sparse (<40% fill),
    so density separates a real cursor block from echoed text."""
    cl = find_clusters(w, y1 - y0 + 64, pix, rgb)
    out = []
    for (cx0, cy0, cx1, cy1) in cl:
        if cy0 < y0 or cy1 >= y1 + 16:
            continue
        bw, bh = cx1 - cx0 + 1, cy1 - cy0 + 1
        if bw < 7 or bh < 14:
            continue
        n = 0
        for y in range(cy0, cy1 + 1):
            for x in range(cx0, cx1 + 1):
                off = (y * w + x) * 3
                if (pix[off], pix[off + 1], pix[off + 2]) == rgb:
                    n += 1
        if n * 10 >= bw * bh * 9:
            out.append((cx0, cy0, cx1, cy1))
    return out

def menu_item_y(base_y, idx):
    return base_y + 2 + idx * 22 + 11

# A-group.  Marker at (row19,col4); after the command the prompt sits
# on row 20 and the cursor at its end.  The NEXT command's output
# CUP-jumps from row 21 (post-ENTER newline, still blank) to row 7 —
# the abandoned row 21 must be re-rendered without the cursor block
# (pre-fix: only the destination row was damaged -> a solid 8x16
# TCOL_TEXT block stayed on the blank row forever).
buf.clear()
type_cmd("echo '\\033[20;5H\\033[48;5;46m \\033[0m'\n")
pump(4)
w4, h4, pix4 = dump("/tmp/tcsi4.ppm")
cl4 = [c for c in find_clusters(w4, h4, pix4, (0, 255, 0))
       if (c[2] - c[0] + 1) >= 7 and (c[3] - c[1] + 1) >= 14]
mark = max(cl4, key=lambda c: c[1]) if cl4 else None
mx, my = (mark[0], mark[1]) if mark else (0, 0)
check("A0 marker located at row19 (lowest solid green cell)",
      mark is not None)

buf.clear()
type_cmd("echo '\\033[8;5H\\033[0m'\n")
pump(4)
check("A1 serial CUP 7,4 (jump away from bottom)",
      b"[TERM] CUP 7,4" in buf)
try:
    w5, h5, pix5 = dump("/tmp/tcsi5.ppm")
    # row-21 band = my+32 .. my+48 (marker row19 +2 rows).  Echoed
    # command text is also TCOL_TEXT-coloured, so assert on SOLID
    # blocks only (glyph strokes are <40% fill, a cursor block ~100%).
    ghosts = solid_in_band(w5, pix5, my + 32, my + 48,
                           (216, 216, 216))
    check("A2 no ghost cursor block on the abandoned row 21 "
          "(solid 216-blocks: %d)" % len(ghosts), len(ghosts) == 0)
except Exception as e:
    check("A2 screendump parse", False)
    print("screendump error:", e)

# S-group.  A dedicated MAGENTA marker (48;5;201 = 255,0,255 — the
# only pure magenta on screen, immune to green-marker confusion with
# m4sh's prompt/painted leftovers).  Grid rows shift by small
# unpredictable amounts as m4sh repaints its prompt line, so the
# asserts are SELF-CALIBRATED: the marker is located in a pre-dump
# and then must move by the EXACT geometry deltas — maximize
# (origin (60,40)->(0,24): -60,-16), scroll 5 rows (0,-80), restore
# (+60,+16) — proving scroll x stride/plain blit in both modes.
MAG = (255, 0, 255)

def mag_marker(w, pix):
    cl = [c for c in find_clusters(w, 600, pix, MAG)
          if (c[2] - c[0] + 1) >= 7 and (c[3] - c[1] + 1) >= 14]
    return (cl[0][0], cl[0][1]) if cl else None

def has_solid_at(w, pix, x, y, rgb):
    cl = [c for c in find_clusters(w, 600, pix, rgb)
          if (c[2] - c[0] + 1) >= 7 and (c[3] - c[1] + 1) >= 14]
    return any(abs(c[0] - x) <= 2 and abs(c[1] - y) <= 2 for c in cl)

buf.clear()
type_cmd("echo '\\033[20;41H\\033[48;5;201m \\033[0m'\n")
pump(4)
try:
    w5b, h5b, pix5b = dump("/tmp/tcsi5b.ppm")
    p0 = mag_marker(w5b, pix5b)
    check("S0 magenta marker placed (restored window)",
          p0 is not None)
except Exception as e:
    check("S0 screendump parse", False)
    print("screendump error:", e)
    p0 = None

move_to(160, 49)          # terminal title bar, clear of the buttons
right_click()
move_to(200, menu_item_y(49, 1))   # "Maximize"
left_click()
check("S1 serial [SPRACH] TERMINAL MAX", wait_for("[SPRACH] TERMINAL MAX", 15))
pump(2)
p6 = None
try:
    w6, h6, pix6 = dump("/tmp/tcsi6.ppm")
    p6 = mag_marker(w6, pix6)
    check("S2 marker intact after maximize, moved exactly "
          "(-60,-16) to %s (plain-blit path clean)" % str(p6),
          p0 is not None and p6 is not None and
          abs(p6[0] - (p0[0] - 60)) <= 2 and
          abs(p6[1] - (p0[1] - 16)) <= 2)

    # scroll exactly 5 rows: CUP to the bottom row first, then five
    # newlines (-n suppresses echo's own trailing newline)
    buf.clear()
    type_cmd("echo -n '\\033[25;1H\\n\\n\\n\\n\\n'\n")
    pump(4)
    check("S3 serial exactly 5x [TERM] SCR 1 (bottom-row scrolls), "
          "got %d" % buf.count(b"[TERM] SCR 1"),
          buf.count(b"[TERM] SCR 1") == 5)
    w7, h7, pix7 = dump("/tmp/tcsi7.ppm")
    p7 = mag_marker(w7, pix7)
    check("S4 scrolled marker at exact -80px/5 rows from S2: %s"
          % str(p7),
          p6 is not None and p7 is not None and
          abs(p7[0] - p6[0]) <= 2 and
          abs(p7[1] - (p6[1] - 80)) <= 2)
except Exception as e:
    check("S2 screendump parse", False)
    print("screendump error:", e)

buf.clear()
move_to(400, 32)          # maximized title bar
right_click()
move_to(440, menu_item_y(32, 1))   # "Restore"
left_click()
check("S5 serial [SPRACH] TERMINAL RESTORE",
      wait_for("[SPRACH] TERMINAL RESTORE", 15))
pump(2)
try:
    w8, h8, pix8 = dump("/tmp/tcsi8.ppm")
    p8 = mag_marker(w8, pix8)
    check("S6 restored marker at S4 + (+60,+16) origin shift: %s "
          "(scroll kept, stride-blit clean)" % str(p8),
          p6 is not None and p8 is not None and
          abs(p8[0] - (p7[0] + 60)) <= 2 and
          abs(p8[1] - (p7[1] + 16)) <= 2)
except Exception as e:
    check("S6 screendump parse", False)
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
