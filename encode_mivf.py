#!/usr/bin/env python3
from __future__ import annotations

import argparse
import contextlib
from concurrent.futures import FIRST_COMPLETED, Future, ThreadPoolExecutor, wait
from dataclasses import dataclass, replace
from multiprocessing import cpu_count
from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib


VIDEO_EXTS = {".mp4", ".mkv", ".mov", ".webm", ".avi"}
DEFAULT_WIDTH = 400
DEFAULT_HEIGHT = 240
DEFAULT_FPS = 30
DEFAULT_AUDIO_RATE = 44100
DEFAULT_AUDIO_CHANNELS = 1
DEFAULT_KEYINT = 240
# Quality-favoring defaults: keep-16 transform (all coefficients) plus an RDO
# tuned to actually spend bits on detail. Pass --keep 4 for small legacy files.
DEFAULT_KEEP = 16
DEFAULT_QP = 34
DEFAULT_C_QP_OFFSET = 5
DEFAULT_LAMBDA = 20.0
DEFAULT_Y_SKIP = 20
DEFAULT_C_SKIP = 24
DEFAULT_Y_DELTA = 24
DEFAULT_C_DELTA = 32
DEFAULT_MV_RANGE = 4
DEFAULT_JOBS = min(8, max(1, cpu_count()))
DEFAULT_CHUNK_FRAMES = 240

# --max-video-packet-kb retry ladder: bounded, deterministic, disabled unless the
# flag is passed (max_video_packet_kb == 0).
PACKET_CAP_MAX_RETRIES = 3
PACKET_CAP_QP_STEP = 6
PACKET_CAP_MAX_QP = 51

HEADER_SIZE = 64
STREAM_DESC_SIZE = 32
PAGE_HEADER_SIZE = 32
PACKET_HEADER_SIZE = 16
PAGE_CRC = 1
PAGE_HAS_KEYFRAME = 2

HFIX58F_MAX_SEEK_POINTS = 4096
HFIX58J_IDX_MAGIC = 0x314A4458
HFIX58J_IDX_VERSION = 2
MIVF_EMBED_IDX_FOOTER_MAGIC = 0x5844494D
MIVF_EMBED_IDX_FOOTER_VERSION = 1
MIVF_EMBED_IDX_FOOTER_SIZE = 32

IMA_INDEX_TABLE = [
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
]

IMA_STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767,
]

# Native helper's per-segment STATS line field names, exactly as emitted by
# tools/miv2y_moflex_tier.c's print_stats(). Used only to parse/aggregate the
# helper's own stdout across chunks for the Patch 0 encode summary -- never
# affects encoder decisions or the bitstream.
HELPER_STATS_INT_FIELDS = [
    "I", "P",
    "Y_skip", "Y_delta", "Y_solid", "Y_mv", "Y_mvqres", "Y_qres", "Y_tr", "Y_mvtr",
    "Y_qz", "Y_mvqz", "Y_trz", "Y_mvtrz", "Y_raw", "Y_run_tokens", "Y_run_blocks",
    "C_skip", "C_delta", "C_solid", "C_mv", "C_mvqres", "C_qres", "C_tr", "C_mvtr",
    "C_qz", "C_mvqz", "C_trz", "C_mvtrz", "C_raw", "C_run_tokens", "C_run_blocks",
    "frames", "total_bytes",
]


def parse_helper_stats_line(stdout_text: str) -> dict[str, int] | None:
    """Parse the native helper's `STATS ...` line (tools/miv2y_moflex_tier.c
    print_stats(), already printed on every successful run) into a dict of
    ints. Returns None if no STATS line is present. Measurement-only: just
    reads back stdout the helper already produces."""
    for line in stdout_text.splitlines():
        if not line.startswith("STATS "):
            continue
        fields: dict[str, int] = {}
        for tok in line[len("STATS "):].split():
            key, sep, val = tok.partition("=")
            if not sep or key not in HELPER_STATS_INT_FIELDS:
                continue
            try:
                fields[key] = int(val)
            except ValueError:
                pass
        return fields or None
    return None


def parse_helper_psnr_line(stdout_text: str) -> dict[str, float] | None:
    """Parse the native helper's `PSNR Y=.. Cb=.. Cr=.. combined=.. dB` line.
    This is already computed unconditionally per segment by the helper (see
    the psnr_se_y/psnr_np_y accumulation in miv2y_moflex_tier.c) -- this just
    reads it back."""
    for line in stdout_text.splitlines():
        if not line.startswith("PSNR "):
            continue
        fields: dict[str, float] = {}
        for tok in line[len("PSNR "):].split():
            key, sep, val = tok.partition("=")
            if not sep:
                continue
            try:
                fields[key] = float(val)
            except ValueError:
                pass
        return fields or None
    return None


def accumulate_helper_stats(agg: dict[str, int], parsed: dict[str, int]) -> None:
    """Sum one segment's parsed STATS into the running cross-segment total."""
    for key, val in parsed.items():
        agg[key] = agg.get(key, 0) + val


def accumulate_helper_psnr(
    agg: dict[str, dict[str, float]], parsed: dict[str, float], weight_frames: int
) -> None:
    """Frame-count-weighted running average of each segment's PSNR (dB).
    Averaging dB directly (rather than inverting to MSE first) is exact only
    when segments have equal frame counts -- true for every chunk except
    possibly the last -- but is precise enough as an A/B signal between
    motion-search modes, and keeps this measurement-only patch simple."""
    if weight_frames <= 0:
        return
    for key, db in parsed.items():
        bucket = agg.setdefault(key, {"weighted_sum": 0.0, "weight": 0})
        bucket["weighted_sum"] += db * weight_frames
        bucket["weight"] += weight_frames


def finalize_helper_psnr(agg: dict[str, dict[str, float]]) -> dict[str, float]:
    out: dict[str, float] = {}
    for key, bucket in agg.items():
        if bucket["weight"] > 0:
            out[key] = bucket["weighted_sum"] / bucket["weight"]
    return out


@dataclass
class EncodeSettings:
    width: int = DEFAULT_WIDTH
    height: int = DEFAULT_HEIGHT
    fps: int = DEFAULT_FPS
    audio_rate: int = DEFAULT_AUDIO_RATE
    audio_channels: int = DEFAULT_AUDIO_CHANNELS
    audio_codec: str = "ia4m"
    audio_offset_ms: int = 0
    keyint: int = DEFAULT_KEYINT
    qp: int = DEFAULT_QP
    c_qp_offset: int = DEFAULT_C_QP_OFFSET
    lambda_value: float = DEFAULT_LAMBDA
    y_skip: int = DEFAULT_Y_SKIP
    c_skip: int = DEFAULT_C_SKIP
    y_delta: int = DEFAULT_Y_DELTA
    c_delta: int = DEFAULT_C_DELTA
    mv_range: int = DEFAULT_MV_RANGE
    keep: int = DEFAULT_KEEP
    jobs: int = DEFAULT_JOBS
    chunk_frames: int = DEFAULT_CHUNK_FRAMES
    max_video_packet_kb: int = 0  # 0 = disabled (default, unchanged behavior)
    warm_start_chunks: bool = False  # experimental, opt-in (see --warm-start-chunks)
    motion_search: str = "full"  # full (default, unchanged) | diamond | fast | hybrid (all experimental)


@dataclass
class SeekIndexData:
    file_size: int
    first_offset: int
    total_frames: int
    seek_points: list[tuple[int, int]]


def resource_dir() -> Path:
    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        return Path(sys._MEIPASS)
    return Path(__file__).resolve().parent


def bundled_path(name: str) -> Path:
    path = resource_dir() / name
    if not path.exists():
        raise FileNotFoundError(f"Missing required file: {path}")
    return path


@contextlib.contextmanager
def pushd(path: Path):
    previous = Path.cwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(previous)


def le16(data: bytes, offset: int) -> int:
    return data[offset] | (data[offset + 1] << 8)


def le32(data: bytes, offset: int) -> int:
    return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24)


def le64(data: bytes, offset: int) -> int:
    return le32(data, offset) | (le32(data, offset + 4) << 32)


def wr_u32le(buf: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", buf, offset, value)


def wr_u64le(buf: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<Q", buf, offset, value)


def make_idx_sidecar_path(mivf_path: Path) -> Path:
    return mivf_path.with_suffix(".idx")


def body_is_m2y_delta_codec(body: bytes) -> bool:
    return (
        len(body) >= 13
        and body[0:3] == b"M2Y"
        and body[3] in (ord("1"), ord("2"))
    )


def body_is_m2y_keyframe(body: bytes) -> bool:
    return body_is_m2y_delta_codec(body) and body[12] == 1


def packet_body_is_sync_video(body: bytes, codec: bytes, width: int, height: int, flags: int, psize: int) -> bool:
    if len(body) < 4:
        return False

    if body[0:4] == b"M2Y0":
        return True

    if body_is_m2y_delta_codec(body):
        return body_is_m2y_keyframe(body)

    if body[0:4] == b"M1P0":
        return True

    if (not body_is_m2y_delta_codec(body)) and (flags & 1) and body[0] == ord("M") and body[1] in (ord("1"), ord("2")):
        return True

    if codec == b"RAWV":
        raw_size = width * height * 2
        if raw_size > 0 and psize == raw_size:
            return True

    return False


def parse_video_stream_desc(desc_blob: bytes) -> tuple[int, bytes, int, int]:
    pos = 0
    while pos + STREAM_DESC_SIZE <= len(desc_blob):
        dsize = le16(desc_blob, pos + 2)
        if dsize < STREAM_DESC_SIZE or pos + dsize > len(desc_blob):
            break

        sid = desc_blob[pos + 0]
        stype = desc_blob[pos + 1]
        codec = bytes(desc_blob[pos + 4:pos + 8])
        width = le16(desc_blob, pos + 16)
        height = le16(desc_blob, pos + 18)

        if stype == 1:
            return sid, codec, width, height

        pos += dsize

    raise RuntimeError("video stream descriptor not found")


def parse_video_packet_body(body: bytes) -> dict | None:
    """Best-effort decode of the fixed 28-byte metadata header every M2Y1/M2Y2
    video packet body already carries (see tools/miv2y_moflex_tier.c's
    write_page() call site and tools/m2y2_transcode.c's byte-for-byte-copied
    body layout comment). Read-only diagnostic over metadata the encoder
    already writes -- does not require or touch the native encoder.

    Returns None if the body is too short or doesn't match a known codec tag.
    """
    if len(body) < 28 or bytes(body[:4]) not in (b"M2Y1", b"M2Y2"):
        return None

    codec = bytes(body[:4]).decode("ascii")
    frame_idx = le32(body, 8)
    is_keyframe = body[12] == 1
    y_size = le32(body, 16)
    cb_size = le32(body, 20)
    cr_size = le32(body, 24)

    info = {
        "codec": codec,
        "frame_idx": frame_idx,
        "is_keyframe": is_keyframe,
        "y_size": y_size,
        "cb_size": cb_size,
        "cr_size": cr_size,
        "nkeep": None,
        "plane_qp": None,
    }

    if codec == "M2Y1":
        # body[13] is only meaningful for M2Y1; M2Y2's transcoder copies it as 0.
        info["nkeep"] = body[13]
        # encode_plane() writes each plane's active QP as that plane payload's
        # own byte 0 ("Plane payload byte 0 stores active transform QP" in
        # tools/miv2y_moflex_tier.c) -- readable directly since M2Y1 plane
        # payloads are uncompressed.
        plane_off = 28
        qps = []
        for size in (y_size, cb_size, cr_size):
            if size > 0 and plane_off < len(body):
                qps.append(body[plane_off])
            plane_off += size
        info["plane_qp"] = qps
    # M2Y2 plane payloads are range-coded (source/mivf_rc.h); the per-plane QP
    # byte is inside the compressed blob and isn't readable without decompressing.

    return info


def report_packet_size_stats(mivf_path: Path, chunk_frames: int | None = None) -> dict:
    """Parse final .mivf and print per-video-packet size statistics.

    Returns a dict with all computed values for programmatic use.
    """
    file_size = mivf_path.stat().st_size

    with mivf_path.open("rb") as f:
        header = f.read(HEADER_SIZE)
        if len(header) != HEADER_SIZE or header[:4] != b"MIVF":
            raise RuntimeError(f"{mivf_path}: not a valid MIVF file")

        first = le64(header, 36)
        if first <= HEADER_SIZE or first > 4096:
            raise RuntimeError(f"{mivf_path}: invalid first page offset {first}")

        desc_blob = f.read(first - HEADER_SIZE)
        if len(desc_blob) != first - HEADER_SIZE:
            raise RuntimeError(f"{mivf_path}: short stream descriptor block")

        video_sid, video_codec, video_w, video_h = parse_video_stream_desc(desc_blob)

        sizes: list[int] = []
        pos = first
        pages = 0

        # Track the single largest video packet's diagnostic detail as we go
        # (in original file order) instead of relying on `sizes` post-sort,
        # since sorting below discards which page/frame each size came from.
        worst_size = -1
        worst_meta: dict | None = None
        worst_page_seq: int | None = None

        while pos + PAGE_HEADER_SIZE <= file_size:
            f.seek(pos)
            page_hdr = f.read(PAGE_HEADER_SIZE)
            if len(page_hdr) != PAGE_HEADER_SIZE:
                break

            if page_hdr[:2] != b"MP":
                break

            page_seq = le32(page_hdr, 4)
            payload = le32(page_hdr, 16)
            packets = le16(page_hdr, 20)

            if payload == 0 or payload > (512 * 1024) or packets == 0 or packets > 128:
                break

            page_payload_start = pos + PAGE_HEADER_SIZE
            page_end = page_payload_start + payload
            if page_end > file_size:
                break

            pkt_pos = page_payload_start

            for _ in range(packets):
                if pkt_pos + PACKET_HEADER_SIZE > page_end:
                    break

                f.seek(pkt_pos)
                pkt_hdr = f.read(PACKET_HEADER_SIZE)
                if len(pkt_hdr) != PACKET_HEADER_SIZE:
                    break

                pkt_sid = pkt_hdr[0]
                hsize = le16(pkt_hdr, 2)
                psize = le32(pkt_hdr, 8)

                if hsize < PACKET_HEADER_SIZE or pkt_pos + hsize + psize > page_end:
                    break

                if pkt_sid == video_sid:
                    sizes.append(psize)
                    if psize > worst_size:
                        body_start = pkt_pos + hsize
                        body_head = b""
                        if body_start + 28 <= page_end:
                            f.seek(body_start)
                            body_head = f.read(min(28, psize))
                        worst_size = psize
                        worst_meta = parse_video_packet_body(body_head)
                        worst_page_seq = page_seq

                pkt_pos += hsize + psize

            pages += 1
            pos = page_end

    if not sizes:
        print(f"PACKET REPORT: no video packets found in {mivf_path}")
        return {"count": 0}

    sizes.sort()
    n = len(sizes)
    total = sum(sizes)
    avg = total / n

    def percentile(pct: float) -> int:
        idx = int(n * pct / 100.0)
        if idx >= n:
            idx = n - 1
        return sizes[idx]

    buckets = [
        (0, 2048),
        (2048, 5120),
        (5120, 10240),
        (10240, 15360),
        (15360, 20480),
        (20480, 30720),
        (30720, 40960),
        (40960, 999999),
    ]
    bucket_labels = [
        "0-2 KB",
        "2-5 KB",
        "5-10 KB",
        "10-15 KB",
        "15-20 KB",
        "20-30 KB",
        "30-40 KB",
        "40 KB+",
    ]
    bucket_counts: list[int] = []
    for lo, hi in buckets:
        bucket_counts.append(sum(1 for s in sizes if lo <= s < hi))

    thresholds = [10240, 15360, 20480, 30720, 40960]

    print()
    print("============================================================")
    print("VIDEO PACKET SIZE REPORT")
    print("============================================================")
    print(f"  file:           {mivf_path}")
    print(f"  video codec:    {video_codec.decode('ascii','replace')}")
    print(f"  video packets:  {n}")
    print(f"  pages scanned:  {pages}")
    print(f"  min:            {sizes[0]:>6d} B  ({sizes[0]/1024:.1f} KB)")
    print(f"  max:            {sizes[-1]:>6d} B  ({sizes[-1]/1024:.1f} KB)")
    print(f"  avg:            {avg:>6.0f} B  ({avg/1024:.1f} KB)")
    print(f"  p50 (median):   {percentile(50):>6d} B  ({percentile(50)/1024:.1f} KB)")
    print(f"  p90:            {percentile(90):>6d} B  ({percentile(90)/1024:.1f} KB)")
    print(f"  p95:            {percentile(95):>6d} B  ({percentile(95)/1024:.1f} KB)")
    print(f"  p99:            {percentile(99):>6d} B  ({percentile(99)/1024:.1f} KB)")
    print()
    print("  Histogram:")
    max_count = max(bucket_counts) if bucket_counts else 1
    for label, count in zip(bucket_labels, bucket_counts):
        pct = 100.0 * count / n
        bar = "#" * max(1, int(40 * count / max_count))
        print(f"    {label:>10s}: {count:5d} ({pct:5.1f}%)  {bar}")
    print()
    over_warned = False
    for thresh in thresholds:
        over = sum(1 for s in sizes if s >= thresh)
        pct = 100.0 * over / n
        tag = ""
        if thresh <= 15360 and pct > 10.0:
            tag = "  *** WARNING: many large packets may cause decode lag on 3DS"
            over_warned = True
        print(f"    >= {thresh//1024:2d} KB:        {over:5d} ({pct:5.1f}%){tag}")
    if over_warned:
        print()
        print("  Recommendation: consider --profile 3ds-fast or lower --qp / higher --lambda")
        print("  to keep video packet sizes below ~10-15 KB for smooth 3DS playback.")

    print()
    print("  Worst packet detail:")
    if worst_meta is not None:
        print(f"    frame index (from body): {worst_meta['frame_idx']}")
        if worst_page_seq is not None and worst_page_seq != worst_meta["frame_idx"]:
            print(f"    *** page seq ({worst_page_seq}) != body frame_idx ({worst_meta['frame_idx']}) -- investigate")
        print(f"    packet size:              {worst_size} B ({worst_size / 1024:.1f} KB)")
        print(f"    codec:                    {worst_meta['codec']}")
        print(f"    keyframe (body flag):     {'YES' if worst_meta['is_keyframe'] else 'no'}")
        print(
            f"    y/cb/cr payload split:    {worst_meta['y_size']} / {worst_meta['cb_size']} / "
            f"{worst_meta['cr_size']} B"
        )
        if worst_meta["nkeep"] is not None:
            print(f"    keep (nkeep) at encode:   {worst_meta['nkeep']}")
        if worst_meta["plane_qp"]:
            print(f"    per-plane QP byte:        {worst_meta['plane_qp']} (Y, Cb, Cr)")
        elif worst_meta["codec"] == "M2Y2":
            print("    per-plane QP byte:        n/a (range-coded; not directly readable without decompressing)")
        if chunk_frames:
            boundary = (worst_meta["frame_idx"] % chunk_frames) == 0
            print(
                f"    chunk_frames setting:     {chunk_frames} "
                f"(frame at chunk/segment boundary: {'YES' if boundary else 'no'})"
            )
        print(
            "    note: the page-header KEYFRAME flag is always set by this encoder (not a "
            "reliable signal) -- frame type above comes from the packet BODY flag instead."
        )
    else:
        print("    unavailable -- packet body did not match a known M2Y1/M2Y2 layout")

    print()
    print(f"  total video payload: {total/1048576:.1f} MiB")
    print("============================================================")

    return {
        "count": n,
        "min": sizes[0],
        "max": sizes[-1],
        "avg": avg,
        "p50": percentile(50),
        "p90": percentile(90),
        "p95": percentile(95),
        "p99": percentile(99),
        "bucket_counts": bucket_counts,
        "bucket_labels": bucket_labels,
        "worst_meta": worst_meta,
        "worst_page_seq": worst_page_seq,
    }


def collect_seek_index_data(mivf_path: Path) -> SeekIndexData:
    file_size = mivf_path.stat().st_size

    with mivf_path.open("rb") as f:
        header = f.read(HEADER_SIZE)
        if len(header) != HEADER_SIZE or header[:4] != b"MIVF":
            raise RuntimeError(f"{mivf_path}: not a valid MIVF file")

        first = le64(header, 36)
        if first <= HEADER_SIZE or first > 4096:
            raise RuntimeError(f"{mivf_path}: invalid first page offset {first}")

        desc_blob = f.read(first - HEADER_SIZE)
        if len(desc_blob) != first - HEADER_SIZE:
            raise RuntimeError(f"{mivf_path}: short stream descriptor block")

        video_sid, video_codec, video_w, video_h = parse_video_stream_desc(desc_blob)

        seek_points: list[tuple[int, int]] = []
        highest_frame = 0
        pos = first

        while pos + PAGE_HEADER_SIZE <= file_size and len(seek_points) < HFIX58F_MAX_SEEK_POINTS:
            f.seek(pos)
            page_hdr = f.read(PAGE_HEADER_SIZE)
            if len(page_hdr) != PAGE_HEADER_SIZE:
                break

            if page_hdr[:2] != b"MP":
                break

            payload = le32(page_hdr, 16)
            packets = le16(page_hdr, 20)

            if payload == 0 or payload > (512 * 1024) or packets == 0 or packets > 128:
                break

            page_payload_start = pos + PAGE_HEADER_SIZE
            page_end = page_payload_start + payload
            if page_end > file_size:
                break

            pkt_pos = page_payload_start
            page_sync_frame: int | None = None

            for _ in range(packets):
                if pkt_pos + PACKET_HEADER_SIZE > page_end:
                    break

                f.seek(pkt_pos)
                pkt_hdr = f.read(PACKET_HEADER_SIZE)
                if len(pkt_hdr) != PACKET_HEADER_SIZE:
                    break

                pkt_sid = pkt_hdr[0]
                pkt_flags = pkt_hdr[1]
                hsize = le16(pkt_hdr, 2)
                psize = le32(pkt_hdr, 8)
                frame = le32(pkt_hdr, 12)

                if hsize < PACKET_HEADER_SIZE or pkt_pos + hsize + psize > page_end:
                    break

                if pkt_sid == video_sid:
                    if frame > highest_frame:
                        highest_frame = frame

                    probe_len = min(psize, 16)
                    body = b""
                    if probe_len > 0:
                        f.seek(pkt_pos + hsize)
                        body = f.read(probe_len)

                    if packet_body_is_sync_video(body, video_codec, video_w, video_h, pkt_flags, psize):
                        page_sync_frame = frame
                        break

                pkt_pos += hsize + psize

            if page_sync_frame is not None:
                seek_points.append((page_sync_frame, pos))

            pos = page_end

    total_frames = highest_frame + 1
    return SeekIndexData(
        file_size=file_size,
        first_offset=first,
        total_frames=total_frames,
        seek_points=seek_points,
    )


def build_hfix58j_payload(
    file_size: int,
    first_offset: int,
    total_frames: int,
    seek_points: list[tuple[int, int]],
    include_total_frames_field: bool = False,
) -> bytes:
    """Builds the on-disk seek-index payload.

    IMPORTANT -- there are currently two different C-side readers for this
    payload in source/main.c, and as of this writing they expect two
    different header layouts. Check both again before changing this:

    - Sidecar .idx files are read by hfix58j_try_load_seek_cache, which
      expects a 24-byte header with NO total_frames field: magic, version,
      file_size, first_offset, count, then records (total_frames is
      recomputed by that reader from the max frame seen in the records).
      Use include_total_frames_field=False (the default) for sidecar
      output -- this was the actual bug: this builder used to always
      include total_frames, so the reader read total_frames where it
      expected count and rejected every generated sidecar.

    - The embedded MIDX footer is read by a *different* function,
      hfix58f_try_load_embedded_index, which has NOT been updated and
      still expects the OLD 28-byte header WITH total_frames before
      count. Use include_total_frames_field=True there so the embedded
      path keeps working exactly as it does today. Unifying the two
      readers onto one format is a separate, player-code change.
    """
    payload = bytearray()
    payload += struct.pack("<I", HFIX58J_IDX_MAGIC)
    payload += struct.pack("<I", HFIX58J_IDX_VERSION)
    payload += struct.pack("<Q", file_size)
    payload += struct.pack("<I", first_offset)
    if include_total_frames_field:
        payload += struct.pack("<I", total_frames)
    payload += struct.pack("<I", len(seek_points))

    # Record layout must match player's Hfix58FSeekPoint ABI:
    # u32 frame + 4-byte padding + u64 file_offset = 16 bytes.
    for frame, file_offset in seek_points:
        payload += struct.pack("<IIQ", frame, 0, file_offset)

    return bytes(payload)


def write_seek_index_sidecar_payload(
    mivf_path: Path,
    sidecar_path: Path,
    payload: bytes,
    seek_points: list[tuple[int, int]],
    total_frames: int,
    file_size: int,
    first_offset: int,
) -> Path:
    with sidecar_path.open("wb") as idx:
        idx.write(payload)

    print(
        "SEEK INDEX SIDECAR: "
        f"path={sidecar_path} points={len(seek_points)} total_frames={total_frames} "
        f"file_size={file_size} first={first_offset} source={mivf_path}"
    )
    return sidecar_path


def append_embedded_seek_index(mivf_path: Path, seek_data: SeekIndexData) -> dict[str, int] | None:
    if not seek_data.seek_points:
        print(f"SEEK INDEX: no sync points found for {mivf_path}; skipping embedded footer")
        return None

    base_size = mivf_path.stat().st_size

    # Payload length is independent of file_size field, so compute final size in 2 passes.
    # include_total_frames_field=True: the embedded-footer reader
    # (hfix58f_try_load_embedded_index) still expects the old 28-byte
    # header -- see build_hfix58j_payload's docstring.
    payload_probe = build_hfix58j_payload(
        base_size, seek_data.first_offset, seek_data.total_frames, seek_data.seek_points,
        include_total_frames_field=True,
    )
    final_size = base_size + len(payload_probe) + MIVF_EMBED_IDX_FOOTER_SIZE
    payload = build_hfix58j_payload(
        final_size, seek_data.first_offset, seek_data.total_frames, seek_data.seek_points,
        include_total_frames_field=True,
    )

    index_offset = base_size
    index_size = len(payload)
    footer = struct.pack(
        "<IIQIIII",
        MIVF_EMBED_IDX_FOOTER_MAGIC,
        MIVF_EMBED_IDX_FOOTER_VERSION,
        index_offset,
        index_size,
        HFIX58J_IDX_MAGIC,
        HFIX58J_IDX_VERSION,
        0,
    )

    if len(footer) != MIVF_EMBED_IDX_FOOTER_SIZE:
        raise RuntimeError(f"embedded footer size mismatch: {len(footer)}")

    with mivf_path.open("ab") as out:
        out.write(payload)
        out.write(footer)

    print(
        "SEEK INDEX EMBEDDED: "
        f"offset={index_offset} size={index_size} points={len(seek_data.seek_points)} "
        f"final_file_size={final_size} path={mivf_path}"
    )

    return {
        "index_offset": index_offset,
        "index_size": index_size,
        "final_file_size": final_size,
    }


def generate_seek_index_sidecar(mivf_path: Path, sidecar_path: Path | None = None) -> Path | None:
    if sidecar_path is None:
        sidecar_path = make_idx_sidecar_path(mivf_path)
    seek_data = collect_seek_index_data(mivf_path)
    if not seek_data.seek_points:
        print(f"SEEK INDEX: no sync points found for {mivf_path}; skipping sidecar")
        return None

    payload = build_hfix58j_payload(
        seek_data.file_size,
        seek_data.first_offset,
        seek_data.total_frames,
        seek_data.seek_points,
    )
    return write_seek_index_sidecar_payload(
        mivf_path,
        sidecar_path,
        payload,
        seek_data.seek_points,
        seek_data.total_frames,
        seek_data.file_size,
        seek_data.first_offset,
    )


def fmt_time(secs: float) -> str:
    minutes, seconds = divmod(int(secs), 60)
    hours, minutes = divmod(minutes, 60)
    if hours > 0:
        return f"{hours:02d}:{minutes:02d}:{seconds:02d}"
    return f"{minutes:02d}:{seconds:02d}"


def clamp_s16(value: int) -> int:
    return max(-32768, min(32767, int(value)))


def make_temp_workdir() -> Path:
    return Path(tempfile.mkdtemp(prefix="mivf_encode_"))


def mivf_helper_path() -> Path:
    return bundled_path("miv2y_moflex_tier.exe")


def copy_helper_binary(workdir: Path) -> Path:
    helper = mivf_helper_path()
    target = workdir / helper.name
    shutil.copy2(helper, target)
    return target


def mivf_ffmpeg_path() -> str:
    candidates = [
        resource_dir() / "ffmpeg.exe",
        resource_dir() / "ffmpeg",
        Path(sys.executable).resolve().parent / "ffmpeg.exe",
        Path(sys.executable).resolve().parent / "ffmpeg",
    ]

    if getattr(sys, "frozen", False):
        exe_dir = Path(sys.executable).resolve().parent
        candidates = [exe_dir / "ffmpeg.exe", exe_dir / "ffmpeg"] + candidates

    for candidate in candidates:
        if candidate.exists():
            return str(candidate)

    found = shutil.which("ffmpeg") or shutil.which("ffmpeg.exe")
    if found:
        return found

    raise FileNotFoundError(
        "FFmpeg was not found. Install ffmpeg, add it to PATH, or place ffmpeg.exe next to encode_mivf.exe / encode_mivf.py."
    )


def run_ffmpeg_extract(input_path: Path, output_path: Path, settings: EncodeSettings) -> None:
    ffmpeg = mivf_ffmpeg_path()
    ffmpeg_cmd = [
        ffmpeg,
        "-y",
        "-v",
        "quiet",
        "-stats",
        "-i",
        str(input_path),
        "-vf",
        f"scale={settings.width}:{settings.height},format=yuv420p",
        "-c:v",
        "rawvideo",
        str(output_path),
    ]
    subprocess.run(ffmpeg_cmd, check=True)


def start_ffmpeg_raw_pipe(input_path: Path, settings: EncodeSettings) -> subprocess.Popen[bytes]:
    ffmpeg = mivf_ffmpeg_path()
    ffmpeg_cmd = [
        ffmpeg,
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-stats",
        "-i",
        str(input_path),
        "-map",
        "0:v:0",
        "-an",
        "-sn",
        "-dn",
        "-map_chapters",
        "-1",
        "-vf",
        f"scale={settings.width}:{settings.height},format=yuv420p",
        "-vsync",
        "0",
        "-c:v",
        "rawvideo",
        "-f",
        "rawvideo",
        "pipe:1",
    ]
    return subprocess.Popen(ffmpeg_cmd, stdout=subprocess.PIPE)


def read_frame_chunk(pipe, frame_size: int, max_frames: int) -> tuple[bytes, int]:
    target = frame_size * max_frames
    buf = bytearray()

    while len(buf) < target:
        chunk = pipe.read(target - len(buf))
        if not chunk:
            break
        buf += chunk

    if not buf:
        return b"", 0

    if len(buf) % frame_size:
        raise SystemExit(
            f"FFmpeg produced a partial raw frame: {len(buf)} bytes is not divisible by frame size {frame_size}."
        )

    return bytes(buf), len(buf) // frame_size


def ima_encode_nibble(sample: int, predictor: int, index: int) -> tuple[int, int, int]:
    step = IMA_STEP_TABLE[index]
    diff = int(sample) - predictor
    nibble = 0

    if diff < 0:
        nibble = 8
        diff = -diff

    delta = step >> 3

    if diff >= step:
        nibble |= 4
        diff -= step
        delta += step

    step2 = step >> 1
    if diff >= step2:
        nibble |= 2
        diff -= step2
        delta += step2

    step4 = step >> 2
    if diff >= step4:
        nibble |= 1
        delta += step4

    if nibble & 8:
        predictor -= delta
    else:
        predictor += delta

    predictor = clamp_s16(predictor)
    index += IMA_INDEX_TABLE[nibble & 15]
    index = max(0, min(88, index))
    return nibble & 15, predictor, index


def encode_ia4m_packet(samples: list[int], frame_no: int) -> bytes:
    if not samples:
        samples = [0]

    ns = len(samples)
    predictor0 = clamp_s16(samples[0])
    predictor = predictor0
    index0 = 0
    index = index0

    nibbles: list[int] = []
    for sample in samples[1:]:
        nibble, predictor, index = ima_encode_nibble(sample, predictor, index)
        nibbles.append(nibble)

    adpcm = bytearray()
    for idx in range(0, len(nibbles), 2):
        lo = nibbles[idx] & 15
        hi = nibbles[idx + 1] & 15 if idx + 1 < len(nibbles) else 0
        adpcm.append(lo | (hi << 4))

    body = bytearray()
    body += b"IA4M"
    body += struct.pack("<I", frame_no)
    body += struct.pack("<H", ns)
    body += bytes([1, 0])
    body += struct.pack("<h", predictor0)
    body += bytes([index0, 0])
    body += struct.pack("<I", len(adpcm))
    body += adpcm
    return bytes(body)


def extract_pcm16(source: Path, rate: int, channels: int, workdir: Path) -> Path:
    pcm_path = workdir / "audio.s16le"
    ffmpeg = mivf_ffmpeg_path()
    cmd = [
        ffmpeg,
        "-y",
        "-i",
        str(source),
        "-vn",
        "-ac",
        str(channels),
        "-ar",
        str(rate),
        "-f",
        "s16le",
        str(pcm_path),
    ]
    print("Running:", " ".join(cmd))
    subprocess.run(cmd, check=True)
    return pcm_path


def start_ffmpeg_audio_pipe(source: Path, rate: int, channels: int) -> subprocess.Popen[bytes]:
    ffmpeg = mivf_ffmpeg_path()
    cmd = [
        ffmpeg,
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        str(source),
        "-map",
        "0:a:0",
        "-vn",
        "-sn",
        "-dn",
        "-map_chapters",
        "-1",
        "-ac",
        str(channels),
        "-ar",
        str(rate),
        "-f",
        "s16le",
        "-acodec",
        "pcm_s16le",
        "pipe:1",
    ]
    return subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def read_audio_samples_from_pipe(proc: subprocess.Popen[bytes], samples_per_frame: int, channels: int) -> list[int]:
    if channels != 1:
        raise SystemExit("IA4M mux currently supports mono only")

    bytes_needed = samples_per_frame * channels * 2
    data = proc.stdout.read(bytes_needed) if proc.stdout else b""
    if len(data) < bytes_needed:
        data += b"\x00" * (bytes_needed - len(data))
    return list(struct.unpack("<" + "h" * samples_per_frame, data[:samples_per_frame * 2]))


def read_pcm16(path: Path) -> list[int]:
    data = path.read_bytes()
    if len(data) & 1:
        data = data[:-1]
    if not data:
        return []
    return list(struct.unpack("<" + "h" * (len(data) // 2), data))


def make_stream_desc(sid: int, stype: int, codec: bytes, w: int, h: int, fpsn: int, fpsd: int, extra: bytes) -> bytes:
    return struct.pack(
        "<BBH4sIIHHHHIBBH",
        sid,
        stype,
        STREAM_DESC_SIZE + len(extra),
        codec,
        1,
        fpsn,
        w,
        h,
        fpsn,
        fpsd,
        0,
        0,
        0,
        len(extra),
    ) + extra


def wr_header(template: bytes, streams: int, first: int) -> bytes:
    buf = bytearray(template[:64])
    buf[0:4] = b"MIVF"
    buf[12:16] = struct.pack("<I", streams)
    buf[28:32] = struct.pack("<I", streams)
    buf[36:44] = struct.pack("<Q", first)
    return bytes(buf)


def encoder_segment_cmd(
    helper: Path,
    output_path: Path,
    settings: EncodeSettings,
    start_frame: int,
    recon_dump_path: Path | None = None,
) -> list[str]:
    cmd = [
        str(helper),
        "--input",
        "-",
        "--output",
        str(output_path),
        "--width",
        str(settings.width),
        "--height",
        str(settings.height),
        "--fps",
        str(settings.fps),
        "--keyint",
        str(settings.keyint),
        "--qp",
        str(settings.qp),
        "--c-qp-offset",
        str(settings.c_qp_offset),
        "--lambda",
        str(settings.lambda_value),
        "--y-skip",
        str(settings.y_skip),
        "--c-skip",
        str(settings.c_skip),
        "--y-delta",
        str(settings.y_delta),
        "--c-delta",
        str(settings.c_delta),
        "--mv-range",
        str(settings.mv_range),
        "--motion-search",
        settings.motion_search,
        "--keep",
        str(settings.keep),
        "--start-frame",
        str(start_frame),
    ]
    if recon_dump_path is not None:
        cmd += ["--dump-last-recon", str(recon_dump_path)]
    return cmd


def run_encoder_segment(
    helper: Path,
    output_path: Path,
    settings: EncodeSettings,
    start_frame: int,
    frame_count: int,
    raw_frames: bytes,
    workdir: Path,
    recon_dump_path: Path | None = None,
) -> tuple[Path, str]:
    """Returns (output_path, decoded stdout text). The stdout text carries the
    helper's existing STATS/PSNR lines (see print_stats() / the PSNR printf in
    tools/miv2y_moflex_tier.c) so callers can aggregate them; it was previously
    discarded on success. Encoder invocation, error handling, and the produced
    .mivf bytes are unchanged."""
    result = subprocess.run(
        encoder_segment_cmd(helper, output_path, settings, start_frame, recon_dump_path),
        input=raw_frames,
        capture_output=True,
        cwd=workdir,
    )
    if result.returncode != 0:
        if result.stdout:
            sys.stdout.buffer.write(result.stdout)
        if result.stderr:
            sys.stderr.buffer.write(result.stderr)
        raise RuntimeError(
            f"encoder segment failed at frame {start_frame} ({frame_count} frames), exit {result.returncode}"
        )
    if not output_path.exists():
        raise RuntimeError(f"encoder segment did not create {output_path}")
    stdout_text = result.stdout.decode("utf-8", errors="replace") if result.stdout else ""
    return output_path, stdout_text


def scan_segment_video_packet_sizes(mivf_path: Path, skip_first_page: bool = False) -> list[int]:
    """Return the list of video-packet payload sizes in a single-segment .mivf
    file, before segments are merged into the final output. Segment files have
    the same header/page/packet framing as a full .mivf (see inspect_video_segment
    / copy_video_pages), so the same low-level scan applies.

    skip_first_page: set when this segment has a --warm-start-chunks throwaway
    warm-up page prepended, so the packet-cap retry loop judges/targets only the
    packets that will actually reach the final output (the warm-up page is always
    a forced, QP/keep-insensitive keyframe and would otherwise waste retries)."""
    file_size = mivf_path.stat().st_size
    with mivf_path.open("rb") as f:
        header = f.read(HEADER_SIZE)
        if len(header) != HEADER_SIZE or header[:4] != b"MIVF":
            raise RuntimeError(f"{mivf_path}: not a valid MIVF file")

        first = le64(header, 36)
        if first <= HEADER_SIZE or first > 4096:
            raise RuntimeError(f"{mivf_path}: invalid first page offset {first}")

        desc_blob = f.read(first - HEADER_SIZE)
        if len(desc_blob) != first - HEADER_SIZE:
            raise RuntimeError(f"{mivf_path}: short stream descriptor block")

        video_sid, _codec, _w, _h = parse_video_stream_desc(desc_blob)

        sizes: list[int] = []
        pos = first
        first_page = True
        while pos + PAGE_HEADER_SIZE <= file_size:
            f.seek(pos)
            page_hdr = f.read(PAGE_HEADER_SIZE)
            if len(page_hdr) != PAGE_HEADER_SIZE or page_hdr[:2] != b"MP":
                break

            payload = le32(page_hdr, 16)
            packets = le16(page_hdr, 20)
            if payload == 0 or payload > (512 * 1024) or packets == 0 or packets > 128:
                break

            page_payload_start = pos + PAGE_HEADER_SIZE
            page_end = page_payload_start + payload
            if page_end > file_size:
                break

            if skip_first_page and first_page:
                first_page = False
                pos = page_end
                continue
            first_page = False

            pkt_pos = page_payload_start
            for _ in range(packets):
                if pkt_pos + PACKET_HEADER_SIZE > page_end:
                    break

                f.seek(pkt_pos)
                pkt_hdr = f.read(PACKET_HEADER_SIZE)
                if len(pkt_hdr) != PACKET_HEADER_SIZE:
                    break

                pkt_sid = pkt_hdr[0]
                hsize = le16(pkt_hdr, 2)
                psize = le32(pkt_hdr, 8)
                if hsize < PACKET_HEADER_SIZE or pkt_pos + hsize + psize > page_end:
                    break

                if pkt_sid == video_sid:
                    sizes.append(psize)

                pkt_pos += hsize + psize

            pos = page_end

    return sizes


def run_encoder_segment_capped(
    helper: Path,
    output_path: Path,
    settings: EncodeSettings,
    start_frame: int,
    frame_count: int,
    raw_frames: bytes,
    workdir: Path,
    recon_dump_path: Path | None = None,
    has_warmup_frame: bool = False,
) -> dict:
    """Encode one segment normally; if settings.max_video_packet_kb is set and the
    segment's largest video packet exceeds it, retry that whole segment at a
    higher QP (bounded retries), keeping whichever attempt actually produced the
    smallest max packet. No-op (single normal encode, no scanning) when the cap
    is disabled, so default behavior/perf is unchanged.

    If recon_dump_path is given (--warm-start-chunks), the final reconstructed
    frame is requested from whichever attempt is actually kept, and moved to
    recon_dump_path -- so a QP retry can't silently leave a stale/mismatched
    warm-start reference behind.

    has_warmup_frame: this segment has a throwaway warm-up frame prepended
    (raw_frames/frame_count include it), so the packet-cap scan must skip its
    page -- the warm-up is always a forced keyframe and is discarded before
    merging, so judging the cap against it would waste retries on a packet
    that never reaches the final output."""
    initial_dump = output_path.with_name(output_path.stem + "_cap0.recon") if recon_dump_path else None
    _, best_stdout = run_encoder_segment(helper, output_path, settings, start_frame, frame_count, raw_frames, workdir, initial_dump)

    cap_bytes = settings.max_video_packet_kb * 1024 if settings.max_video_packet_kb > 0 else 0

    best_path = output_path
    best_dump = initial_dump
    best_max = -1
    best_qp = settings.qp
    cur_qp = settings.qp
    attempts = 0

    def finish(result: dict) -> dict:
        if recon_dump_path is not None and best_dump is not None and best_dump.exists():
            os.replace(best_dump, recon_dump_path)
        # Measurement-only: attach whichever attempt's stdout actually became
        # the kept segment output, so STATS/PSNR reflect the real result even
        # when a --max-video-packet-kb retry replaced the initial attempt.
        result["helper_stats"] = parse_helper_stats_line(best_stdout)
        result["helper_psnr"] = parse_helper_psnr_line(best_stdout)
        return result

    if cap_bytes <= 0:
        return finish({"capped": False})

    sizes = scan_segment_video_packet_sizes(output_path, skip_first_page=has_warmup_frame)
    initial_max = max(sizes) if sizes else 0
    best_max = initial_max
    if initial_max <= cap_bytes:
        return finish({"capped": False, "initial_max": initial_max, "final_max": initial_max, "retries": 0})

    while best_max > cap_bytes and attempts < PACKET_CAP_MAX_RETRIES and cur_qp < PACKET_CAP_MAX_QP:
        attempts += 1
        cur_qp = min(PACKET_CAP_MAX_QP, cur_qp + PACKET_CAP_QP_STEP)
        retry_settings = replace(settings, qp=cur_qp)
        retry_path = output_path.with_name(output_path.stem + f"_cap{attempts}.mivf")
        retry_dump = output_path.with_name(output_path.stem + f"_cap{attempts}.recon") if recon_dump_path else None
        _, retry_stdout = run_encoder_segment(helper, retry_path, retry_settings, start_frame, frame_count, raw_frames, workdir, retry_dump)
        retry_max = max(scan_segment_video_packet_sizes(retry_path, skip_first_page=has_warmup_frame), default=0)

        if retry_max < best_max:
            if best_path != output_path:
                best_path.unlink(missing_ok=True)
            if best_dump is not None and best_dump.exists():
                best_dump.unlink(missing_ok=True)
            best_path, best_max, best_qp = retry_path, retry_max, cur_qp
            best_dump = retry_dump
            best_stdout = retry_stdout
        else:
            retry_path.unlink(missing_ok=True)
            if retry_dump is not None:
                retry_dump.unlink(missing_ok=True)

    if best_path != output_path:
        os.replace(best_path, output_path)

    return finish({
        "capped": True,
        "initial_max": initial_max,
        "final_max": best_max,
        "retries": attempts,
        "qp_used": best_qp,
        "still_over_cap": best_max > cap_bytes,
    })


def inspect_video_segment(path: Path) -> tuple[bytes, bytes, int, int, int]:
    with path.open("rb") as f:
        header = f.read(HEADER_SIZE)
        if len(header) != HEADER_SIZE or header[:4] != b"MIVF":
            raise RuntimeError(f"{path}: not a MIVF file")

        streams = le32(header, 12)
        first = le64(header, 36)
        if streams != 1:
            raise RuntimeError(f"{path}: expected 1 video stream, got {streams}")
        if first <= HEADER_SIZE or first > 4096:
            raise RuntimeError(f"{path}: invalid first page offset {first}")

        desc = f.read(first - HEADER_SIZE)
        if len(desc) != first - HEADER_SIZE:
            raise RuntimeError(f"{path}: short stream descriptor")

    fpsn = le16(desc, 20) or DEFAULT_FPS
    fpsd = le16(desc, 22) or 1
    return header, desc, first, fpsn, fpsd


def copy_video_pages(path: Path, out_file, expected_frame: int, first: int, skip_pages: int = 0) -> tuple[int, int]:
    copied = 0
    with path.open("rb") as f:
        f.seek(first)

        # --warm-start-chunks: this segment has `skip_pages` throwaway warm-up
        # page(s) prepended (see build_streaming_parallel_mivf) that must never
        # reach the final output -- they duplicate a frame already copied from
        # the previous segment. Skip them before the real frame-continuity copy.
        for _ in range(skip_pages):
            page_header = f.read(PAGE_HEADER_SIZE)
            if not page_header:
                raise RuntimeError(f"{path}: expected a warm-up page to skip, file too short")
            if len(page_header) != PAGE_HEADER_SIZE or page_header[:2] != b"MP":
                raise RuntimeError(f"{path}: bad warm-up page header")
            payload_size = le32(page_header, 16)
            skipped = f.read(payload_size)
            if len(skipped) != payload_size:
                raise RuntimeError(f"{path}: short warm-up page payload")

        while True:
            page_header = f.read(PAGE_HEADER_SIZE)
            if not page_header:
                break
            if len(page_header) != PAGE_HEADER_SIZE:
                raise RuntimeError(f"{path}: short page header")
            if page_header[:2] != b"MP":
                raise RuntimeError(f"{path}: bad page magic at copied frame {copied}")

            seq = le32(page_header, 4)
            payload_size = le32(page_header, 16)
            if seq != expected_frame:
                raise RuntimeError(f"{path}: expected page frame {expected_frame}, got {seq}")

            payload = f.read(payload_size)
            if len(payload) != payload_size:
                raise RuntimeError(f"{path}: short page payload for frame {seq}")

            out_file.write(page_header)
            out_file.write(payload)
            copied += 1
            expected_frame += 1

    return copied, expected_frame


def merge_video_segments(
    output_path: Path,
    segment_paths: list[Path],
    settings: EncodeSettings,
    warm_started_segments: set[int] | None = None,
) -> int:
    if not segment_paths:
        raise RuntimeError("no video segments to merge")

    warm_started_segments = warm_started_segments or set()

    header0, desc0, first0, fpsn0, fpsd0 = inspect_video_segment(segment_paths[0])
    header = bytearray(header0)
    struct.pack_into("<Q", header, 20, 0)
    struct.pack_into("<Q", header, 36, first0)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    total_frames = 0
    expected_frame = 0

    with output_path.open("wb") as out_file:
        out_file.write(header)
        out_file.write(desc0)

        for idx, segment in enumerate(segment_paths):
            _header, desc, first, fpsn, fpsd = inspect_video_segment(segment)
            if first != first0:
                raise RuntimeError(f"{segment}: first page offset differs from segment 0")
            if desc != desc0:
                raise RuntimeError(f"{segment}: stream descriptor differs from segment 0")
            if fpsn != fpsn0 or fpsd != fpsd0:
                raise RuntimeError(f"{segment}: FPS differs from segment 0")

            skip_pages = 1 if idx in warm_started_segments else 0
            copied, expected_frame = copy_video_pages(segment, out_file, expected_frame, first, skip_pages)
            total_frames += copied
            print(f"merged segment {idx}: frames={copied}", flush=True)

        duration = total_frames * 30000 // settings.fps
        out_file.seek(20)
        out_file.write(struct.pack("<Q", duration))

    print(f"WROTE {output_path}")
    print(f"segments={len(segment_paths)} frames={total_frames} bytes={output_path.stat().st_size}")
    return total_frames


def build_streaming_parallel_mivf(input_path: Path, temp_video_only: Path, settings: EncodeSettings, workdir: Path) -> dict:
    helper = copy_helper_binary(workdir)
    frame_size = settings.width * settings.height + (settings.width // 2) * (settings.height // 2) * 2
    jobs = max(1, settings.jobs)
    chunk_frames = max(1, settings.chunk_frames)

    print(
        f"Streaming Engine: ffmpeg pipe -> {jobs} encoder worker(s), "
        f"{chunk_frames} frames/chunk (~{(frame_size * chunk_frames) / 1048576.0:.1f} MiB per active worker)."
    )

    ffmpeg_proc = start_ffmpeg_raw_pipe(input_path, settings)
    if ffmpeg_proc.stdout is None:
        raise RuntimeError("failed to open FFmpeg rawvideo pipe")

    segment_paths: dict[int, Path] = {}
    pending: dict[Future[dict], tuple[int, Path, int, int, Path | None]] = {}
    next_segment = 0
    next_frame = 0
    completed_frames = 0
    start_time = time.time()
    cap_stats: list[dict] = []
    # Measurement-only: cross-segment aggregation of the helper's existing
    # STATS/PSNR stdout (see parse_helper_stats_line / parse_helper_psnr_line).
    agg_helper_stats: dict[str, int] = {}
    agg_helper_psnr: dict[str, dict[str, float]] = {}

    # --warm-start-chunks: chunk N>0 is fed the previous chunk's own closed-loop
    # reconstructed last frame as a throwaway prepended frame, so its real first
    # frame lands at local fi=1 (not fi=0) and can use ordinary QP/keep-responsive
    # P-frame coding instead of being forced into a SOLID/RAW-only keyframe. The
    # warm-up page is dropped before merging (see copy_video_pages/skip_pages).
    warm_bytes_by_seg: dict[int, bytes] = {}
    warm_started_segments: set[int] = set()

    def drain_one(block: bool) -> None:
        nonlocal completed_frames
        if not pending:
            return
        done, _ = wait(pending.keys(), return_when=FIRST_COMPLETED if not block else FIRST_COMPLETED)
        for future in done:
            seg_idx, seg_path, start_frame, frame_count, recon_dump_path = pending.pop(future)
            stats = future.result()
            segment_paths[seg_idx] = seg_path
            completed_frames += frame_count
            if stats.get("capped"):
                cap_stats.append({**stats, "seg_idx": seg_idx, "frame_count": frame_count})
            helper_stats = stats.get("helper_stats")
            if helper_stats:
                accumulate_helper_stats(agg_helper_stats, helper_stats)
            helper_psnr = stats.get("helper_psnr")
            if helper_psnr:
                accumulate_helper_psnr(agg_helper_psnr, helper_psnr, frame_count)
            if recon_dump_path is not None:
                if not recon_dump_path.exists():
                    raise RuntimeError(f"segment {seg_idx}: expected --dump-last-recon output at {recon_dump_path}")
                warm_bytes_by_seg[seg_idx] = recon_dump_path.read_bytes()
                recon_dump_path.unlink(missing_ok=True)
            elapsed = max(0.001, time.time() - start_time)
            print(
                f"encoded segment {seg_idx}: start={start_frame} frames={frame_count} "
                f"total={completed_frames} speed={completed_frames / elapsed:.1f} fps",
                flush=True,
            )
            if not block:
                break

    try:
        with ThreadPoolExecutor(max_workers=jobs) as pool:
            while True:
                while len(pending) >= jobs:
                    drain_one(block=True)

                raw_chunk, frame_count = read_frame_chunk(ffmpeg_proc.stdout, frame_size, chunk_frames)
                if frame_count == 0:
                    break

                seg_idx = next_segment
                seg_path = workdir / f"segment_{seg_idx:05d}.mivf"

                submit_raw = raw_chunk
                submit_start_frame = next_frame
                submit_frame_count = frame_count

                if settings.warm_start_chunks and seg_idx > 0:
                    # Needs chunk (seg_idx-1)'s reconstructed last frame, which only
                    # exists once that chunk's subprocess has fully finished -- this
                    # forces strict sequential ordering across chunk boundaries when
                    # the flag is on (parallelism is inherently traded for a real,
                    # bit-exact prediction reference; default behavior is unaffected).
                    while seg_idx - 1 not in warm_bytes_by_seg:
                        drain_one(block=True)
                    warm_frame = warm_bytes_by_seg.pop(seg_idx - 1)
                    submit_raw = warm_frame + raw_chunk
                    submit_start_frame = next_frame - 1
                    submit_frame_count = frame_count + 1
                    warm_started_segments.add(seg_idx)

                recon_dump_path = workdir / f"segment_{seg_idx:05d}.reconlast" if settings.warm_start_chunks else None

                future = pool.submit(
                    run_encoder_segment_capped,
                    helper,
                    seg_path,
                    settings,
                    submit_start_frame,
                    submit_frame_count,
                    submit_raw,
                    workdir,
                    recon_dump_path,
                    seg_idx in warm_started_segments,
                )
                pending[future] = (seg_idx, seg_path, next_frame, frame_count, recon_dump_path)
                next_segment += 1
                next_frame += frame_count

            while pending:
                drain_one(block=True)
    finally:
        if ffmpeg_proc.stdout:
            ffmpeg_proc.stdout.close()

    ffmpeg_rc = ffmpeg_proc.wait()
    if ffmpeg_rc != 0:
        raise RuntimeError(f"FFmpeg rawvideo pipe failed (exit code {ffmpeg_rc})")

    ordered_segments = [segment_paths[idx] for idx in range(next_segment)]
    merged_frames = merge_video_segments(temp_video_only, ordered_segments, settings, warm_started_segments)
    if merged_frames != next_frame:
        raise RuntimeError(f"merged {merged_frames} frames, expected {next_frame}")

    if settings.warm_start_chunks:
        print()
        print("============================================================")
        print("WARM-START CHUNK RESULTS (experimental)")
        print("============================================================")
        print(f"  segments total:       {next_segment}")
        print(f"  segments warm-started: {len(warm_started_segments)} (segment 0 is never warm-started)")
        print("============================================================")

    if settings.max_video_packet_kb > 0:
        print()
        print("============================================================")
        print("VIDEO PACKET CAP RESULTS")
        print("============================================================")
        print(f"  cap:                  {settings.max_video_packet_kb} KB")
        print(f"  chunk_frames:         {chunk_frames}  (retry unit is a whole chunk, not a single frame)")
        print(f"  segments total:       {next_segment}")
        print(f"  segments capped:      {len(cap_stats)}")
        if cap_stats:
            frames_affected = sum(s["frame_count"] for s in cap_stats)
            avg_retries = sum(s["retries"] for s in cap_stats) / len(cap_stats)
            worst_before = max(s["initial_max"] for s in cap_stats)
            worst_after = max(s["final_max"] for s in cap_stats)
            still_over = [s for s in cap_stats if s.get("still_over_cap")]
            print(f"  frames affected:      {frames_affected}")
            print(f"  avg retries/segment:  {avg_retries:.1f} (limit {PACKET_CAP_MAX_RETRIES})")
            print(f"  worst packet before:  {worst_before} B ({worst_before / 1024:.1f} KB)")
            print(f"  worst packet after:   {worst_after} B ({worst_after / 1024:.1f} KB)")
            if still_over:
                worst_remaining = max(s["final_max"] for s in still_over)
                unmoved = [s for s in still_over if s["final_max"] >= s["initial_max"]]
                print(
                    f"  *** WARNING: {len(still_over)} segment(s) still exceed the cap after "
                    f"{PACKET_CAP_MAX_RETRIES} retries at QP up to {PACKET_CAP_MAX_QP}. "
                    f"Worst remaining packet: {worst_remaining} B ({worst_remaining / 1024:.1f} KB). "
                    f"Further QP increases risk visible quality loss -- consider --keep 8/--profile 3ds-fast instead."
                )
                if unmoved:
                    print(
                        f"  *** NOTE: {len(unmoved)} of those segment(s) shrank by 0 bytes across all "
                        f"{PACKET_CAP_MAX_RETRIES} QP retries -- their largest packet is QP-insensitive "
                        f"(likely a forced keyframe or an extreme-motion frame hitting a size floor this "
                        f"encoder's QP does not control). Raising QP further will not help; a real fix "
                        f"needs a different lever (--keep, source pre-filtering, or a decoder-side change)."
                    )
        print("============================================================")

    for segment in ordered_segments:
        segment.unlink(missing_ok=True)

    return {
        "frames": merged_frames,
        "segments": next_segment,
        "elapsed_sec": time.time() - start_time,
        "helper_stats": agg_helper_stats,
        "helper_psnr": finalize_helper_psnr(agg_helper_psnr),
    }


def build_parallel_mivf(workdir: Path, temp_master_yuv: Path, temp_video_only: Path, settings: EncodeSettings) -> None:
    helper = copy_helper_binary(workdir)

    total_bytes = temp_master_yuv.stat().st_size
    frame_size = settings.width * settings.height + (settings.width // 2) * (settings.height // 2) * 2
    total_frames = total_bytes // frame_size

    cores = cpu_count()
    print(f"Parallel Engine: Slicing raw data across {cores} CPU Core Clusters...")

    frames_per_core = total_frames // cores if cores else total_frames
    if frames_per_core == 0:
        cores = 1
        frames_per_core = total_frames

    chunk_files: list[Path] = []
    chunk_outputs: list[Path] = []
    processes: list[subprocess.Popen[bytes]] = []

    with temp_master_yuv.open("rb") as source:
        for idx in range(cores):
            num_frames = frames_per_core if idx < cores - 1 else total_frames - (frames_per_core * idx)
            if num_frames <= 0:
                break

            chunk_yuv = workdir / f"temp_slice_{idx}.yuv"
            chunk_mivf = workdir / f"temp_slice_{idx}.mivf"
            chunk_files.append(chunk_yuv)
            chunk_outputs.append(chunk_mivf)
            chunk_yuv.write_bytes(source.read(num_frames * frame_size))

    print("Parallel Engine: Master raw cache split successfully. Flushing temporary disk space...")
    temp_master_yuv.unlink(missing_ok=True)

    print(f"Parallel Engine: Launching {len(chunk_files)} concurrent compression instances at full throttle...")
    start_time = time.time()

    for idx, chunk_file in enumerate(chunk_files):
        cmd = [
            str(helper),
            "--input",
            str(chunk_file),
            "--output",
            str(chunk_outputs[idx]),
            "--width",
            str(settings.width),
            "--height",
            str(settings.height),
            "--fps",
            str(settings.fps),
            "--keyint",
            str(settings.keyint),
            "--qp",
            str(settings.qp),
            "--c-qp-offset",
            str(settings.c_qp_offset),
            "--lambda",
            str(settings.lambda_value),
            "--y-skip",
            str(settings.y_skip),
            "--c-skip",
            str(settings.c_skip),
            "--y-delta",
            str(settings.y_delta),
            "--c-delta",
            str(settings.c_delta),
            "--mv-range",
            str(settings.mv_range),
            "--motion-search",
            settings.motion_search,
            "--keep",
            str(settings.keep),
        ]
        processes.append(subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=workdir))

    while True:
        active = [proc for proc in processes if proc.poll() is None]
        active_count = len(active)
        done_count = len(processes) - active_count
        elapsed = time.time() - start_time

        eta_str = fmt_time((len(processes) - done_count) * (elapsed / done_count)) if done_count > 0 else "Calculating..."
        sys.stdout.write(
            f"\r[Time: {fmt_time(elapsed)}] | Cores Active: {active_count}/{len(processes)} | Chunks Done: {done_count} | Predicted ETA: {eta_str}"
        )
        sys.stdout.flush()

        if active_count == 0:
            break
        time.sleep(1)

    final_time = time.time() - start_time
    print("\n============================================================")
    print(f"TOTAL ENCODER RUNTIME: {fmt_time(final_time)} | Speed: {total_frames / final_time:.1f} fps")
    print("============================================================")

    print("Parallel Engine: Patching headers and reconstructing container streams...")
    with temp_video_only.open("wb") as out_file:
        first_chunk = chunk_outputs[0].read_bytes()
        header = bytearray(first_chunk[:96])

        chunk_duration = (total_frames // cores) * 30000 // settings.fps
        total_duration = total_frames * 30000 // settings.fps
        chunk_dur_bytes = struct.pack("<Q", chunk_duration)
        total_dur_bytes = struct.pack("<Q", total_duration)
        idx = header.find(chunk_dur_bytes)
        if idx != -1:
            header[idx:idx + 8] = total_dur_bytes
        out_file.write(header)

        running_frame_idx = 0
        for chunk_mivf in chunk_outputs:
            data = chunk_mivf.read_bytes()
            offset = 96
            file_len = len(data)

            while offset < file_len:
                page_header = bytearray(data[offset:offset + 32])
                payload_size = struct.unpack_from("<I", page_header, 16)[0]
                wr_u32le(page_header, 4, running_frame_idx)
                wr_u64le(page_header, 8, running_frame_idx * 30000 // settings.fps)
                out_file.write(page_header)
                out_file.write(data[offset + 32:offset + 32 + payload_size])
                offset += 32 + payload_size
                running_frame_idx += 1

    for chunk_file, chunk_mivf in zip(chunk_files, chunk_outputs):
        chunk_file.unlink(missing_ok=True)
        chunk_mivf.unlink(missing_ok=True)

    print(f"Parallel Engine: Master container unified with {running_frame_idx} sequential frames.")


def mux_audio_into_mivf(video_mivf: Path, audio_src: Path, out_path: Path, rate: int, channels: int, workdir: Path, audio_codec: str = "ia4m", audio_offset_ms: int = 0) -> None:
    audio_codec = audio_codec.lower()
    if audio_codec not in {"ia4m", "pc16"}:
        raise SystemExit(f"unsupported audio codec: {audio_codec}")
    if audio_codec == "ia4m" and channels != 1:
        raise SystemExit("IA4M mux currently supports mono only; use --audio-codec pc16 for stereo")
    if channels not in (1, 2):
        raise SystemExit("audio channels must be 1 or 2")

    frame_no = 0

    with video_mivf.open("rb") as vf, out_path.open("wb") as out_file:
        header = vf.read(HEADER_SIZE)
        if len(header) != HEADER_SIZE or header[:4] != b"MIVF":
            raise SystemExit("not MIVF")

        streams = le32(header, 12)
        first_old = le64(header, 36)
        if streams != 1:
            raise SystemExit(f"expected video-only MIVF with 1 stream, got {streams}")
        if first_old <= HEADER_SIZE or first_old > 4096:
            raise SystemExit(f"invalid first page offset: {first_old}")

        desc0 = vf.read(first_old - HEADER_SIZE)
        if len(desc0) != first_old - HEADER_SIZE:
            raise SystemExit("short video stream descriptor")

        fpsn = le16(desc0, 20) or DEFAULT_FPS
        fpsd = le16(desc0, 22) or 1
        samples_per_frame = rate * fpsd // fpsn
        if samples_per_frame <= 0:
            raise SystemExit("bad audio samples/frame")

        codec_tag = b"IA4M" if audio_codec == "ia4m" else b"PC16"
        extra = codec_tag + struct.pack("<IHHI", rate, channels, samples_per_frame, 0)
        if len(extra) != 16:
            raise AssertionError(len(extra))

        desc1 = make_stream_desc(1, 2, codec_tag, rate, channels, samples_per_frame, 1, extra)
        first_new = HEADER_SIZE + len(desc0) + len(desc1)

        out_file.write(wr_header(header, 2, first_new))
        out_file.write(desc0)
        out_file.write(desc1)

        # Phase 1 perf: these three are constant for the whole file (same
        # samples_per_frame/format string every iteration), so build them
        # once here instead of having struct.pack/unpack re-parse a format
        # string (and, for pcm_struct, re-concatenate "<" + "h"*N) on every
        # single frame. Pure bookkeeping -- same bytes in, same bytes out.
        pcm_struct = struct.Struct("<" + "h" * samples_per_frame)
        packet_hdr_struct = struct.Struct("<BBHIII")
        page_struct = struct.Struct("<2sBBIQIHHII")

        audio_proc = start_ffmpeg_audio_pipe(audio_src, rate, channels)

        # --audio-offset-ms: shift the whole audio track relative to video by
        # a constant number of samples, computed once up front. Positive:
        # delay audio (prepend silence). Negative: advance audio (drop
        # samples from the start). Applied to the raw linear PCM stream
        # before it's chopped into samples_per_frame packets below, so every
        # packet after the shift keeps its normal size/frame alignment --
        # only the content of the first affected packet(s) changes.
        offset_samples = int(round(audio_offset_ms * rate / 1000.0))
        silence_samples_remaining = offset_samples if offset_samples > 0 else 0
        samples_to_trim = -offset_samples if offset_samples < 0 else 0

        if samples_to_trim > 0:
            trim_bytes = samples_to_trim * channels * 2
            drained = 0
            while drained < trim_bytes and audio_proc.stdout:
                chunk = audio_proc.stdout.read(min(65536, trim_bytes - drained))
                if not chunk:
                    break
                drained += len(chunk)
            print(
                f"AUDIO OFFSET: audio_offset_ms={audio_offset_ms} trimmed {drained // (channels * 2)} "
                f"samples ({drained} bytes) from the start of the audio track"
            )
        elif silence_samples_remaining > 0:
            print(
                f"AUDIO OFFSET: audio_offset_ms={audio_offset_ms} prepending {silence_samples_remaining} "
                f"samples ({silence_samples_remaining * channels * 2} bytes) of silence before the audio track"
            )

        try:
            while True:
                page_header = vf.read(PAGE_HEADER_SIZE)
                if not page_header:
                    break
                if len(page_header) != PAGE_HEADER_SIZE:
                    raise SystemExit("short page header")
                if page_header[:2] != b"MP":
                    raise SystemExit(f"bad page magic at frame {frame_no}")

                page_flags = page_header[3]
                page_seq = le32(page_header, 4)
                page_pts = le64(page_header, 8)
                payload_size = le32(page_header, 16)
                packets = le16(page_header, 20)
                reserved = le16(page_header, 22)
                payload = vf.read(payload_size)
                if len(payload) != payload_size:
                    raise SystemExit(f"short page payload at frame {frame_no}")

                pcm_bytes_needed = samples_per_frame * channels * 2

                if silence_samples_remaining > 0:
                    silence_here = min(silence_samples_remaining, samples_per_frame)
                    pcm = b"\x00" * (silence_here * channels * 2)
                    silence_samples_remaining -= silence_here
                    remaining_bytes = pcm_bytes_needed - len(pcm)
                    if remaining_bytes > 0 and audio_proc.stdout:
                        pcm += audio_proc.stdout.read(remaining_bytes)
                elif audio_proc.stdout:
                    pcm = audio_proc.stdout.read(pcm_bytes_needed)
                else:
                    pcm = b""
                if len(pcm) < pcm_bytes_needed:
                    pcm += b"\x00" * (pcm_bytes_needed - len(pcm))

                if audio_codec == "ia4m":
                    samples = list(pcm_struct.unpack(pcm[:samples_per_frame * 2]))
                    abody = encode_ia4m_packet(samples, frame_no)
                else:
                    abody = pcm
                apkt = packet_hdr_struct.pack(1, 0, PACKET_HEADER_SIZE, 0, len(abody), frame_no) + abody

                new_payload = payload + apkt
                crc = zlib.crc32(new_payload) & 0xFFFFFFFF

                page = page_struct.pack(
                    b"MP",
                    PAGE_HEADER_SIZE,
                    page_flags,
                    page_seq,
                    page_pts,
                    len(new_payload),
                    packets + 1,
                    reserved,
                    crc,
                    0,
                )

                out_file.write(page)
                out_file.write(new_payload)

                frame_no += 1
                if (frame_no % 300) == 0:
                    print(f"muxed {frame_no} frames", flush=True)
        finally:
            if audio_proc.stdout:
                audio_proc.stdout.close()
            stderr = audio_proc.stderr.read().decode("utf-8", errors="replace") if audio_proc.stderr else ""
            audio_rc = audio_proc.wait()
            if audio_rc != 0 and frame_no == 0:
                if stderr:
                    sys.stderr.write(stderr)
                raise SystemExit(f"FFmpeg audio pipe failed (exit code {audio_rc})")
            if audio_rc != 0 and stderr.strip():
                print("NOTE: FFmpeg audio pipe ended nonzero after mux completion:", file=sys.stderr)
                print(stderr, file=sys.stderr)

    print(f"WROTE {out_path}")
    print(f"frames={frame_no} audio={audio_codec.upper()} {rate}Hz channels={channels} samples/frame={samples_per_frame} bytes={out_path.stat().st_size}")


def deploy_output(output_path: Path) -> None:
    sd_card = Path("/d")

    print()
    print("============================================================")
    print("Deploying Package to SD Card")
    print("============================================================")

    if not sd_card.exists():
        print(f"WARNING: SD Card volume '{sd_card}' not found!")
        print(f"Your completed file is safe locally at: {output_path}")
        return

    target_dir = sd_card / "3ds" / "mivf_player_3ds"
    target_dir.mkdir(parents=True, exist_ok=True)
    player_3dsx = resource_dir() / "mivf_player_3ds.3dsx"
    if player_3dsx.exists():
        shutil.copy2(player_3dsx, target_dir / "mivf_player_3ds.3dsx")
    else:
        print(f"WARNING: Bundled 3DS payload not found at {player_3dsx}; skipping 3DS copy.")
    shutil.copy2(output_path, sd_card / output_path.name)

    sidecar_idx = make_idx_sidecar_path(output_path)
    if sidecar_idx.exists():
        shutil.copy2(sidecar_idx, sd_card / sidecar_idx.name)

    print("DEPLOY SUCCESSFUL!")


def find_m2y2_transcoder() -> Path | None:
    """Locate the native m2y2_transcode helper (script or frozen layouts)."""
    here = Path(__file__).resolve().parent
    candidates = [
        resource_dir() / "m2y2_transcode.exe",
        resource_dir() / "tools" / "m2y2_transcode.exe",
        here / "tools" / "m2y2_transcode.exe",
        here / "m2y2_transcode.exe",
        resource_dir() / "m2y2_transcode",
        resource_dir() / "tools" / "m2y2_transcode",
        here / "tools" / "m2y2_transcode",
    ]
    for path in candidates:
        if path.exists():
            return path
    return None


def transcode_to_m2y2(src: Path, dst: Path) -> None:
    """Losslessly range-code an M2Y1 .mivf into the smaller M2Y2 codec.

    Reuses the self-verifying native transcoder (tools/m2y2_transcode.c). It
    checks every converted packet byte-for-byte against the original and exits
    non-zero on any mismatch, so a clean exit guarantees identical decoded
    quality with a smaller file.
    """
    exe = find_m2y2_transcoder()
    if exe is None:
        raise SystemExit(
            "M2Y2 requested but m2y2_transcode(.exe) was not found.\n"
            "Build it once with:\n"
            "  gcc -O2 -o tools/m2y2_transcode.exe tools/m2y2_transcode.c"
        )
    result = subprocess.run(
        [str(exe), str(src), str(dst)],
        capture_output=True,
        text=True,
    )
    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.returncode != 0 or not dst.exists():
        if result.stderr:
            sys.stderr.write(result.stderr)
        raise SystemExit(f"M2Y2 transcode failed (exit code {result.returncode}).")


def print_encode_summary(
    input_path: Path,
    output_path: Path,
    settings: EncodeSettings,
    make_m2y2: bool,
    video_only_bytes: int,
    total_frames: int,
    elapsed_sec: float,
    helper_stats: dict,
    helper_psnr: dict,
    packet_stats: dict | None,
) -> None:
    """Measurement-only final summary. Aggregates the native helper's existing
    STATS/PSNR stdout (tools/miv2y_moflex_tier.c) plus already-computed file
    sizes, so full/diamond/fast (and any future --motion-search mode) can be
    compared run-to-run. Prints only -- never changes encoder decisions or the
    bitstream."""
    final_size = output_path.stat().st_size if output_path.exists() else 0

    print()
    print("============================================================")
    print("ENCODE SUMMARY")
    print("============================================================")
    print(f"  input:                {input_path}")
    print(f"  output:               {output_path}")
    print(f"  motion-search:        {settings.motion_search}")
    print(f"  resolution:           {settings.width}x{settings.height} @ {settings.fps} fps")
    print(f"  qp:                   {settings.qp} (c-qp-offset {settings.c_qp_offset})")
    print(f"  lambda:               {settings.lambda_value}")
    print(f"  keep:                 {settings.keep}")
    print(f"  mv-range:             {settings.mv_range}")
    print(f"  chunk-frames:         {settings.chunk_frames}")
    print(f"  jobs:                 {settings.jobs}")
    print(f"  warm-start-chunks:    {'on' if settings.warm_start_chunks else 'off'}")
    print(f"  m2y2:                 {'on' if make_m2y2 else 'off'}")
    print(f"  total frames:         {total_frames}")
    fps_note = f"  ({total_frames / elapsed_sec:.1f} fps)" if elapsed_sec > 0 and total_frames > 0 else ""
    print(f"  encode wall time:     {elapsed_sec:.1f} s{fps_note}")
    print(f"  video-only bytes:     {video_only_bytes} ({video_only_bytes / 1048576.0:.2f} MiB)")
    print(f"  final output bytes:   {final_size} ({final_size / 1048576.0:.2f} MiB)")

    if helper_stats:
        frames_seen = helper_stats.get("frames", 0)
        print(f"  helper-reported frames: {frames_seen} (I={helper_stats.get('I', 0)} P={helper_stats.get('P', 0)})")
        if frames_seen != total_frames:
            print(
                f"    note: differs from total frames above by {frames_seen - total_frames} -- "
                f"expected when --warm-start-chunks is on (each warm-started segment encodes "
                f"one extra throwaway frame that never reaches the final output)."
            )

        def mode_line(prefix: str) -> str:
            keys = [
                "skip", "delta", "solid", "mv", "mvqres", "qres", "tr", "mvtr",
                "qz", "mvqz", "trz", "mvtrz", "raw", "run_tokens", "run_blocks",
            ]
            return " ".join(f"{k}={helper_stats.get(f'{prefix}_{k}', 0)}" for k in keys)

        print(f"  Y modes:              {mode_line('Y')}")
        print(f"  C modes:              {mode_line('C')}")
        print(
            f"  sum of segment total_bytes: {helper_stats.get('total_bytes', 0)} "
            f"(includes per-segment header overhead and any warm-start filler frames; "
            f"not directly comparable to final output bytes above)"
        )
    else:
        print("  helper STATS:         not available (no STATS line found in encoder stdout)")

    if helper_psnr:
        parts = [f"{k}={v:.2f}dB" for k, v in helper_psnr.items()]
        print(f"  PSNR (frame-weighted avg, approximate): {' '.join(parts)}")

    if packet_stats and packet_stats.get("count"):
        print(
            f"  video packet sizes:   min={packet_stats['min']}B avg={packet_stats['avg']:.0f}B "
            f"p50={packet_stats['p50']}B p95={packet_stats['p95']}B p99={packet_stats['p99']}B "
            f"max={packet_stats['max']}B"
        )
    else:
        print("  video packet sizes:   not computed (pass --report-packet-sizes for full histogram)")

    print("============================================================")


def encode_one(
    input_path: Path,
    output_path: Path,
    settings: EncodeSettings,
    deploy_sd: bool,
    make_m2y2: bool = False,
    make_seek_index: bool = True,
    make_embedded_index: bool = True,
    report_packet_sizes: bool = False,
    seek_index_sidecar: Path | None = None,
) -> None:
    workdir = make_temp_workdir()
    total_t0 = time.time()
    mux_elapsed = 0.0
    m2y2_elapsed = 0.0
    seek_index_elapsed = 0.0
    try:
        temp_video_only = workdir / "temp_video_only.mivf"

        print("============================================================")
        print("1. Streaming and Compressing Video")
        print("============================================================")
        encode_stats = build_streaming_parallel_mivf(input_path, temp_video_only, settings, workdir)
        video_only_bytes = temp_video_only.stat().st_size

        print()
        print("============================================================")
        print(f"2. Multiplexing Audio ({settings.audio_codec.upper()})")
        print("============================================================")
        mux_t0 = time.time()
        mux_audio_into_mivf(temp_video_only, input_path, output_path, settings.audio_rate, settings.audio_channels, workdir, settings.audio_codec, settings.audio_offset_ms)
        mux_elapsed = time.time() - mux_t0

        if make_m2y2:
            print()
            print("============================================================")
            print("3. Range-coding video to M2Y2 (lossless, smaller file)")
            print("============================================================")
            m2y2_t0 = time.time()
            tmp_m2y2 = output_path.with_name(output_path.stem + ".m2y2tmp")
            transcode_to_m2y2(output_path, tmp_m2y2)
            os.replace(tmp_m2y2, output_path)
            m2y2_elapsed = time.time() - m2y2_t0

        if make_seek_index:
            print()
            print("============================================================")
            print("4. Generating Seek Index Metadata")
            print("============================================================")
            seek_index_t0 = time.time()
            try:
                seek_data = collect_seek_index_data(output_path)
                if not seek_data.seek_points:
                    print(f"SEEK INDEX: no sync points found for {output_path}; skipping embedded+sidecar")
                else:
                    if make_embedded_index:
                        append_embedded_seek_index(output_path, seek_data)
                    else:
                        print("SEEK INDEX EMBEDDED: disabled by --no-embedded-index")

                    sidecar_path = seek_index_sidecar or make_idx_sidecar_path(output_path)
                    final_file_size = output_path.stat().st_size
                    sidecar_payload = build_hfix58j_payload(
                        final_file_size,
                        seek_data.first_offset,
                        seek_data.total_frames,
                        seek_data.seek_points,
                    )
                    write_seek_index_sidecar_payload(
                        output_path,
                        sidecar_path,
                        sidecar_payload,
                        seek_data.seek_points,
                        seek_data.total_frames,
                        final_file_size,
                        seek_data.first_offset,
                    )
            except Exception as exc:
                print(f"WARNING: seek index generation failed: {exc}")
            seek_index_elapsed = time.time() - seek_index_t0

        packet_stats: dict | None = None
        if report_packet_sizes:
            print()
            print("============================================================")
            print("5. Packet Size Report")
            print("============================================================")
            try:
                packet_stats = report_packet_size_stats(output_path, chunk_frames=settings.chunk_frames)
            except Exception as exc:
                print(f"WARNING: packet size report failed: {exc}")

        if deploy_sd:
            deploy_output(output_path)

        total_elapsed = time.time() - total_t0
        video_elapsed = encode_stats.get("elapsed_sec", 0.0)

        print()
        print("============================================================")
        print("STAGE TIMING")
        print("============================================================")
        print(f"  1. video encode:      {fmt_time(video_elapsed)} ({video_elapsed:.1f}s)")
        print(f"  2. audio mux:         {fmt_time(mux_elapsed)} ({mux_elapsed:.1f}s)")
        if make_m2y2:
            print(f"  3. m2y2 transcode:    {fmt_time(m2y2_elapsed)} ({m2y2_elapsed:.1f}s)")
        if make_seek_index:
            print(f"  4. seek index:        {fmt_time(seek_index_elapsed)} ({seek_index_elapsed:.1f}s)")
        print(f"  total:                {fmt_time(total_elapsed)} ({total_elapsed:.1f}s)")
        print("============================================================")

        print_encode_summary(
            input_path=input_path,
            output_path=output_path,
            settings=settings,
            make_m2y2=make_m2y2,
            video_only_bytes=video_only_bytes,
            total_frames=encode_stats.get("frames", 0),
            elapsed_sec=encode_stats.get("elapsed_sec", 0.0),
            helper_stats=encode_stats.get("helper_stats") or {},
            helper_psnr=encode_stats.get("helper_psnr") or {},
            packet_stats=packet_stats,
        )
    finally:
        shutil.rmtree(workdir, ignore_errors=True)


def encode_folder(
    input_dir: Path,
    output_dir: Path,
    settings: EncodeSettings,
    make_m2y2: bool = False,
    make_seek_index: bool = True,
    make_embedded_index: bool = True,
    report_packet_sizes: bool = False,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    files = sorted(p for p in input_dir.iterdir() if p.is_file() and p.suffix.lower() in VIDEO_EXTS)

    if not files:
        raise SystemExit(f"No supported video files found in {input_dir}")

    for source in files:
        target = output_dir / f"{source.stem}.mivf"
        print()
        print("============================================================")
        print(f"Encoding: {source}")
        print(f"Output:   {target}")
        print("============================================================")
        encode_one(
            source,
            target,
            settings,
            deploy_sd=False,
            make_m2y2=make_m2y2,
            make_seek_index=make_seek_index,
            make_embedded_index=make_embedded_index,
            report_packet_sizes=report_packet_sizes,
            seek_index_sidecar=None,
        )

    print()
    print(f"Batch encode complete. Outputs are in: {output_dir}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="All-in-one MIVF encoder")
    parser.add_argument("input", help="input video file or input folder")
    parser.add_argument("output", help="output .mivf file or output folder")
    parser.add_argument("--width", type=int, default=DEFAULT_WIDTH)
    parser.add_argument("--height", type=int, default=DEFAULT_HEIGHT)
    parser.add_argument("--fps", type=int, default=DEFAULT_FPS)
    parser.add_argument("--audio-rate", type=int, default=DEFAULT_AUDIO_RATE)
    parser.add_argument("--audio-channels", type=int, default=DEFAULT_AUDIO_CHANNELS)
    parser.add_argument("--audio-codec", choices=["ia4m", "pc16"], default="ia4m", help="audio mux format: ia4m is small ADPCM mono, pc16 is larger high-quality PCM")
    parser.add_argument("--audio-offset-ms", type=int, default=0, help="shift audio relative to video by this many ms; positive delays audio (prepends silence), negative advances it (trims samples from the start)")
    parser.add_argument("--keyint", type=int, default=DEFAULT_KEYINT)
    parser.add_argument("--qp", type=int, default=DEFAULT_QP)
    parser.add_argument("--c-qp-offset", type=int, default=DEFAULT_C_QP_OFFSET)
    parser.add_argument("--lambda", dest="lambda_value", type=float, default=DEFAULT_LAMBDA)
    parser.add_argument("--y-skip", type=int, default=DEFAULT_Y_SKIP)
    parser.add_argument("--c-skip", type=int, default=DEFAULT_C_SKIP)
    parser.add_argument("--y-delta", type=int, default=DEFAULT_Y_DELTA)
    parser.add_argument("--c-delta", type=int, default=DEFAULT_C_DELTA)
    parser.add_argument("--mv-range", type=int, default=DEFAULT_MV_RANGE)
    parser.add_argument(
        "--motion-search",
        choices=["full", "diamond", "fast", "hybrid"],
        default="full",
        help="per-block motion search algorithm: full = exhaustive, slowest, best quality "
             "(default, unchanged behavior); diamond = experimental iterative search, faster, "
             "small quality/size risk; fast = experimental, more speed-biased, larger quality/size risk; "
             "hybrid = experimental, two-seed diamond plus capped local refine, aims for diamond-ish "
             "speed with less of its quality/size cost",
    )
    parser.add_argument("--keep", type=int, default=DEFAULT_KEEP, choices=[4, 8, 16], help="transform coefficients kept per 4x4 quadrant: 16=HD detail (default), 4=small legacy files")
    parser.add_argument("--jobs", type=int, default=DEFAULT_JOBS, help=f"parallel encoder workers, default {DEFAULT_JOBS}")
    parser.add_argument("--chunk-frames", type=int, default=DEFAULT_CHUNK_FRAMES, help=f"frames per streaming worker chunk, default {DEFAULT_CHUNK_FRAMES}")
    parser.add_argument(
        "--max-video-packet-kb",
        type=int,
        default=0,
        help="if set, any encoded chunk whose largest video packet exceeds this size (KB) "
             "is retried at a higher QP (bounded retries, keeps the smallest-packet attempt); "
             "0 disables (default). Retry granularity is a whole --chunk-frames chunk, not a "
             "single frame -- pair with a smaller --chunk-frames for finer-grained retries.",
    )
    parser.add_argument(
        "--warm-start-chunks",
        action="store_true",
        help="experimental: feed each chunk after the first its predecessor's actual "
             "reconstructed last frame as a throwaway warm-up, so the chunk's real first "
             "frame can use ordinary QP/keep-responsive P-frame coding instead of being "
             "forced into a SOLID/RAW-only keyframe at every chunk boundary. Serializes "
             "chunk encoding (no parallelism across chunks) since each needs its "
             "predecessor's finished reconstruction first. Off by default.",
    )
    parser.add_argument("--no-deploy", action="store_true", help="skip SD card deployment")
    parser.add_argument("--m2y2", action="store_true", help="range-code video to the smaller M2Y2 codec (lossless, approximately 20 percent smaller)")
    parser.add_argument("--no-seek-index", action="store_true", help="do not generate .idx sidecar seek index")
    parser.add_argument("--no-embedded-index", action="store_true", help="do not append embedded seek index payload/footer into output .mivf")
    parser.add_argument("--report-packet-sizes", action="store_true", help="print per-video-packet size histogram after encoding")
    parser.add_argument("--profile", choices=["default", "3ds-fast"], default="default", help="encoder quality/speed preset: default for quality, 3ds-fast for smaller packets and smoother 3DS playback")
    parser.add_argument("--seek-index-sidecar", default=None, help="optional explicit path for generated .idx sidecar")
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    settings = EncodeSettings(
        width=args.width,
        height=args.height,
        fps=args.fps,
        audio_rate=args.audio_rate,
        audio_channels=args.audio_channels,
        audio_codec=args.audio_codec,
        audio_offset_ms=args.audio_offset_ms,
        keyint=args.keyint,
        qp=args.qp,
        c_qp_offset=args.c_qp_offset,
        lambda_value=args.lambda_value,
        y_skip=args.y_skip,
        c_skip=args.c_skip,
        y_delta=args.y_delta,
        c_delta=args.c_delta,
        mv_range=args.mv_range,
        keep=args.keep,
        jobs=args.jobs,
        chunk_frames=args.chunk_frames,
        max_video_packet_kb=args.max_video_packet_kb,
        warm_start_chunks=args.warm_start_chunks,
        motion_search=args.motion_search,
    )

    if settings.max_video_packet_kb < 0:
        raise SystemExit("--max-video-packet-kb must be >= 0")

    # ---- profile presets ----
    # Apply preset defaults, but let explicit CLI flags override by
    # only changing fields that the user did NOT explicitly set.
    if args.profile == "3ds-fast":
        if args.qp == DEFAULT_QP:
            settings.qp = 42
        if args.lambda_value == DEFAULT_LAMBDA:
            settings.lambda_value = 35.0
        if args.keep == DEFAULT_KEEP:
            settings.keep = 8
        if args.y_skip == DEFAULT_Y_SKIP:
            settings.y_skip = 30
        if args.c_skip == DEFAULT_C_SKIP:
            settings.c_skip = 36
        if args.y_delta == DEFAULT_Y_DELTA:
            settings.y_delta = 32
        if args.c_delta == DEFAULT_C_DELTA:
            settings.c_delta = 40
        if args.c_qp_offset == DEFAULT_C_QP_OFFSET:
            settings.c_qp_offset = 8
        # mv_range stays at 4 (already minimal)

    # Print effective encode settings
    print("============================================================")
    print("ENCODE SETTINGS")
    print("============================================================")
    print(f"  profile:         {args.profile}")
    print(f"  resolution:      {settings.width}x{settings.height}")
    print(f"  fps:             {settings.fps}")
    print(f"  keyint:          {settings.keyint}")
    print(f"  qp:              {settings.qp}")
    print(f"  c_qp_offset:     {settings.c_qp_offset}")
    print(f"  lambda:          {settings.lambda_value:.1f}")
    print(f"  keep:            {settings.keep}")
    print(f"  y_skip:          {settings.y_skip}")
    print(f"  c_skip:          {settings.c_skip}")
    print(f"  y_delta:         {settings.y_delta}")
    print(f"  c_delta:         {settings.c_delta}")
    print(f"  mv_range:        {settings.mv_range}")
    motion_search_desc = settings.motion_search if settings.motion_search == "full" else f"{settings.motion_search} (experimental)"
    print(f"  motion_search:   {motion_search_desc}")
    print(f"  audio:           {settings.audio_codec.upper()} {settings.audio_rate}Hz ch={settings.audio_channels}")
    cap_desc = f"{settings.max_video_packet_kb} KB" if settings.max_video_packet_kb > 0 else "disabled"
    print(f"  max_video_pkt:   {cap_desc}")
    print(f"  warm_start:      {'ENABLED (experimental)' if settings.warm_start_chunks else 'disabled'}")
    if args.profile == "3ds-fast":
        print()
        print("  Tip: use --report-packet-sizes to check packet size distribution.")
    print("============================================================")
    print()

    input_path = Path(args.input)
    output_path = Path(args.output)

    seek_index_sidecar = Path(args.seek_index_sidecar) if args.seek_index_sidecar else None

    if input_path.is_dir():
        if seek_index_sidecar is not None:
            raise SystemExit("--seek-index-sidecar is only supported for single-file encode")
        encode_folder(
            input_path,
            output_path,
            settings,
            make_m2y2=args.m2y2,
            make_seek_index=not args.no_seek_index,
            make_embedded_index=not args.no_embedded_index,
            report_packet_sizes=args.report_packet_sizes,
        )
        return

    if not input_path.exists():
        raise SystemExit(f"Input file not found: {input_path}")

    encode_one(
        input_path,
        output_path,
        settings,
        deploy_sd=not args.no_deploy,
        make_m2y2=args.m2y2,
        make_seek_index=not args.no_seek_index,
        make_embedded_index=not args.no_embedded_index,
        report_packet_sizes=args.report_packet_sizes,
        seek_index_sidecar=seek_index_sidecar,
    )


if __name__ == "__main__":
    main()
