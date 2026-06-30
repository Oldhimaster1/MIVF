#!/usr/bin/env python3
"""
miv1p_tools_fast2.py

Drop-in launcher for miv1p_tools_fast.py with raw RGB565 frame input support.

Why this exists:
  PPM is faster than PNG, but raw RGB565 is even better because the encoder no
  longer asks PIL to parse RGB888 and convert pixels to RGB565 in Python.

Usage:
  python miv1p_tools_fast2.py images out.mivf frames_rgb565/frame_%04d.rgb565 \
    --codec miv1p --frames 120 --width 400 --height 240 --fps 30 ...

This script imports miv1p_tools_fast.py, overrides its load_images() function,
and then calls its normal main(). Put both files in the same folder.
"""
from __future__ import annotations

import sys
from pathlib import Path

try:
    import miv1p_tools_fast as base
except Exception as e:
    raise SystemExit(f"ERROR: could not import miv1p_tools_fast.py next to this file: {e}")

_orig_load_images = base.load_images


def _load_images_fast2(pattern, start, count, width, height):
    # Direct raw RGB565LE input. One file per frame, exactly width*height*2 bytes.
    if str(pattern).lower().endswith('.rgb565'):
        if not width or not height:
            raise SystemExit('ERROR: .rgb565 input requires --width and --height')
        frame_size = width * height * 2
        paths = [Path(pattern % i) for i in range(start, start + count)] if '%' in pattern else []
        if not paths:
            raise SystemExit('ERROR: .rgb565 input currently expects a printf pattern like frame_%04d.rgb565')
        frames = []
        for p in paths:
            if not p.exists():
                raise SystemExit(f'missing {p}')
            data = p.read_bytes()
            if len(data) != frame_size:
                raise SystemExit(f'bad size for {p}: got {len(data)}, expected {frame_size}')
            frames.append(data)
        return frames, width, height

    # Fallback to original PNG/PPM/PIL loader.
    return _orig_load_images(pattern, start, count, width, height)


base.load_images = _load_images_fast2

if __name__ == '__main__':
    base.main()
