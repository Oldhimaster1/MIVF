#!/usr/bin/env python3
"""Create a MIVF Phase C customization dashboard-background asset.

New asset type, new format -- see mivf_customization_gui_20260716/
CUSTOMIZATION_ASSET_FORMAT_DECISION.md and phase_c/ evidence. Written and
read by source/mivf_customization.c's MVCA loader (mivf_cust_load_asset()).
Phase C.1: shares mivf_theme_asset_common.py with the GUI's live preview.

Output format ("MVCA" asset, no mask section -- backgrounds are always
drawn fully opaque, matching every other MIVF sidecar image today):
  bytes 0..3   "MVCA"
  u32 LE       version (=1)
  u16 LE       width  (=320, the real bottom-screen dashboard canvas width --
               see mivf_customization.c's own comment on why this corrects
               an earlier, unverified 400x240 design-phase assumption)
  u16 LE       height (=240)
  w*h u16 LE   RGB565 pixels, row-major, top-to-bottom

Example:
  python tools/mivf_make_dashboard_bg.py bg.png movie.mivfasset --force
  (then reference it from movie.mivftheme as: DASHBOARD_BG=movie)
"""
from __future__ import annotations

import argparse
from pathlib import Path
from PIL import Image

from mivf_theme_asset_common import fit_to_canvas, rgb565le_bytes, mvca_header

W, H = 320, 240


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help="source PNG/JPEG/etc.")
    ap.add_argument("output", help="output .mivfasset")
    ap.add_argument("--bg", default="0,0,0", help="letterbox fill RGB, default 0,0,0")
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
    canvas = fit_to_canvas(image, W, H, bg=tuple(bg_parts), margin=0, opaque=True)
    rgb = canvas.convert("RGB")

    header = mvca_header(W, H)
    pixels = rgb565le_bytes(rgb)
    expected_pixels = W * H * 2
    if len(pixels) != expected_pixels:
        raise SystemExit(f"internal size error: expected {expected_pixels} pixel bytes, got {len(pixels)}")

    out.write_bytes(header + pixels)
    expected_total = len(header) + expected_pixels
    if out.stat().st_size != expected_total:
        raise SystemExit(f"write size error: expected {expected_total}, got {out.stat().st_size}")

    if args.preview:
        rgb.save(args.preview)

    print(f"WROTE {out}")
    print(f"SIZE  {out.stat().st_size} bytes (header 12 + pixels {expected_pixels})")


if __name__ == "__main__":
    main()
