#!/usr/bin/env python3
"""
MIVF Phase 3 v0.8 FAST encoder tools.

This is a faster drop-in replacement for miv1p_tools.py.
It writes the same M1P0 bitstream/profile, so the existing Phase 3 v0.7 player can decode it.

Main speed changes:
  - precomputes RGB565 -> luma table once;
  - uses luma-only motion search instead of repeated RGB888 conversion;
  - avoids building candidate RGB blocks during motion search;
  - uses early-exit SAD/MAD for candidate rejection;
  - keeps exact RGB block reconstruction for the final selected mode.

CLI is intentionally compatible with v0.7 for --codec miv1p.
"""
from __future__ import annotations
import argparse, glob, struct, zlib, time
from pathlib import Path

HEADER_SIZE = 64
STREAM_DESC_SIZE = 32
PAGE_HEADER_SIZE = 32
PACKET_HEADER_SIZE = 16
PKT_KEYFRAME = 1
PKT_FRAME_START = 2
PKT_FRAME_END = 4
PAGE_CRC = 1
PAGE_HAS_KEYFRAME = 2

M_SKIP = 0
M_RAW = 1
M_TWO = 2
M_SOLID = 3
M_AVGDELTA = 4
M_MVDELTA = 5
M_MVCOPY = 6

# RGB565 -> luma lookup. This is a big win for motion search.
LUMA = [0] * 65536
for v in range(65536):
    r5 = (v >> 11) & 31
    g6 = (v >> 5) & 63
    b5 = v & 31
    r = (r5 << 3) | (r5 >> 2)
    g = (g6 << 2) | (g6 >> 4)
    b = (b5 << 3) | (b5 >> 2)
    LUMA[v] = (77 * r + 150 * g + 29 * b) >> 8


def clamp(x, lo=0, hi=255):
    return lo if x < lo else hi if x > hi else int(x)


def clamp_s8(x):
    return -128 if x < -128 else 127 if x > 127 else int(x)


def rgb565(r, g, b):
    return ((clamp(r) >> 3) << 11) | ((clamp(g) >> 2) << 5) | (clamp(b) >> 3)


def rgb888(v):
    r5 = (v >> 11) & 31
    g6 = (v >> 5) & 63
    b5 = v & 31
    return ((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2))


def add_delta565(v, dr, dg, db):
    r, g, b = rgb888(v)
    return rgb565(r + dr, g + dg, b + db)


def frame_to_pixels(frame_bytes):
    return [p for (p,) in struct.iter_unpack('<H', frame_bytes)]


def pixels_to_frame(px):
    return struct.pack('<' + 'H' * len(px), *px)


def make_luma_frame(px):
    return [LUMA[p] for p in px]


def get_block(px, w, bx, by):
    out = []
    x0 = bx * 8
    y0 = by * 8
    for y in range(8):
        s = (y0 + y) * w + x0
        out.extend(px[s:s+8])
    return out


def get_block_xy(px, w, x0, y0):
    out = []
    for y in range(8):
        s = (y0 + y) * w + x0
        out.extend(px[s:s+8])
    return out


def put_block(px, w, bx, by, block):
    x0 = bx * 8
    y0 = by * 8
    k = 0
    for y in range(8):
        s = (y0 + y) * w + x0
        px[s:s+8] = block[k:k+8]
        k += 8


def luma_block_mad_same(curr_luma, prev_luma, w, bx, by, early_limit=999999):
    x0 = bx * 8
    y0 = by * 8
    total = 0
    cutoff = early_limit * 64
    for y in range(8):
        cs = (y0 + y) * w + x0
        ps = cs
        for x in range(8):
            total += abs(curr_luma[cs+x] - prev_luma[ps+x])
        if total > cutoff:
            return total / 64.0
    return total / 64.0


def luma_block_mad_xy(curr_luma, prev_luma, w, bx, by, x1, y1, early_limit=999999):
    x0 = bx * 8
    y0 = by * 8
    total = 0
    cutoff = early_limit * 64
    for y in range(8):
        cs = (y0 + y) * w + x0
        ps = (y1 + y) * w + x1
        for x in range(8):
            total += abs(curr_luma[cs+x] - prev_luma[ps+x])
        if total > cutoff:
            return total / 64.0
    return total / 64.0


def motion_search_luma(curr_luma, prev_luma, w, h, bx, by, radius, step, start_best):
    base_x = bx * 8
    base_y = by * 8
    best_dx = 0
    best_dy = 0
    best = start_best
    if radius <= 0:
        return best_dx, best_dy, best
    for dy in range(-radius, radius + 1, step):
        y1 = base_y + dy
        if y1 < 0 or y1 + 8 > h:
            continue
        for dx in range(-radius, radius + 1, step):
            if dx == 0 and dy == 0:
                continue
            x1 = base_x + dx
            if x1 < 0 or x1 + 8 > w:
                continue
            cost = luma_block_mad_xy(curr_luma, prev_luma, w, bx, by, x1, y1, best)
            # Small motion penalty to avoid noisy motion vectors.
            cost += (abs(dx) + abs(dy)) * 0.08
            if cost < best:
                best = cost
                best_dx = dx
                best_dy = dy
    return best_dx, best_dy, best


def color_dist2(a, b):
    ar, ag, ab = rgb888(a)
    br, bg, bb = rgb888(b)
    return (ar-br)*(ar-br) + (ag-bg)*(ag-bg) + (ab-bb)*(ab-bb)


def avg_color565(block):
    rs = gs = bs = 0
    for p in block:
        r, g, b = rgb888(p)
        rs += r
        gs += g
        bs += b
    return rgb565(rs // 64, gs // 64, bs // 64)


def avg_delta(curr, pred):
    sr = sg = sb = 0
    for c, p in zip(curr, pred):
        cr, cg, cb = rgb888(c)
        pr, pg, pb = rgb888(p)
        sr += cr - pr
        sg += cg - pg
        sb += cb - pb
    return clamp_s8(round(sr / 64)), clamp_s8(round(sg / 64)), clamp_s8(round(sb / 64))


def apply_delta_block(pred, dr, dg, db):
    return [add_delta565(p, dr, dg, db) for p in pred]


def rgb_mad(curr, pred):
    total = 0
    for c, p in zip(curr, pred):
        cr, cg, cb = rgb888(c)
        pr, pg, pb = rgb888(p)
        total += abs(cr-pr) + abs(cg-pg) + abs(cb-pb)
    return total / 192.0


def encode_intra_block(block, solid_threshold, two_threshold):
    avg = avg_color565(block)
    maxd = max(color_dist2(p, avg) for p in block)
    if maxd <= solid_threshold * solid_threshold:
        return bytes([M_SOLID]) + struct.pack('<H', avg), [avg] * 64, 'solid'

    dark = min(block, key=lambda p: LUMA[p])
    light = max(block, key=lambda p: LUMA[p])
    th = (LUMA[dark] + LUMA[light]) // 2
    bits = bytearray(8)
    rec = []
    err = 0
    for i, p in enumerate(block):
        choose = 1 if LUMA[p] >= th else 0
        ref = light if choose else dark
        if choose:
            bits[i >> 3] |= 1 << (i & 7)
        rec.append(ref)
        err += color_dist2(p, ref)
    if err // 64 <= two_threshold * two_threshold:
        return bytes([M_TWO]) + struct.pack('<HH', dark, light) + bytes(bits), rec, 'two'

    return bytes([M_RAW]) + pixels_to_frame(block), block[:], 'raw'


def encode_frame(frame_px, frame_luma, w, h, frame_no, prev_px, prev_luma, args):
    blocks_x = w // 8
    blocks_y = h // 8
    is_i = prev_px is None or frame_no % args.keyint == 0
    out = bytearray(b'M1P0')
    out += struct.pack('<HHIBBBBI', w, h, frame_no, 1 if is_i else 2, 8, 8, 0, blocks_x * blocks_y)
    rec = [0] * (w * h)
    rec_luma = [0] * (w * h)
    stats = {'I': 1 if is_i else 0, 'P': 0 if is_i else 1, 'skip': 0, 'mv': 0, 'avg': 0, 'mvavg': 0, 'raw': 0, 'two': 0, 'solid': 0}

    for by in range(blocks_y):
        for bx in range(blocks_x):
            block = get_block(frame_px, w, bx, by)
            chosen = None
            block_rec = None
            stat = None

            if not is_i and prev_px is not None:
                same_luma = luma_block_mad_same(frame_luma, prev_luma, w, bx, by, args.skip)
                if same_luma <= args.skip:
                    chosen = bytes([M_SKIP])
                    block_rec = get_block(prev_px, w, bx, by)
                    stat = 'skip'
                else:
                    dx, dy, mv_luma = motion_search_luma(frame_luma, prev_luma, w, h, bx, by, args.mvradius, args.mvstep, same_luma)
                    if mv_luma <= args.mvskip:
                        chosen = bytes([M_MVCOPY]) + struct.pack('<bb', dx, dy)
                        block_rec = get_block_xy(prev_px, w, bx * 8 + dx, by * 8 + dy)
                        stat = 'mv'
                    else:
                        same = get_block(prev_px, w, bx, by)
                        dr, dg, db = avg_delta(block, same)
                        avg_rec = apply_delta_block(same, dr, dg, db)
                        if rgb_mad(block, avg_rec) <= args.delta:
                            chosen = bytes([M_AVGDELTA]) + struct.pack('<bbb', dr, dg, db)
                            block_rec = avg_rec
                            stat = 'avg'
                        else:
                            mv_block = get_block_xy(prev_px, w, bx * 8 + dx, by * 8 + dy)
                            dr, dg, db = avg_delta(block, mv_block)
                            mvavg_rec = apply_delta_block(mv_block, dr, dg, db)
                            if rgb_mad(block, mvavg_rec) <= args.mvdelta:
                                chosen = bytes([M_MVDELTA]) + struct.pack('<bbbbb', dx, dy, dr, dg, db)
                                block_rec = mvavg_rec
                                stat = 'mvavg'

            if chosen is None:
                chosen, block_rec, stat = encode_intra_block(block, args.solid, args.two)

            out += chosen
            stats[stat] += 1
            put_block(rec, w, bx, by, block_rec)
            # Fill reconstructed luma cheaply.
            lblock = [LUMA[p] for p in block_rec]
            put_block(rec_luma, w, bx, by, lblock)

    return bytes(out), rec, rec_luma, stats


def decode_frame(payload, prev_px):
    if payload[:4] != b'M1P0':
        raise ValueError('bad M1P0')
    w, h, frame_no, frame_type, bw, bh, flags, block_count = struct.unpack_from('<HHIBBBBI', payload, 4)
    blocks_x = w // 8
    blocks_y = h // 8
    px = [0] * (w * h)
    off = 20
    for by in range(blocks_y):
        for bx in range(blocks_x):
            mode = payload[off]
            off += 1
            if mode == M_SKIP:
                block = get_block(prev_px, w, bx, by)
            elif mode == M_RAW:
                block = list(struct.unpack_from('<64H', payload, off))
                off += 128
            elif mode == M_TWO:
                c0, c1 = struct.unpack_from('<HH', payload, off)
                bits = payload[off+4:off+12]
                off += 12
                block = [c1 if ((bits[i >> 3] >> (i & 7)) & 1) else c0 for i in range(64)]
            elif mode == M_SOLID:
                c, = struct.unpack_from('<H', payload, off)
                off += 2
                block = [c] * 64
            elif mode == M_AVGDELTA:
                dr, dg, db = struct.unpack_from('<bbb', payload, off)
                off += 3
                block = apply_delta_block(get_block(prev_px, w, bx, by), dr, dg, db)
            elif mode == M_MVCOPY:
                dx, dy = struct.unpack_from('<bb', payload, off)
                off += 2
                block = get_block_xy(prev_px, w, bx * 8 + dx, by * 8 + dy)
            elif mode == M_MVDELTA:
                dx, dy, dr, dg, db = struct.unpack_from('<bbbbb', payload, off)
                off += 5
                block = apply_delta_block(get_block_xy(prev_px, w, bx * 8 + dx, by * 8 + dy), dr, dg, db)
            else:
                raise ValueError(f'bad mode {mode}')
            put_block(px, w, bx, by, block)
    return pixels_to_frame(px), px, w, h, frame_no


def write_mivf(path, w, h, payloads, fps, codec=b'MIV1'):
    extra = b'M1P0' + struct.pack('<BBBBHHHH', 8, 8, 30, 0, 0, 0, 0, 0)
    first = HEADER_SIZE + STREAM_DESC_SIZE + len(extra)
    dur = len(payloads) * 30000 // fps
    header = struct.pack('<4sHHIIIQIIQQIII', b'MIVF', 0, 8, HEADER_SIZE, 1, 30000, dur, 1, 4096, first, 0, 0, 0, 0)
    stream = struct.pack('<BBH4sIIHHHHIBBH', 0, 1, STREAM_DESC_SIZE + len(extra), codec, 1, fps, w, h, fps, 1, 0, 0, 0, len(extra))
    pages = bytearray()
    for i, payload in enumerate(payloads):
        pkt = struct.pack('<BBHIII', 0, PKT_KEYFRAME|PKT_FRAME_START|PKT_FRAME_END, PACKET_HEADER_SIZE, 0, len(payload), i) + payload
        page = struct.pack('<2sBBIQIHHII', b'MP', PAGE_HEADER_SIZE, PAGE_CRC|PAGE_HAS_KEYFRAME, i, i*30000//fps, len(pkt), 1, 0, zlib.crc32(pkt)&0xffffffff, 0)
        pages += page + pkt
    Path(path).write_bytes(header + stream + extra + pages)


def load_images(pattern, start, count, width, height):
    from PIL import Image
    paths = [Path(pattern % i) for i in range(start, start+count)] if '%' in pattern else [Path(p) for p in sorted(glob.glob(pattern))[:count]]
    frames = []
    for p in paths:
        if not p.exists():
            raise SystemExit(f'missing {p}')
        img = Image.open(p).convert('RGB')
        if width and height:
            img = img.resize((width, height), Image.Resampling.BICUBIC)
        out = bytearray()
        for r, g, b in img.getdata():
            out += struct.pack('<H', rgb565(r, g, b))
        frames.append(bytes(out))
    return frames, width, height


def encode_images(args):
    t0 = time.time()
    frames_b, w, h = load_images(args.pattern, args.start, args.frames, args.width, args.height)
    frames_px = [frame_to_pixels(f) for f in frames_b]
    frames_luma = [make_luma_frame(px) for px in frames_px]
    payloads = []
    prev_px = None
    prev_luma = None
    totals = {'I': 0, 'P': 0, 'skip': 0, 'mv': 0, 'avg': 0, 'mvavg': 0, 'raw': 0, 'two': 0, 'solid': 0}
    for i, (px, lu) in enumerate(zip(frames_px, frames_luma)):
        payload, prev_px, prev_luma, stats = encode_frame(px, lu, w, h, i, prev_px, prev_luma, args)
        payloads.append(payload)
        for k, v in stats.items():
            totals[k] += v
        if args.progress and (i + 1) % args.progress == 0:
            print(f'encoded {i+1}/{len(frames_px)} frames...')
    write_mivf(args.output, w, h, payloads, args.fps)
    validate(args.output)
    print('STATS', ' '.join(f'{k}={v}' for k, v in totals.items()))
    print(f'ENCODE_TIME {time.time() - t0:.2f}s')


def validate(path):
    data = Path(path).read_bytes()
    first = struct.unpack_from('<Q', data, 0x24)[0]
    codec = data[68:72].decode('ascii', 'replace')
    w, h, fpsn, fpsd = struct.unpack_from('<HHHH', data, 64+0x10)
    print(f'MIVF codec={codec} {w}x{h} fps={fpsn}/{fpsd} first=0x{first:X} bytes={len(data)}')
    off = first
    pages = 0
    prev = None
    while off + 32 <= len(data):
        _, _, flags, pg, _, psz, pc, _, crc, _ = struct.unpack_from('<2sBBIQIHHII', data, off)
        payload = data[off+32:off+32+psz]
        if flags & 1 and (zlib.crc32(payload) & 0xffffffff) != crc:
            raise ValueError('crc fail')
        po = 0
        for _ in range(pc):
            sid, pf, hs, pts, ps, fn = struct.unpack_from('<BBHIII', payload, po)
            body = payload[po+hs:po+hs+ps]
            if body[:4] == b'M1P0':
                _, prev, _, _, _ = decode_frame(body, prev)
            po += hs + ps
        pages += 1
        off += 32 + psz
    print(f'OK pages={pages}')


def dump(path, prefix):
    data = Path(path).read_bytes()
    first = struct.unpack_from('<Q', data, 0x24)[0]
    off = first
    prev = None
    while off + 32 <= len(data):
        _, _, _, _, _, psz, pc, _, _, _ = struct.unpack_from('<2sBBIQIHHII', data, off)
        payload = data[off+32:off+32+psz]
        po = 0
        for _ in range(pc):
            sid, pf, hs, pts, ps, fn = struct.unpack_from('<BBHIII', payload, po)
            body = payload[po+hs:po+hs+ps]
            frame, prev, w, h, fn = decode_frame(body, prev)
            with open(f'{prefix}_{fn:04d}.ppm', 'wb') as f:
                f.write(f'P6\n{w} {h}\n255\n'.encode())
                for (pix,) in struct.iter_unpack('<H', frame):
                    f.write(bytes(rgb888(pix)))
            po += hs + ps
        off += 32 + psz


def main():
    ap = argparse.ArgumentParser(description='Fast MIV1P encoder v0.8')
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('images')
    p.add_argument('output')
    p.add_argument('pattern', help='printf image pattern, e.g. frame_%%04d.png')
    p.add_argument('--codec', choices=['miv1p'], default='miv1p')
    p.add_argument('--start', type=int, default=1)
    p.add_argument('--frames', type=int, required=True)
    p.add_argument('--width', type=int, default=240)
    p.add_argument('--height', type=int, default=144)
    p.add_argument('--fps', type=int, default=30)
    p.add_argument('--keyint', type=int, default=30)
    p.add_argument('--skip', type=int, default=5)
    p.add_argument('--mvskip', type=int, default=8)
    p.add_argument('--delta', type=int, default=8)
    p.add_argument('--mvdelta', type=int, default=10)
    p.add_argument('--mvradius', type=int, default=4)
    p.add_argument('--mvstep', type=int, default=2)
    p.add_argument('--solid', type=int, default=24)
    p.add_argument('--two', type=int, default=80)
    p.add_argument('--progress', type=int, default=10)
    p.set_defaults(func=encode_images)
    p = sub.add_parser('validate')
    p.add_argument('input')
    p.set_defaults(func=lambda a: validate(a.input))
    p = sub.add_parser('dump')
    p.add_argument('input')
    p.add_argument('prefix')
    p.set_defaults(func=lambda a: dump(a.input, a.prefix))
    args = ap.parse_args()
    args.func(args)

if __name__ == '__main__':
    main()
