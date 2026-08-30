#!/usr/bin/env python3
"""Convert the icon PNG sources (F:/Source/icons/sources) into .svg
assets under the repo's assets/ directory.

Each SVG embeds the original PNG losslessly as a base64 data URI
(image/png) — the icons are pixel art with alpha gradients; tracing
them to vector paths would alter the artwork.  The SVG is a faithful,
self-contained wrapper: standard-compliant, previewable in any
browser, and the PNG can be regenerated bit-for-bit from it.
"""

import base64
import sys
from pathlib import Path

SRC = Path("F:/Source/icons/sources")
DST = Path("F:/M4KK1/assets")


def png_to_svg(src: Path) -> tuple[str, int, int]:
    """Wrap one PNG into an SVG with the same pixel dimensions."""
    # Parse IHDR for width/height (bytes 16..24, big-endian).
    data = src.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n" or len(data) < 24:
        raise ValueError(f"not a valid PNG: {src}")
    w = int.from_bytes(data[16:20], "big")
    h = int.from_bytes(data[20:24], "big")
    b64 = base64.b64encode(data).decode("ascii")
    svg = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" '
        f'xmlns:xlink="http://www.w3.org/1999/xlink" '
        f'width="{w}" height="{h}" viewBox="0 0 {w} {h}">\n'
        f'  <image width="{w}" height="{h}" '
        f'xlink:href="data:image/png;base64,{b64}"/>\n'
        "</svg>\n"
    )
    return svg, w, h


def main() -> int:
    converted = 0
    for cat in ("apps", "folder", "net", "files"):
        srcdir = SRC / cat
        if not srcdir.is_dir():
            print(f"skip: {srcdir} missing")
            continue
        outdir = DST / cat
        outdir.mkdir(parents=True, exist_ok=True)
        for png in sorted(srcdir.glob("*.png")):
            svg_text, w, h = png_to_svg(png)
            out = outdir / (png.stem + ".svg")
            out.write_text(svg_text, encoding="utf-8", newline="\n")
            print(f"{cat}/{png.name} ({w}x{h}) -> {out}")
            converted += 1
    print(f"converted: {converted}")
    return 0 if converted else 1


if __name__ == "__main__":
    sys.exit(main())
