#!/usr/bin/env python3
"""
miv1p_mt_wrapper.py — multithread/multiprocess wrapper for the MIV1P converter.

This is a speed wrapper around miv1p_tools_fast.py. It splits the image sequence
into independent GOP chunks, runs several miv1p_tools_fast.py encoders in
parallel, then merges the resulting MIVF pages into one playable file.

Why this works:
  - M1P0 prediction only depends on previous frames inside a GOP.
  - Starting each chunk with a fresh I-frame makes chunks independently encodable.
  - The final file is stitched back into one MIVF stream.

Tradeoff:
  - More chunks = more I-frames = slightly larger file.
  - Usually worth it for much faster testing.

Example:
  python miv1p_mt_wrapper.py output_400_mt.mivf frames_miv1_400/frame_%04d.png \
    --frames 120 --width 400 --height 240 --fps 30 --jobs 4 --chunk 30 \
    --keyint 30 --skip 1 --mvskip 1 --delta 1 --mvdelta 1 --mvradius 2 --mvstep 1 --solid 0 --two 0
"""
from __future__ import annotations

import argparse
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

HEADER_SIZE = 64
STREAM_DESC_SIZE = 32
PAGE_HEADER_SIZE = 32
PACKET_HEADER_SIZE = 16
PAGE_CRC = 1
PAGE_HAS_KEYFRAME = 2
PKT_KEYFRAME = 1
PKT_FRAME_START = 2
PKT_FRAME_END = 4


def read_payloads(path: Path):
    data = path.read_bytes()
    if data[:4] != b'MIVF':
        raise ValueError(f'{path}: not MIVF')
    codec = data[68:72]
    w, h, fpsn, fpsd = struct.unpack_from('<HHHH', data, 64 + 0x10)
    extra_size = struct.unpack_from('<H', data, 64 + 0x1e)[0]
    extra = data[96:96+extra_size]
    first = struct.unpack_from('<Q', data, 0x24)[0]
    payloads = []
    off = first
    while off + PAGE_HEADER_SIZE <= len(data):
        if data[off:off+2] != b'MP':
            raise ValueError(f'{path}: bad page magic at {off:x}')
        _, _, flags, page_no, ts, psz, pc, _, crc, _ = struct.unpack_from('<2sBBIQIHHII', data, off)
        page_payload = data[off+PAGE_HEADER_SIZE:off+PAGE_HEADER_SIZE+psz]
        if flags & PAGE_CRC:
            calc = zlib.crc32(page_payload) & 0xffffffff
            if calc != crc:
                raise ValueError(f'{path}: CRC mismatch page {page_no}')
        po = 0
        for _ in range(pc):
            sid, pflags, hsize, pts, psize, frame_no = struct.unpack_from('<BBHIII', page_payload, po)
            body = page_payload[po+hsize:po+hsize+psize]
            payloads.append(body)
            po += hsize + psize
        off += PAGE_HEADER_SIZE + psz
    return codec, w, h, fpsn, fpsd, extra, payloads


def write_combined(path: Path, w: int, h: int, fps: int, payloads: list[bytes], extra: bytes):
    first = HEADER_SIZE + STREAM_DESC_SIZE + len(extra)
    duration = len(payloads) * 30000 // fps
    header = struct.pack(
        '<4sHHIIIQIIQQIII',
        b'MIVF', 0, 8, HEADER_SIZE, 1, 30000, duration,
        1, 4096, first, 0, 0, 0, 0
    )
    stream = struct.pack(
        '<BBH4sIIHHHHIBBH',
        0, 1, STREAM_DESC_SIZE + len(extra), b'MIV1',
        1, fps, w, h, fps, 1,
        0, 0, 0, len(extra)
    )
    pages = bytearray()
    for i, body in enumerate(payloads):
        pkt = struct.pack('<BBHIII', 0, PKT_KEYFRAME | PKT_FRAME_START | PKT_FRAME_END, PACKET_HEADER_SIZE, 0, len(body), i) + body
        crc = zlib.crc32(pkt) & 0xffffffff
        page = struct.pack('<2sBBIQIHHII', b'MP', PAGE_HEADER_SIZE, PAGE_CRC | PAGE_HAS_KEYFRAME, i, i * 30000 // fps, len(pkt), 1, 0, crc, 0)
        pages += page + pkt
    path.write_bytes(header + stream + extra + pages)


def run_chunk(args, chunk_index, start_frame, count, temp_dir: Path, tool_path: Path):
    out = temp_dir / f'chunk_{chunk_index:04d}.mivf'
    cmd = [
        sys.executable, str(tool_path), 'images', str(out), args.pattern,
        '--codec', 'miv1p',
        '--start', str(start_frame),
        '--frames', str(count),
        '--width', str(args.width),
        '--height', str(args.height),
        '--fps', str(args.fps),
        '--keyint', str(args.keyint),
        '--skip', str(args.skip),
        '--mvskip', str(args.mvskip),
        '--delta', str(args.delta),
        '--mvdelta', str(args.mvdelta),
        '--mvradius', str(args.mvradius),
        '--mvstep', str(args.mvstep),
        '--solid', str(args.solid),
        '--two', str(args.two),
        '--progress', '0',
    ]
    t0 = time.time()
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    dt = time.time() - t0
    if proc.returncode != 0:
        raise RuntimeError(f'chunk {chunk_index} failed:\n{proc.stdout}')
    return chunk_index, out, dt, proc.stdout


def validate_basic(path: Path):
    codec, w, h, fpsn, fpsd, extra, payloads = read_payloads(path)
    print(f'MIVF codec={codec.decode()} {w}x{h} fps={fpsn}/{fpsd} frames={len(payloads)} bytes={path.stat().st_size}')
    print('OK basic validation')


def main():
    ap = argparse.ArgumentParser(description='Parallel wrapper for miv1p_tools_fast.py')
    ap.add_argument('output')
    ap.add_argument('pattern', help='printf pattern, e.g. frames/frame_%%04d.png')
    ap.add_argument('--start', type=int, default=1)
    ap.add_argument('--frames', type=int, required=True)
    ap.add_argument('--width', type=int, required=True)
    ap.add_argument('--height', type=int, required=True)
    ap.add_argument('--fps', type=int, default=30)
    ap.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 4) // 2))
    ap.add_argument('--chunk', type=int, default=30, help='frames per independent chunk/GOP; 30 is a good default')
    ap.add_argument('--tool', default='miv1p_tools_fast.py', help='path to miv1p_tools_fast.py')
    ap.add_argument('--keep-temp', action='store_true')

    # Forwarded encoder flags.
    ap.add_argument('--keyint', type=int, default=30)
    ap.add_argument('--skip', type=int, default=1)
    ap.add_argument('--mvskip', type=int, default=1)
    ap.add_argument('--delta', type=int, default=1)
    ap.add_argument('--mvdelta', type=int, default=1)
    ap.add_argument('--mvradius', type=int, default=2)
    ap.add_argument('--mvstep', type=int, default=1)
    ap.add_argument('--solid', type=int, default=0)
    ap.add_argument('--two', type=int, default=0)
    args = ap.parse_args()

    tool_path = Path(args.tool)
    if not tool_path.exists():
        same_dir = Path(__file__).resolve().parent / args.tool
        if same_dir.exists():
            tool_path = same_dir
        else:
            raise SystemExit(f'ERROR: could not find {args.tool}. Put miv1p_tools_fast.py next to this wrapper or pass --tool PATH')

    chunks = []
    remaining = args.frames
    start = args.start
    idx = 0
    while remaining > 0:
        count = min(args.chunk, remaining)
        chunks.append((idx, start, count))
        start += count
        remaining -= count
        idx += 1

    print(f'Parallel MIV1P encode: frames={args.frames}, chunks={len(chunks)}, jobs={args.jobs}, chunk={args.chunk}')
    print('NOTE: each chunk starts with its own I-frame; smaller chunks encode faster but can increase output size.')
    t_all = time.time()

    temp_root = Path(tempfile.mkdtemp(prefix='miv1p_mt_'))
    results = []
    try:
        with ThreadPoolExecutor(max_workers=args.jobs) as ex:
            futs = [ex.submit(run_chunk, args, idx, st, cnt, temp_root, tool_path) for idx, st, cnt in chunks]
            for fut in as_completed(futs):
                idx, out, dt, log = fut.result()
                print(f'chunk {idx:04d} done in {dt:.2f}s -> {out.name}')
                # Show compact useful lines from child encoder.
                for line in log.splitlines():
                    if line.startswith('MIVF codec=') or line.startswith('STATS ') or line.startswith('ENCODE_TIME'):
                        print(f'  {line}')
                results.append((idx, out))
        results.sort()

        all_payloads = []
        extra = None
        got_w = got_h = got_fps = None
        for idx, out in results:
            codec, w, h, fpsn, fpsd, ex, payloads = read_payloads(out)
            if codec != b'MIV1':
                raise RuntimeError(f'{out}: expected MIV1, got {codec}')
            if extra is None:
                extra = ex
                got_w, got_h, got_fps = w, h, fpsn
            elif (w, h, fpsn) != (got_w, got_h, got_fps):
                raise RuntimeError('chunk format mismatch')
            all_payloads.extend(payloads)

        write_combined(Path(args.output), got_w, got_h, got_fps, all_payloads, extra)
        validate_basic(Path(args.output))
        print(f'TOTAL_TIME {time.time() - t_all:.2f}s')
        print(f'WROTE {args.output}')
    finally:
        if args.keep_temp:
            print(f'Keeping temp folder: {temp_root}')
        else:
            shutil.rmtree(temp_root, ignore_errors=True)


if __name__ == '__main__':
    main()
