#!/usr/bin/env python3
"""Standalone seek-index (.idx) generator for an already-encoded .mivf file.

Usage:
    python3 tools/mivf_build_idx.py input.mivf output.idx

Why this exists: the player builds this same index automatically at open
time, but skips that scan for files over 256 MiB (HFIX58F_SYNC_INDEX_FAST_
LIMIT_BYTES in source/main.c) to avoid a multi-minute stall the first time
a large movie is opened. Without a sidecar, chapter/scene-selection seeks on
such files fall back to a bounded approximate scan. Pre-generating the real
.idx here (once, on a PC) gives the player the same seek accuracy it would
have built itself for a small file.

This intentionally duplicates the *reading* logic already in
hfix58f_build_seek_index/read_header/read_stream (source/main.c) rather than
calling into the encoder or the player -- it only ever reads bytes already
present in a finished .mivf, and must match that C code's parsing byte for
byte or the player will reject the cache outright (it strictly checks magic/
version/file_size/first_offset before trusting a sidecar). See the comments
below for exactly which C functions each section mirrors.
"""

import struct
import sys
from pathlib import Path

MIVF_HEADER_SIZE = 64
MIVF_PAGE_HEADER_SIZE = 32
MIVF_STREAM_HEADER_SIZE = 64
HFIX58F_MAX_SEEK_POINTS = 4096

# Matches hfix58j_* in source/main.c exactly -- magic/version/record layout.
HFIX58J_IDX_MAGIC = 0x314A4458
HFIX58J_IDX_VERSION = 2

KNOWN_CODECS = (b"MIV", b"M2Y0", b"M2Y1", b"M2Y2", b"RAWV", b"PC16", b"IA4M")


def le16(buf: bytes, off: int) -> int:
    return struct.unpack_from("<H", buf, off)[0]


def le32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def le64(buf: bytes, off: int) -> int:
    return struct.unpack_from("<Q", buf, off)[0]


class Stream:
    __slots__ = ("id", "type", "codec", "w", "h", "fpsn", "fpsd")


def find_first_offset(probe: bytes) -> int:
    """Mirrors read_header's page-anchor search: scan from
    MIVF_HEADER_SIZE in 4-byte steps for a page whose header passes basic
    sanity (payload/packet-count bounds, first packet's hsize/psize fit)."""
    limit = len(probe) - (MIVF_PAGE_HEADER_SIZE + 16)
    off = MIVF_HEADER_SIZE
    while off <= limit:
        if probe[off:off + 2] == b"MP":
            payload = le32(probe, off + 0x10)
            packets = le16(probe, off + 0x14)
            if 0 < payload <= 512 * 1024 and 0 < packets <= 128:
                pkt_off = off + MIVF_PAGE_HEADER_SIZE
                pkt_hsize = le16(probe, pkt_off + 2)
                pkt_psize = le32(probe, pkt_off + 8)
                if pkt_hsize == 16 and pkt_hsize + pkt_psize <= payload:
                    return off
        off += 4
    raise ValueError("no valid MP page header found in the first "
                     f"{len(probe)} bytes (header scan region)")


def infer_streams(probe: bytes, first_offset: int) -> tuple[int, int]:
    """Mirrors read_header's stream count/stride inference: try every
    divisor of the header-to-first-page gap as a candidate stride, score
    candidates whose every slot has a printable+known codec tag, and keep
    the best-scoring layout. Returns (count, stride)."""
    stream_area = first_offset - MIVF_HEADER_SIZE
    best_count = 0
    best_stride = 0
    best_score = -1

    for count in range(1, 17):
        if stream_area % count:
            continue
        stride = stream_area // count
        if stride < 24 or stride > 4096:
            continue

        score = 0
        seen_video = False
        seen_audio = False
        valid = True

        for i in range(count):
            pos = MIVF_HEADER_SIZE + i * stride
            if pos + 24 > first_offset:
                valid = False
                break

            sid = probe[pos + 0]
            stype = probe[pos + 1]
            tag = probe[pos + 4:pos + 8]

            if not all(32 <= c <= 126 for c in tag):
                valid = False
                break

            codec_known = (
                tag[:3] == b"MIV"
                or tag == b"M2Y0" or tag == b"M2Y1" or tag == b"M2Y2"
                or tag == b"RAWV" or tag == b"PC16" or tag == b"IA4M"
            )
            if not codec_known:
                valid = False
                break

            if stype == 1:
                seen_video = True
                score += 100
            elif stype == 2:
                seen_audio = True
                score += 100
            else:
                valid = False
                break

            if sid < 16:
                score += 5
            score += 25  # codec_known, always true here

        if not valid:
            continue

        if seen_video:
            score += 200
        if seen_audio:
            score += 250
        score += 32 - count

        if score > best_score:
            best_score = score
            best_count = count
            best_stride = stride

    if best_count == 0:
        raise ValueError("could not infer stream layout (no valid stride "
                          f"found for area={stream_area} bytes)")
    return best_count, best_stride


def read_streams(probe: bytes, count: int, stride: int) -> list[Stream]:
    """Mirrors read_stream's field offsets within each stride-sized block."""
    streams = []
    for i in range(count):
        pos = MIVF_HEADER_SIZE + i * stride
        s = Stream()
        s.id = probe[pos + 0]
        s.type = probe[pos + 1]
        s.codec = probe[pos + 4:pos + 8].split(b"\x00", 1)[0]
        s.w = le16(probe, pos + 0x10)
        s.h = le16(probe, pos + 0x12)
        s.fpsn = le16(probe, pos + 0x14)
        s.fpsd = le16(probe, pos + 0x16)
        streams.append(s)
    return streams


def is_m2y_delta(body: bytes) -> bool:
    return len(body) >= 13 and body[0:3] == b"M2Y" and body[3] in (0x31, 0x32)


def is_sync_body(body: bytes, psize: int, flags: int, video: Stream) -> bool:
    """Mirrors the inline sync-detection logic in hfix58f_build_seek_index
    (source/main.c) exactly -- see that function's comments for rationale."""
    if body[0:4] == b"M2Y0":
        return True
    if is_m2y_delta(body):
        return len(body) > 12 and body[12] == 1
    if body[0:4] == b"M1P0":
        return True
    if (flags & 1) != 0 and len(body) >= 2 and body[0:1] == b"M" and body[1:2] in (b"1", b"2"):
        return True
    if video.codec == b"RAWV":
        raw_size = video.w * video.h * 2
        if raw_size != 0 and psize == raw_size:
            return True
    return False


def build_index(path: Path) -> tuple[int, int, int, list[tuple[int, int]]]:
    """Returns (file_size, first_offset, total_frames, points) where points
    is a list of (frame, file_offset) tuples, at most HFIX58F_MAX_SEEK_POINTS
    long -- one entry per page that contains a sync packet for the video
    stream, matching hfix58f_build_seek_index's full-scan branch."""
    file_size = path.stat().st_size

    with open(path, "rb") as f:
        probe = f.read(8192)

        if probe[0:4] != b"MIVF":
            raise ValueError("not a MIVF file (missing 'MIVF' magic)")

        first_offset = find_first_offset(probe)
        count, stride = infer_streams(probe, first_offset)
        streams = read_streams(probe, count, stride)

        video = next((s for s in streams if s.type == 1), None)
        if video is None:
            raise ValueError("no video stream (type=1) found in header")

        print(f"header: streams={count} stride={stride} first={first_offset}")
        print(f"video stream: id={video.id} codec={video.codec.decode('ascii', 'replace')} "
              f"{video.w}x{video.h} fps={video.fpsn}/{video.fpsd or 1}")

        points: list[tuple[int, int]] = []
        highest_frame = 0
        pos = first_offset

        f.seek(pos)
        while pos + MIVF_PAGE_HEADER_SIZE < file_size and len(points) < HFIX58F_MAX_SEEK_POINTS:
            f.seek(pos)
            page_hdr = f.read(MIVF_PAGE_HEADER_SIZE)
            if len(page_hdr) < MIVF_PAGE_HEADER_SIZE:
                break

            payload = le32(page_hdr, 0x10)
            packets = le16(page_hdr, 0x14)
            if not (0 < payload <= 512 * 1024) or not (0 < packets <= 128):
                break

            page_payload_start = pos + MIVF_PAGE_HEADER_SIZE
            page_end = page_payload_start + payload
            if page_end > file_size:
                break

            f.seek(page_payload_start)
            page_bytes = f.read(payload)

            pkt_off = 0
            page_has_sync = False
            sync_frame = 0

            for _ in range(packets):
                if pkt_off + 16 > payload:
                    break

                pkt_hdr = page_bytes[pkt_off:pkt_off + 16]
                pkt_stream = pkt_hdr[0]
                flags = pkt_hdr[1]
                hsize = le16(pkt_hdr, 2)
                psize = le32(pkt_hdr, 8)
                frame = le32(pkt_hdr, 12)

                if hsize < 16 or psize > payload or pkt_off + hsize + psize > payload:
                    break

                if pkt_stream != video.id:
                    pkt_off += hsize + psize
                    continue

                if frame > highest_frame:
                    highest_frame = frame

                body = page_bytes[pkt_off + hsize:pkt_off + hsize + min(psize, 16)]
                if is_sync_body(body, psize, flags, video):
                    page_has_sync = True
                    sync_frame = frame
                    break

                pkt_off += hsize + psize

            if page_has_sync:
                points.append((sync_frame, pos))

            pos = page_end

    total_frames = highest_frame + 1
    return file_size, first_offset, total_frames, points


def write_idx(out_path: Path, file_size: int, first_offset: int, total_frames: int,
              points: list[tuple[int, int]]) -> None:
    with open(out_path, "wb") as f:
        f.write(struct.pack("<II", HFIX58J_IDX_MAGIC, HFIX58J_IDX_VERSION))
        f.write(struct.pack("<Q", file_size))
        f.write(struct.pack("<II", first_offset, len(points)))
        # HFIX58FSeekPoint is { u32 frame; u64 file_offset; } with no packing
        # attribute -- on the ARM EABI target this pads to 16 bytes (4-byte
        # frame + 4 bytes padding + 8-byte file_offset), confirmed against
        # the actual devkitARM compiler rather than assumed. "<I4xQ" matches
        # that layout exactly; writing "<IQ" (12 bytes, no padding) would
        # desync every record after the first and the player would read
        # garbage.
        for frame, file_offset in points:
            f.write(struct.pack("<I4xQ", frame, file_offset))


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} input.mivf output.idx", file=sys.stderr)
        return 2

    in_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])

    if not in_path.is_file():
        print(f"error: {in_path} not found", file=sys.stderr)
        return 1

    try:
        file_size, first_offset, total_frames, points = build_index(in_path)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if not points:
        print("error: no sync points found -- refusing to write an empty/useless index", file=sys.stderr)
        return 1

    write_idx(out_path, file_size, first_offset, total_frames, points)

    print(f"wrote {out_path}")
    print(f"  file_size={file_size} first_offset={first_offset} "
          f"total_frames={total_frames} seek_points={len(points)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
