#!/usr/bin/env python3
"""Create a MIVF .screensaver.cover asset.

Output format:
  - 96x54 pixels
  - raw headerless RGB565 little-endian
  - exact size: 10,368 bytes

Example:
  python tools/mivf_make_screensaver_cover.py logo.png les-mis.screensaver.cover --preview les-mis_preview.png --force
"""
from __future__ import annotations

import argparse
from pathlib import Path
from PIL import Image, ImageOps

W, H = 96, 54
EXPECTED = W * H * 2


def rgb565le_bytes(img: Image.Image, bgr: bool = False) -> bytes:
    img = img.convert("RGB")
    raw = bytearray()
    for r, g, b in img.getdata():
        if bgr:
            r, b = b, r
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        raw.append(v & 0xFF)
        raw.append((v >> 8) & 0xFF)
    return bytes(raw)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help="source PNG/JPEG/etc.")
    ap.add_argument("output", help="output .screensaver.cover")
    ap.add_argument("--bg", default="0,0,0", help="background RGB, default 0,0,0")
    ap.add_argument("--margin", type=int, default=3, help="safe margin around fitted image")
    ap.add_argument("--bgr565", action="store_true", help="swap R/B if your loader expects BGR565")
    ap.add_argument("--preview", help="optional PNG preview output")
    ap.add_argument("--force", action="store_true", help="overwrite output")
    args = ap.parse_args()

    src = Path(args.input)
    out = Path(args.output)
    if out.exists() and not args.force:
        raise SystemExit(f"refusing to overwrite {out}; pass --force")

    bg_parts = [int(x) for x in args.bg.split(",")]
    if len(bg_parts) != 3 or any(x < 0 or x > 255 for x in bg_parts):
        raise SystemExit("--bg must be R,G,B with each value 0..255")

    image = Image.open(src).convert("RGBA")
    bbox = image.getbbox()
    if bbox:
        image = image.crop(bbox)

    max_w = max(1, W - args.margin * 2)
    max_h = max(1, H - args.margin * 2)
    image = ImageOps.contain(image, (max_w, max_h), method=Image.Resampling.LANCZOS)

    canvas = Image.new("RGBA", (W, H), tuple(bg_parts) + (255,))
    x = (W - image.width) // 2
    y = (H - image.height) // 2
    canvas.alpha_composite(image, (x, y))
    rgb = canvas.convert("RGB")

    raw = rgb565le_bytes(rgb, bgr=args.bgr565)
    if len(raw) != EXPECTED:
        raise SystemExit(f"internal size error: expected {EXPECTED}, got {len(raw)}")
    out.write_bytes(raw)
    if out.stat().st_size != EXPECTED:
        raise SystemExit(f"write size error: expected {EXPECTED}, got {out.stat().st_size}")

    if args.preview:
        rgb.save(args.preview)

    print(f"WROTE {out}")
    print(f"SIZE  {out.stat().st_size} bytes")


if __name__ == "__main__":
    main()
