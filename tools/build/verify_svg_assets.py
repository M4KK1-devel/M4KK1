import base64, re, hashlib
from pathlib import Path

ok = 0
bad = 0
for svg in Path("assets").rglob("*.svg"):
    m = re.search(r"base64,([A-Za-z0-9+/=]+)", svg.read_text())
    png = base64.b64decode(m.group(1))
    orig = Path("F:/Source/icons/sources") / svg.parent.name / (svg.stem + ".png")
    h1 = hashlib.sha256(png).hexdigest()
    h2 = hashlib.sha256(orig.read_bytes()).hexdigest()
    if h1 == h2:
        ok += 1
    else:
        bad += 1
        print("MISMATCH", svg)
print(f"lossless check: {ok} ok, {bad} bad")
