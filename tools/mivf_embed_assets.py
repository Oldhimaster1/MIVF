#!/usr/bin/env python3
"""Pack sidecar assets into a MIVF Asset Bundle (MASB) appended to a .mivf.

Usage:
    python3 tools/mivf_embed_assets.py input.mivf output.mivf \\
        --menu-bg input.menu_bg.cover [--menu input.menu.ini] \\
        [--chapters input.chapters] [--nfo input.nfo] [--idx input.idx]

Always writes a NEW file -- never modifies input.mivf in place. This is
deliberate: these files can be gigabytes, and an in-place rewrite has no
good recovery path if it's interrupted partway through. If input.mivf
already ends in a MASB bundle, that bundle is stripped (not merged) and
replaced with exactly the assets given on this run, appended after the
original movie data.

Phase A: the player (source/main.c, mivf_menu_load_background) only reads
the "menu_bg.cover" key back out so far. The other flags are accepted and
packed now anyway, since the bundle format itself is already general --
later phases just need to teach the player's text-sidecar loaders to also
check the bundle, not change this tool or the format again.

Format (little-endian throughout), matching source/main.c's HFIX74 reader:

  [movie data, byte-for-byte from input.mivf's original movie portion]
  [asset directory header, 16 bytes]
      u32 magic    "MABD"
      u32 version  1
      u32 entry_count
      u32 dir_size (bytes from the start of this header to the first payload)
  [entry_count * 64-byte directory entries]
      char key[32] (NUL-padded ASCII, e.g. "menu_bg.cover")
      u32 type     (informational; see ASSET_TYPES below)
      u32 flags    (reserved, 0)
      u64 offset   (payload offset, relative to the directory header start)
      u64 size     (payload byte length)
      u32 crc32    (zlib.crc32 of the payload; not yet verified by the
                    Phase A reader, but written now so a later phase can
                    start enforcing it without a format version bump)
      u32 reserved
  [payloads, back to back, in entry order]
  [footer, 64 bytes, at absolute EOF]
      u32 magic          "MASB"
      u32 version        1
      u64 bundle_offset  absolute file offset where the directory header starts
      u64 bundle_size    bytes from bundle_offset to the start of this footer
      u32 entry_count
      u32 flags          (reserved, 0)
      u64 movie_size     bundle_offset, restated (lets a tool find where the
                          real movie data ends without re-deriving it)
      u64 reserved0
      u64 reserved1
      u64 reserved2
"""

import argparse
import struct
import sys
import zlib
from pathlib import Path

FOOTER_MAGIC = 0x4253414D    # "MASB"
FOOTER_VERSION = 1
FOOTER_SIZE = 64
DIR_MAGIC = 0x44424D41       # "MABD"
DIR_VERSION = 1
DIR_HEADER_SIZE = 16
ENTRY_SIZE = 64
KEY_MAX = 32
COPY_CHUNK = 4 * 1024 * 1024

# Informational only in Phase A -- the reader matches by key string, not
# type. Numbering matches the asset-key table in the design discussion.
ASSET_TYPES = {
    "menu.ini": 1,
    "chapters": 2,
    "nfo": 3,
    "menu_bg.cover": 4,
    "idx": 5,
}


def existing_movie_size(path: Path) -> int:
    """If `path` already carries a valid MASB footer, return the original
    movie size so re-packing doesn't nest a bundle inside a bundle."""
    size = path.stat().st_size
    if size < FOOTER_SIZE:
        return size

    with open(path, "rb") as f:
        f.seek(size - FOOTER_SIZE)
        footer = f.read(FOOTER_SIZE)

    magic, version = struct.unpack_from("<II", footer, 0)
    if magic != FOOTER_MAGIC or version != FOOTER_VERSION:
        return size

    bundle_offset = struct.unpack_from("<Q", footer, 8)[0]
    bundle_size = struct.unpack_from("<Q", footer, 16)[0]
    if bundle_offset + bundle_size + FOOTER_SIZE != size:
        return size  # inconsistent footer -- don't trust it, treat as unpacked

    return bundle_offset


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", type=Path, help="source .mivf (read-only)")
    ap.add_argument("output", type=Path, help="destination .mivf (overwritten if it exists)")
    ap.add_argument("--menu-bg", type=Path, metavar="PATH", help="raw RGB565 menu_bg.cover")
    ap.add_argument("--menu", type=Path, metavar="PATH", help="menu.ini")
    ap.add_argument("--chapters", type=Path, metavar="PATH", help=".chapters sidecar")
    ap.add_argument("--nfo", type=Path, metavar="PATH", help=".nfo sidecar")
    ap.add_argument("--idx", type=Path, metavar="PATH", help=".idx seek index")
    return ap.parse_args()


def main() -> int:
    args = parse_args()

    flag_to_key = (
        (args.menu, "menu.ini"),
        (args.chapters, "chapters"),
        (args.nfo, "nfo"),
        (args.menu_bg, "menu_bg.cover"),
        (args.idx, "idx"),
    )
    assets: list[tuple[str, Path]] = []
    for path, key in flag_to_key:
        if path is None:
            continue
        if not path.is_file():
            print(f"error: {path} not found", file=sys.stderr)
            return 1
        assets.append((key, path))

    if not assets:
        print("error: no assets given (use --menu-bg/--menu/--chapters/--nfo/--idx)", file=sys.stderr)
        return 1

    if not args.input.is_file():
        print(f"error: {args.input} not found", file=sys.stderr)
        return 1

    if args.output.resolve() == args.input.resolve():
        print("error: output must not be the same file as input (always writes a new file)", file=sys.stderr)
        return 1

    movie_size = existing_movie_size(args.input)

    with open(args.input, "rb") as fin, open(args.output, "wb") as fout:
        remaining = movie_size
        while remaining > 0:
            chunk = fin.read(min(COPY_CHUNK, remaining))
            if not chunk:
                break
            fout.write(chunk)
            remaining -= len(chunk)

        bundle_offset = fout.tell()
        if bundle_offset != movie_size:
            print(f"error: short read copying movie data ({bundle_offset} of {movie_size} bytes)", file=sys.stderr)
            return 1

        payload_cursor = DIR_HEADER_SIZE + len(assets) * ENTRY_SIZE
        entries = []
        payloads = []
        for key, path in assets:
            data = path.read_bytes()
            entries.append((key, len(data), payload_cursor, zlib.crc32(data) & 0xFFFFFFFF))
            payloads.append(data)
            payload_cursor += len(data)

        fout.write(struct.pack("<IIII", DIR_MAGIC, DIR_VERSION, len(assets), payload_cursor))

        for key, size, rel_offset, crc in entries:
            key_bytes = key.encode("ascii")
            if len(key_bytes) > KEY_MAX:
                print(f"error: asset key {key!r} exceeds {KEY_MAX} bytes", file=sys.stderr)
                return 1
            fout.write(key_bytes.ljust(KEY_MAX, b"\x00"))
            fout.write(struct.pack("<IIQQII", ASSET_TYPES.get(key, 0), 0, rel_offset, size, crc, 0))

        for data in payloads:
            fout.write(data)

        bundle_size = fout.tell() - bundle_offset

        footer = struct.pack(
            "<IIQQIIQQQQ",
            FOOTER_MAGIC, FOOTER_VERSION,
            bundle_offset, bundle_size,
            len(assets), 0,
            bundle_offset, 0, 0, 0,
        )
        assert len(footer) == FOOTER_SIZE, f"footer size drifted: {len(footer)} != {FOOTER_SIZE}"
        fout.write(footer)

    print(f"input movie size: {movie_size}")
    print("assets:")
    for key, path in assets:
        print(f"  {key}: {path.stat().st_size} bytes")
    print(f"bundle offset: {bundle_offset}")
    print(f"bundle size: {bundle_size}")
    print(f"output: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
