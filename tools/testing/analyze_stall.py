import re, sys

log = open(r"F:\M4KK1\output\s30.log", "rb").read().decode("latin1")

# Split into lines, but probes interrupt marker streams mid-line.
# Rebuild a token stream: split on 'P' followed by U/K.
tokens = re.split(r'(?=P[UK])', log)

stalls = [m.start() for m in re.finditer(r'WM heartbeat stalled', log)]
kills = [m.start() for m in re.finditer(r'^KILL ', log, re.M)]
print(f"log bytes={len(log)} stalls={len(stalls)} kills={len(kills)}")
for i, s in enumerate(stalls):
    # find next kill after this stall
    nxt = [k for k in kills if k > s]
    tail = log[max(0, s-1500):s]
    # count A markers between stalls (sprach cycles)
    if i < len(stalls)-1:
        seg = log[s:stalls[i+1]]
    else:
        seg = log[s:]
    a_cnt = seg.count('A')
    qlines = len(re.findall(r'Q cur=', seg))
    print(f"stall {i}: at byte {s}, next-kill delta={nxt[0]-s if nxt else -1}, "
          f"A-marks in following seg={a_cnt}, Q-lines in seg={qlines}")

def parse_q(text):
    m = re.search(r'Q cur=(\d+):(\w+) esp=([0-9A-Fa-f]+) tags=([0-9A-Fa-f]+) q=\[([0-9,]*)\]', text)
    if not m:
        return None
    return (int(m.group(1)), m.group(2), m.group(3), m.group(4), m.group(5))

print("\n=== Transition analysis around stall 0 (2000 bytes before) ===")
seg = log[max(0, stalls[0]-2000):stalls[0]]
prev = None
for tok in re.split(r'(?=P[UK])', seg):
    q = parse_q(tok)
    if q:
        cur, name, esp, tags, que = q
        eip = ''
        m = re.search(r'P[UK]([0-9A-Fa-f]+)', tok)
        if m: eip = m.group(1)
        print(f"P{eip:7s} cur={cur}:{name} esp={esp} tags={tags} q=[{que}]")
