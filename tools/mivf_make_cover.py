#!/usr/bin/env python3
"""Create a MIVF ".cover" library-poster asset.

Fills the real gap identified in mivf_customization_gui_20260716's Phase A
audit (ENCODER_GUI_WORKFLOW_AUDIT.md #6): mivf_make_screensaver_cover.py was
the only image -> raw-RGB565 conversion tool that existed. This follows its
exact same pattern (letterbox fit, safe margin, optional BGR565 swap,
--force, --preview) for the 88x50 ".cover" format instead.

Phase C.1: image processing now goes through mivf_theme_asset_common.py,
shared with the GUI's live preview -- see that module's docstring for why.

Output format:
  - 88x50 pixels
  - raw headerless RGB565 little-endian
  - exact size: 8,800 bytes

Example:
  python tools/mivf_make_cover.py poster.png movie.cover --preview preview.png --force
"""
from __future__ import annotations

import argparse
from pathlib import Path
from PIL import Image

from mivf_theme_asset_common import fit_to_canvas, rgb565le_bytes

W, H = 88, 50
EXPECTED = W * H * 2


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help="source PNG/JPEG/etc.")
    ap.add_argument("output", help="output .cover")
    ap.add_argument("--bg", default="0,0,0", help="background RGB, default 0,0,0")
    ap.add_argument("--margin", type=int, default=2, help="safe margin around fitted image")
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

    image = Image.open(src)
    canvas = fit_to_canvas(image, W, H, bg=tuple(bg_parts), margin=args.margin, opaque=True)
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
