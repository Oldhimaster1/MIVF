#!/usr/bin/env python3
import argparse
import struct
from pathlib import Path

HEADER_SIZE = 64
PAGE_HEADER_SIZE = 32

def le16(b, o):
    return b[o] | (b[o + 1] << 8)

def le32(b, o):
    return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)

def le64(b, o):
    return le32(b, o) | (le32(b, o + 4) << 32)

def read_exact(f, n, label):
    b = f.read(n)
    if len(b) != n:
        raise SystemExit(f"short read {label}: wanted {n}, got {len(b)}")
    return b

def inspect(path):
    with open(path, "rb") as f:
        header = read_exact(f, HEADER_SIZE, f"{path} header")

        if header[:4] != b"MIVF":
            raise SystemExit(f"{path}: not MIVF")

        streams = le32(header, 12)
        first = le64(header, 36)

        if streams != 1:
            raise SystemExit(f"{path}: expected 1 stream video-only MIVF, got {streams}")

        if first <= HEADER_SIZE or first > 4096:
            raise SystemExit(f"{path}: invalid first page offset {first}")

        desc = read_exact(f, first - HEADER_SIZE, f"{path} descriptor")
        fpsn = le16(desc, 20) or 30
        fpsd = le16(desc, 22) or 1

        return header, desc, first, fpsn, fpsd

def copy_pages(path, out_f, expected_frame, first):
    copied = 0

    with open(path, "rb") as f:
        f.seek(first)

        while True:
            ph = f.read(PAGE_HEADER_SIZE)

            if not ph:
                break

            if len(ph) != PAGE_HEADER_SIZE:
                raise SystemExit(f"{path}: short page header")

            if ph[:2] != b"MP":
                raise SystemExit(f"{path}: bad page magic")

            seq = le32(ph, 4)
            payload_size = le32(ph, 16)

            if seq != expected_frame:
                raise SystemExit(
                    f"{path}: page sequence mismatch: expected {expected_frame}, got {seq}. "
                    "Check segment frame count and --start-frame."
                )

            payload = read_exact(f, payload_size, f"{path} payload")

            out_f.write(ph)
            out_f.write(payload)

            copied += 1
            expected_frame += 1

    return copied, expected_frame

def merge(output, segments):
    segments = [Path(x) for x in segments]

    if not segments:
        raise SystemExit("no segments provided")

    h0, d0, first0, fpsn, fpsd = inspect(segments[0])

    header = bytearray(h0)
    struct.pack_into("<Q", header, 20, 0)
    struct.pack_into("<Q", header, 36, first0)

    total = 0
    expected = 0

    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)

    with open(output, "wb") as out:
        out.write(header)
        out.write(d0)

        for i, seg in enumerate(segments):
            h, d, first, sfpsn, sfpsd = inspect(seg)

            if first != first0:
                raise SystemExit(f"{seg}: first offset differs from segment 0")

            if d != d0:
                raise SystemExit(f"{seg}: descriptor differs from segment 0")

            if sfpsn != fpsn or sfpsd != fpsd:
                raise SystemExit(f"{seg}: FPS differs from segment 0")

            n, expected = copy_pages(seg, out, expected, first)
            total += n
            print(f"merged segment {i}: {seg} frames={n}", flush=True)

        duration = total * 30000 // fpsn
        out.seek(20)
        out.write(struct.pack("<Q", duration))

    print(f"WROTE {output}")
    print(f"segments={len(segments)} frames={total} fps={fpsn}/{fpsd} duration={duration} bytes={output.stat().st_size}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", required=True)
    ap.add_argument("segments", nargs="+")
    args = ap.parse_args()
    merge(args.output, args.segments)

if __name__ == "__main__":
    main()
