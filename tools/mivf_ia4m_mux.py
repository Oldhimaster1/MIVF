#!/usr/bin/env python3
from pathlib import Path
import argparse, struct, subprocess, tempfile, zlib

HEADER_SIZE = 64
STREAM_DESC_SIZE = 32
PAGE_HEADER_SIZE = 32
PACKET_HEADER_SIZE = 16
PAGE_CRC = 1
PAGE_HAS_KEYFRAME = 2

def le16(b, o): return b[o] | (b[o+1] << 8)
def le32(b, o): return b[o] | (b[o+1] << 8) | (b[o+2] << 16) | (b[o+3] << 24)
def le64(b, o): return le32(b, o) | (le32(b, o+4) << 32)

def wr_header(template, streams, first):
    b = bytearray(template[:64])
    b[0:4] = b"MIVF"
    b[28:32] = struct.pack("<I", streams)
    b[36:44] = struct.pack("<Q", first)
    return bytes(b)

def make_stream_desc(sid, stype, codec, w, h, fpsn, fpsd, extra):
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
        len(extra)
    ) + extra

IMA_INDEX_TABLE = [
    -1,-1,-1,-1, 2,4,6,8,
    -1,-1,-1,-1, 2,4,6,8
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
    32767
]

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

    for s in samples[1:]:
        nib, predictor, index = ima_encode_nibble(s, predictor, index)
        nibbles.append(nib)

    adpcm = bytearray()

    for i in range(0, len(nibbles), 2):
        lo = nibbles[i] & 15
        hi = nibbles[i+1] & 15 if i + 1 < len(nibbles) else 0
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

def extract_pcm16(src, rate, channels):
    tmp = Path(tempfile.mkdtemp(prefix="ia4m_")) / "audio.s16le"
    cmd = [
        "ffmpeg", "-y",
        "-i", src,
        "-vn",
        "-ac", str(channels),
        "-ar", str(rate),
        "-f", "s16le",
        str(tmp)
    ]
    print("Running:", " ".join(cmd))
    subprocess.check_call(cmd)
    return tmp

def read_pcm16(path):
    data = Path(path).read_bytes()
    if len(data) & 1:
        data = data[:-1]
    if not data:
        return []
    return list(struct.unpack("<" + "h" * (len(data) // 2), data))

def mux(video_path, audio_src, out_path, rate, channels):
    if channels != 1:
        raise SystemExit("IA4M mux currently supports mono only")

    data = Path(video_path).read_bytes()

    if data[:4] != b"MIVF":
        raise SystemExit("not MIVF")

    streams = le32(data, 28)
    first_old = le64(data, 36)

    if streams != 1:
        raise SystemExit(f"expected video-only MIVF with 1 stream, got {streams}")

    desc0 = data[64:first_old]

    fpsn = le16(desc0, 20)
    fpsd = le16(desc0, 22)

    if fpsn == 0:
        fpsn = 30
    if fpsd == 0:
        fpsd = 1

    samples_per_frame = rate * fpsd // fpsn

    extra = b"IA4M" + struct.pack("<IHHI", rate, channels, samples_per_frame, 0)
    assert len(extra) == 16, len(extra)

    desc1 = make_stream_desc(
        1,
        2,
        b"IA4M",
        rate,
        channels,
        samples_per_frame,
        1,
        extra
    )

    first_new = HEADER_SIZE + len(desc0) + len(desc1)

    pcm_path = extract_pcm16(audio_src, rate, channels)
    pcm = read_pcm16(pcm_path)

    pages = bytearray()
    off = first_old
    frame_no = 0

    while off + PAGE_HEADER_SIZE <= len(data):
        if data[off:off+2] != b"MP":
            break

        page_flags = data[off+3]
        page_seq = le32(data, off+4)
        page_pts = le64(data, off+8)
        payload_size = le32(data, off+16)
        packets = le16(data, off+20)
        reserved = le16(data, off+22)

        payload = data[off+PAGE_HEADER_SIZE:off+PAGE_HEADER_SIZE+payload_size]

        start = frame_no * samples_per_frame
        end = start + samples_per_frame
        samples = pcm[start:end]

        if len(samples) < samples_per_frame:
            samples += [0] * (samples_per_frame - len(samples))

        abody = encode_ia4m_packet(samples, frame_no)

        apkt = struct.pack(
            "<BBHIII",
            1,
            0,
            PACKET_HEADER_SIZE,
            0,
            len(abody),
            frame_no
        ) + abody

        new_payload = payload + apkt
        crc = zlib.crc32(new_payload) & 0xffffffff

        page = struct.pack(
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
            0
        )

        pages += page + new_payload

        off += PAGE_HEADER_SIZE + payload_size
        frame_no += 1

    header = wr_header(data[:64], 2, first_new)
    Path(out_path).write_bytes(header + desc0 + desc1 + pages)

    print(f"WROTE {out_path}")
    print(f"frames={frame_no} audio={rate}Hz channels={channels} samples/frame={samples_per_frame} bytes={Path(out_path).stat().st_size}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("video_mivf")
    ap.add_argument("audio_src")
    ap.add_argument("out_mivf")
    ap.add_argument("--rate", type=int, default=11040)
    ap.add_argument("--channels", type=int, default=1)
    args = ap.parse_args()

    mux(args.video_mivf, args.audio_src, args.out_mivf, args.rate, args.channels)

if __name__ == "__main__":
    main()
