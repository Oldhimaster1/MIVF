#!/usr/bin/env python3
"""Generate a ".chapthumbs" sidecar of Scene Selection thumbnails.

Usage:
    python3 tools/mivf_build_chapter_thumbs.py source.mkv movie.chapters movie.chapthumbs [--ffmpeg PATH]

Why this exists: real DVD authoring renders scene-selection thumbnails once,
at author time, from the original source -- not on the player at playback
time. This does the same thing: for each chapter timestamp already recorded
in the ".chapters" sidecar (the same file convert_*.sh / encode_mivf.py
already generate), grab one frame directly from the *original* source video
via ffmpeg -ss seek, downscale it, and pack all of them into one small
binary sidecar the player reads as a flat array -- no decode of the
compressed .mivf, and nothing for the 3DS to do at Scene Selection time
except read a file already sized for exactly this many chapters.

Output format (little-endian), matching source/main.c's
mivf_menu_load_chapter_thumbs reader exactly -- see that function's comment
for the authoritative layout:
    bytes 0..3   "MCTH"
    u32          version = 1
    u32          count
    u16          thumb_w  (96)
    u16          thumb_h  (54)
    count * (thumb_w * thumb_h) u16 RGB565LE pixels, one per chapter, in
    chapter order.

The player reader is strict: count must equal the number of chapters loaded
from the same movie's ".chapters" sidecar at menu time, or it discards the
whole file and falls back to a plain text list. That means this tool must be
re-run any time the ".chapters" sidecar changes (chapters added/removed/
reordered), and the two files should always be regenerated together.
"""

import argparse
import re
import struct
import subprocess
import sys
from pathlib import Path

MAGIC = b"MCTH"
VERSION = 1
THUMB_W = 96
THUMB_H = 54


def parse_chapters(path: Path) -> list[tuple[float, str]]:
    chapters = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        if "|" in line:
            ts, label = line.split("|", 1)
        else:
            m = re.match(r"^(\S+)\s+(.*)$", line)
            if not m:
                continue
            ts, label = m.group(1), m.group(2)

        parts = ts.split(":")
        try:
            if len(parts) == 3:
                h, mi, s = parts
                secs = int(h) * 3600 + int(mi) * 60 + float(s)
            elif len(parts) == 2:
                mi, s = parts
                secs = int(mi) * 60 + float(s)
            else:
                secs = float(ts)
        except ValueError:
            continue

        chapters.append((secs, label.strip() or f"Chapter {len(chapters) + 1}"))

    return chapters


def grab_thumbnail(ffmpeg: str, source: Path, timestamp_sec: float) -> bytes:
    # -ss before -i: fast seek (keyframe-nearest), acceptable here since this
    # is a preview thumbnail, not a frame-accurate seek point like the .idx.
    cmd = [
        ffmpeg,
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-ss",
        f"{timestamp_sec:.3f}",
        "-i",
        str(source),
        "-frames:v",
        "1",
        "-vf",
        f"scale={THUMB_W}:{THUMB_H}:force_original_aspect_ratio=increase,crop={THUMB_W}:{THUMB_H},format=rgb565le",
        "-f",
        "rawvideo",
        "pipe:1",
    ]
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    expected = THUMB_W * THUMB_H * 2

    if result.returncode != 0 or len(result.stdout) != expected:
        raise RuntimeError(
            f"ffmpeg failed to grab a thumbnail at {timestamp_sec:.3f}s "
            f"(exit {result.returncode}, got {len(result.stdout)} of {expected} bytes): "
            f"{result.stderr.decode('utf-8', 'replace')}"
        )

    return result.stdout


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", type=Path, help="original source video (not the .mivf)")
    ap.add_argument("chapters", type=Path, help="the movie's .chapters sidecar")
    ap.add_argument("output", type=Path, help="output .chapthumbs sidecar")
    ap.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable (default: 'ffmpeg' on PATH)")
    args = ap.parse_args()

    if not args.source.is_file():
        print(f"error: source not found: {args.source}", file=sys.stderr)
        return 1
    if not args.chapters.is_file():
        print(f"error: chapters sidecar not found: {args.chapters}", file=sys.stderr)
        return 1

    chapters = parse_chapters(args.chapters)
    if not chapters:
        print(f"error: no chapters parsed from {args.chapters}", file=sys.stderr)
        return 1

    print(f"source:    {args.source}")
    print(f"chapters:  {args.chapters} ({len(chapters)} entries)")
    print(f"thumbs:    {THUMB_W}x{THUMB_H}, {len(chapters)} total")

    thumbs = []
    for i, (secs, label) in enumerate(chapters):
        print(f"  [{i + 1}/{len(chapters)}] t={secs:8.2f}s  {label}")
        thumbs.append(grab_thumbnail(args.ffmpeg, args.source, secs))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<IIHH", VERSION, len(chapters), THUMB_W, THUMB_H))
        for data in thumbs:
            f.write(data)

    print(f"wrote {args.output} ({args.output.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
