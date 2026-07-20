#!/usr/bin/env python3
"""Create a MIVF Phase C/C.1 customization control-underlay asset.

Supports the three controls approved so far -- fast_forward and play_pause
(Phase C, Cinematic/Premiere dashboard, CUSTOMIZATION_VERTICAL_SLICE_PLAN.md),
and movie_menu_back (Phase C.1, the real root DVD-style menu's Back row,
MIVF_MENU_ACTION_BACK -- see BACK_RENDER_GEOMETRY.md). Each uses its real,
source-derived rectangle: the two transport controls from
g_mivf_touch_layouts[MIVF_TRANSPORT_STYLE_CINEMATIC] (source/main.c:2332),
movie_menu_back from mivf_menu_draw_button_top()'s own ROW_W/ROW_H constants
(source/main.c:13416) -- 222x20, a real rendered-rectangle constant, not a
guess. Shares mivf_theme_asset_common.py with the GUI's live preview,
including the exact same alpha-threshold mask logic -- what the GUI shows
you is what this tool will actually write.

Output format ("MVCA" asset, WITH a 1bpp mask -- see
CUSTOMIZATION_ASSET_FORMAT_DECISION.md for why RGB565+1bpp mask was chosen
over RGBA5551/color-key/IA4 for this slice):
  bytes 0..3   "MVCA"
  u32 LE       version (=1)
  u16 LE       width
  u16 LE       height
  w*h u16 LE   RGB565 pixels, row-major, top-to-bottom
  ceil(w*h/8)  1bpp mask, row-major, MSB-first, 1=opaque/draw, 0=transparent

The mask is derived from the source image's own alpha channel, thresholded
at 128 (documented, not a soft/anti-aliased edge -- matches the format
decision's explicit "hard 1-bit transparency only" scope limit).

Example:
  python tools/mivf_make_control_asset.py ff_glow.png movie_ff.mivfasset --control fast_forward --force
  (then reference it from movie.mivftheme as: CONTROL.UNDERLAY=movie_ff)
"""
from __future__ import annotations

import argparse
from pathlib import Path
from PIL import Image

from mivf_theme_asset_common import fit_to_canvas, encode_control_asset, mvca_header, DEFAULT_ALPHA_THRESHOLD

# Real hitbox caps, main.c:2332, CINEMATIC style, main[2]=Forward, main[1]=Play/Pause.
# movie_menu_back: main.c:13416's ROW_W/ROW_H (the root DVD-menu's real
# rendered row rectangle -- see BACK_RENDER_GEOMETRY.md for the derivation).
DIMS = {
    "fast_forward": (64, 60),
    "play_pause": (74, 78),
    "movie_menu_back": (222, 20),
    "rewind": (64, 60),  # main.c:2332, g_mivf_touch_layouts[CINEMATIC].main[0], same size as fast_forward
}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help="source PNG with alpha channel")
    ap.add_argument("output", help="output .mivfasset")
    ap.add_argument("--control", required=True, choices=sorted(DIMS.keys()))
    ap.add_argument("--preview", help="optional PNG preview output (RGBA, shows the thresholded mask)")
    ap.add_argument("--force", action="store_true", help="overwrite output")
    args = ap.parse_args()

    w, h = DIMS[args.control]
    src = Path(args.input)
    out = Path(args.output)
    if out.exists() and not args.force:
        raise SystemExit(f"refusing to overwrite {out}; pass --force")

    image = Image.open(src)
    canvas = fit_to_canvas(image, w, h, bg=(0, 0, 0), margin=0, opaque=False)

    header = mvca_header(w, h)
    pixels, mask = encode_control_asset(canvas, threshold=DEFAULT_ALPHA_THRESHOLD)

    expected_px = w * h * 2
    expected_mask = (w * h + 7) // 8
    if len(pixels) != expected_px or len(mask) != expected_mask:
        raise SystemExit(
            f"internal size error: pixels {len(pixels)}/{expected_px}, mask {len(mask)}/{expected_mask}"
        )

    out.write_bytes(header + pixels + mask)
    expected_total = len(header) + expected_px + expected_mask
    if out.stat().st_size != expected_total:
        raise SystemExit(f"write size error: expected {expected_total}, got {out.stat().st_size}")

    if args.preview:
        from mivf_theme_asset_common import masked_preview_rgba
        masked_preview_rgba(canvas, threshold=DEFAULT_ALPHA_THRESHOLD).save(args.preview)

    print(f"WROTE {out}")
    print(f"SIZE  {out.stat().st_size} bytes (header 12 + pixels {expected_px} + mask {expected_mask})")


if __name__ == "__main__":
    main()
