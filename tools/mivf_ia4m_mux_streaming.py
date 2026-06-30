#!/usr/bin/env python3
import argparse
import struct
import subprocess
import sys
import zlib
from pathlib import Path

HEADER_SIZE = 64
PAGE_HEADER_SIZE = 32
PACKET_HEADER_SIZE = 16
STREAM_DESC_BASE_SIZE = 32

IMA_INDEX_TABLE = [
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
]

IMA_STEP_TABLE = [
        7,     8,     9,    10,    11,    12,    13,    14,
       16,    17,    19,    21,    23,    25,    28,    31,
       34,    37,    41,    45,    50,    55,    60,    66,
       73,    80,    88,    97,   107,   118,   130,   143,
      157,   173,   190,   209,   230,   253,   279,   307,
      337,   371,   408,   449,   494,   544,   598,   658,
      724,   796,   876,   963,  1060,  1166,  1282,  1411,
     1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,
     3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,
     7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767,
]

def read_exact(f, n):
    b = f.read(n)
    if len(b) != n:
        raise EOFError(f"wanted {n} bytes, got {len(b)}")
    return b

def le16(b, o):
    return b[o] | (b[o + 1] << 8)

def le32(b, o):
    return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)

def le64(b, o):
    return le32(b, o) | (le32(b, o + 4) << 32)

def wr_header(template, streams, first):
    b = bytearray(template[:HEADER_SIZE])
    if b[:4] != b"MIVF":
        raise SystemExit("input is not MIVF")
    struct.pack_into("<I", b, 12, streams)
    struct.pack_into("<Q", b, 36, first)
    return bytes(b)

def make_stream_desc(sid, stype, codec, w, h, fpsn, fpsd, extra):
    return struct.pack(
        "<BBH4sIIHHHHIBBH",
        sid,
        stype,
        STREAM_DESC_BASE_SIZE + len(extra),
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

def clamp_s16(v):
    return max(-32768, min(32767, int(v)))

def ima_encode_nibble(sample, predictor, index):
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

def encode_ia4m_packet(samples, frame_no):
    if not samples:
        samples = [0]
    ns = len(samples)
    predictor0 = clamp_s16(samples[0])
    predictor = predictor0
    index0 = 0
    index = index0
    nibbles = []
    for sample in samples[1:]:
        nib, predictor, index = ima_encode_nibble(sample, predictor, index)
        nibbles.append(nib)
    adpcm = bytearray()
    for i in range(0, len(nibbles), 2):
        lo = nibbles[i] & 15
        hi = nibbles[i + 1] & 15 if i + 1 < len(nibbles) else 0
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

def start_audio_pipe(audio_src, rate, channels, ss=None, duration=None):
    cmd = [
        "ffmpeg",
        "-hide_banner",
        "-loglevel",
        "error",
    ]

    if ss:
        cmd += ["-ss", str(ss)]

    cmd += [
        "-i",
        audio_src,
    ]

    if duration:
        cmd += ["-t", str(duration)]

    cmd += [
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
        "-",
    ]

    return subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

def read_audio_samples(proc, samples_per_frame, channels):
    if channels != 1:
        raise SystemExit("streaming IA4M mux currently supports mono only")
    bytes_needed = samples_per_frame * channels * 2
    data = proc.stdout.read(bytes_needed) if proc.stdout else b""
    if len(data) < bytes_needed:
        data += b"\x00" * (bytes_needed - len(data))
    return list(struct.unpack("<" + "h" * samples_per_frame, data[:samples_per_frame * 2]))

def mux_streaming(video_path, audio_src, out_path, rate, channels, ss=None, duration=None):
    if channels != 1:
        raise SystemExit("IA4M streaming mux currently supports mono only")

    with open(video_path, "rb") as vf, open(out_path, "wb") as of:
        header = read_exact(vf, HEADER_SIZE)
        if header[:4] != b"MIVF":
            raise SystemExit("input video is not MIVF")

        stream_count = le32(header, 12)
        first_old = le64(header, 36)

        if stream_count != 1:
            raise SystemExit(f"expected video-only MIVF with 1 stream, got {stream_count}")

        desc0_len = first_old - HEADER_SIZE
        if desc0_len <= 0 or desc0_len > 4096:
            raise SystemExit(f"invalid descriptor length: {desc0_len}")

        desc0 = read_exact(vf, desc0_len)
        fpsn = le16(desc0, 20)
        fpsd = le16(desc0, 22)

        if fpsn == 0:
            fpsn = 30
        if fpsd == 0:
            fpsd = 1

        samples_per_frame = rate * fpsd // fpsn
        if samples_per_frame <= 0:
            raise SystemExit("bad samples_per_frame")

        extra = b"IA4M" + struct.pack("<IHHI", rate, channels, samples_per_frame, 0)
        desc1 = make_stream_desc(1, 2, b"IA4M", rate, channels, samples_per_frame, 1, extra)
        first_new = HEADER_SIZE + len(desc0) + len(desc1)

        of.write(wr_header(header, 2, first_new))
        of.write(desc0)
        of.write(desc1)

        aproc = start_audio_pipe(audio_src, rate, channels, ss=ss, duration=duration)
        frame_no = 0

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

                payload = read_exact(vf, payload_size)
                samples = read_audio_samples(aproc, samples_per_frame, channels)
                abody = encode_ia4m_packet(samples, frame_no)

                apkt = struct.pack(
                    "<BBHIII",
                    1,
                    0,
                    PACKET_HEADER_SIZE,
                    0,
                    len(abody),
                    frame_no,
                ) + abody

                new_payload = payload + apkt
                crc = zlib.crc32(new_payload) & 0xFFFFFFFF

                new_page_header = struct.pack(
                    "<2sBBIQIHHII",
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

                of.write(new_page_header)
                of.write(new_payload)

                frame_no += 1
                if frame_no % 300 == 0:
                    print(f"muxed {frame_no} frames", flush=True)
        finally:
            if aproc.stdout:
                aproc.stdout.close()
            stderr = aproc.stderr.read().decode("utf-8", errors="replace") if aproc.stderr else ""
            rc = aproc.wait()
            if rc != 0:
                # If all video pages were already muxed, FFmpeg may report a
                # broken/closed output pipe after the muxer has consumed enough
                # audio. Treat this as non-fatal for completed muxes.
                if frame_no <= 0:
                    print(stderr, file=sys.stderr)
                    raise SystemExit(f"ffmpeg audio pipe failed with exit code {rc}")
                else:
                    if stderr.strip():
                        print("NOTE: ffmpeg audio pipe ended nonzero after mux completion:", file=sys.stderr)
                        print(stderr, file=sys.stderr)

    print(f"WROTE {out_path}")
    print(f"frames={frame_no} audio={rate}Hz channels={channels} samples/frame={samples_per_frame} bytes={Path(out_path).stat().st_size}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("video_mivf")
    ap.add_argument("audio_src")
    ap.add_argument("out_mivf")
    ap.add_argument("--rate", type=int, default=11040)
    ap.add_argument("--channels", type=int, default=1)
    ap.add_argument("--ss", default=None, help="optional audio start offset, e.g. 00:10:00")
    ap.add_argument("--duration", default=None, help="optional audio duration, e.g. 00:01:00")
    args = ap.parse_args()
    mux_streaming(args.video_mivf, args.audio_src, args.out_mivf, args.rate, args.channels, ss=args.ss, duration=args.duration)

if __name__ == "__main__":
    main()
