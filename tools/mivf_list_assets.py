#!/usr/bin/env python3
"""List the embedded MIVF Asset Bundle (MASB), if any, in a .mivf file.

Usage:
    python3 tools/mivf_list_assets.py movie.mivf

Reads only the trailing footer and directory -- never touches or loads
movie/video data. Exits 1 (with a message) if the file has no MASB bundle;
that's the normal, expected state for a plain sidecar-only .mivf.
"""

import struct
import sys
from pathlib import Path

FOOTER_MAGIC = 0x4253414D    # "MASB"
FOOTER_VERSION = 1
FOOTER_SIZE = 64
DIR_MAGIC = 0x44424D41       # "MABD"
DIR_VERSION = 1
DIR_HEADER_SIZE = 16
ENTRY_SIZE = 64
KEY_MAX = 32


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} movie.mivf", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    if not path.is_file():
        print(f"error: {path} not found", file=sys.stderr)
        return 1

    size = path.stat().st_size
    if size < FOOTER_SIZE:
        print("no asset bundle (file too small for a footer)")
        return 1

    with open(path, "rb") as f:
        f.seek(size - FOOTER_SIZE)
        footer = f.read(FOOTER_SIZE)

        magic, version = struct.unpack_from("<II", footer, 0)
        if magic != FOOTER_MAGIC or version != FOOTER_VERSION:
            print("no asset bundle found")
            return 1

        bundle_offset, bundle_size = struct.unpack_from("<QQ", footer, 8)
        entry_count = struct.unpack_from("<I", footer, 24)[0]
        movie_size = struct.unpack_from("<Q", footer, 32)[0]

        if bundle_offset >= size or bundle_offset + bundle_size + FOOTER_SIZE != size:
            print("asset bundle footer present but inconsistent (bundle_offset/"
                  "bundle_size don't reconcile with file size) -- treating as absent")
            return 1

        f.seek(bundle_offset)
        dir_hdr = f.read(DIR_HEADER_SIZE)
        dmagic, dversion, dcount, dir_size = struct.unpack("<IIII", dir_hdr)

        if dmagic != DIR_MAGIC or dversion != DIR_VERSION or dcount != entry_count:
            print("asset bundle footer valid but directory header is not -- treating as absent")
            return 1

        print(f"movie size:   {movie_size} bytes")
        print(f"bundle offset:{bundle_offset} bytes")
        print(f"bundle size:  {bundle_size} bytes")
        print(f"entries:      {entry_count}")
        print()

        for _ in range(entry_count):
            entry = f.read(ENTRY_SIZE)
            key = entry[:KEY_MAX].split(b"\x00", 1)[0].decode("ascii", "replace")
            asset_type, flags = struct.unpack_from("<II", entry, 32)
            rel_offset, asset_size = struct.unpack_from("<QQ", entry, 40)
            crc32 = struct.unpack_from("<I", entry, 56)[0]
            print(f"  {key:<20} {asset_size:>10} bytes  type={asset_type} "
                  f"flags={flags} offset={bundle_offset + rel_offset} crc32={crc32:08x}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
