#!/usr/bin/env python3
from __future__ import annotations

import argparse
import contextlib
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
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
import json

# MIVF_PROGRESS_HOOK_BEGIN
import builtins as _mivf_builtins
import re as _mivf_re
import sys as _mivf_sys

if not hasattr(_mivf_builtins, "_mivf_orig_print"):
    _mivf_builtins._mivf_orig_print = _mivf_builtins.print

    def _mivf_progress_print(*args, **kwargs):
        text = " ".join(str(a) for a in args)

        m = _mivf_re.search(
            r"Cores Active:\s*(\d+)\s*/\s*(\d+).*?Chunks Done:\s*(\d+).*?Predicted ETA:\s*([0-9:]+)",
            text
        )

        if m:
            active = int(m.group(1))
            total = max(1, int(m.group(2)))
            done = int(m.group(3))
            eta = m.group(4)

            done = max(0, min(done, total))
            width = 36
            fill = int(width * done / total)
            bar = "#" * fill + "-" * (width - fill)
            pct = 100.0 * done / total

            _mivf_sys.stdout.write(
                f"\r[{bar}] {pct:6.2f}% | cores {active}/{total} | chunks {done}/{total} | ETA {eta}   "
            )
            _mivf_sys.stdout.flush()

            if done >= total:
                _mivf_sys.stdout.write("\n")
                _mivf_sys.stdout.flush()

            return

        return _mivf_builtins._mivf_orig_print(*args, **kwargs)

    _mivf_builtins.print = _mivf_progress_print
# MIVF_PROGRESS_HOOK_END



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
DEFAULT_JOBS = min(6, max(1, cpu_count()))
DEFAULT_SEEK_PREROLL = 2.0
DEFAULT_ETA_FPS = 286.0

HEADER_SIZE = 64
STREAM_DESC_SIZE = 32
PAGE_HEADER_SIZE = 32
PACKET_HEADER_SIZE = 16
PAGE_CRC = 1
PAGE_HAS_KEYFRAME = 2

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


@dataclass
class EncodeSettings:
    width: int = DEFAULT_WIDTH
    height: int = DEFAULT_HEIGHT
    fps: int = DEFAULT_FPS
    audio_rate: int = DEFAULT_AUDIO_RATE
    audio_channels: int = DEFAULT_AUDIO_CHANNELS
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
    seek_preroll: float = DEFAULT_SEEK_PREROLL
    eta_fps: float = DEFAULT_ETA_FPS


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


def fmt_time(secs: float) -> str:
    minutes, seconds = divmod(int(secs), 60)
    hours, minutes = divmod(minutes, 60)
    if hours > 0:
        return f"{hours:02d}:{minutes:02d}:{seconds:02d}"
    return f"{minutes:02d}:{seconds:02d}"


def read_mivf_first_page_offset(path: Path | str) -> int:
    with Path(path).open("rb") as handle:
        header = handle.read(64)
    if len(header) < 64 or header[:4] != b"MIVF":
        raise SystemExit(f"Bad MIVF header in {path}")
    first = struct.unpack_from("<Q", header, 36)[0]
    if first < 64 or first > 1 << 30:
        raise SystemExit(f"Bad MIVF first-page offset {first} in {path}")
    return int(first)


def count_mivf_frames(path: Path | str) -> int:
    path = Path(path)
    first_page_offset = read_mivf_first_page_offset(path)
    count = 0
    with path.open("rb") as handle:
        handle.seek(first_page_offset)
        while True:
            page_header = handle.read(PAGE_HEADER_SIZE)
            if not page_header:
                break
            if len(page_header) < PAGE_HEADER_SIZE:
                raise SystemExit(f"Truncated MIVF page header in {path}")
            if page_header[:2] != b"MP":
                break
            payload_size = struct.unpack_from("<I", page_header, 16)[0]
            handle.seek(payload_size, os.SEEK_CUR)
            count += 1
    return count


def clamp_s16(value: int) -> int:
    return max(-32768, min(32767, int(value)))


def make_temp_workdir() -> Path:
    return Path(tempfile.mkdtemp(prefix="mivf_encode_"))


def mivf_helper_path() -> Path:
    return bundled_path("miv2y_moflex_tier.exe")


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


def find_ffprobe_path() -> str:
    candidates = [
        resource_dir() / "ffprobe.exe",
        resource_dir() / "ffprobe",
        Path(sys.executable).resolve().parent / "ffprobe.exe",
        Path(sys.executable).resolve().parent / "ffprobe",
    ]

    for candidate in candidates:
        if candidate.exists():
            return str(candidate)

    found = shutil.which("ffprobe") or shutil.which("ffprobe.exe")
    if found:
        return found

    raise FileNotFoundError(
        "FFprobe was not found. Install ffprobe, add it to PATH, or place ffprobe.exe next to encode_mivf.exe / encode_mivf.py."
    )


def probe_video_frame_count(input_path: Path) -> int:
    """Instantly extracts video frame count via fast header metadata json query."""
    ffprobe = find_ffprobe_path()
    
    # Fast approach: query container-level fields using structured json format
    cmd = [
        ffprobe,
        "-v", "error",
        "-select_streams", "v:0",
        "-show_entries", "stream=nb_frames,duration,r_frame_rate:format=duration",
        "-of", "json",
        str(input_path),
    ]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        data = json.loads(result.stdout)
        
        # Strategy A: Check stream-level frame count headers directly
        if "streams" in data and data["streams"]:
            stream = data["streams"][0]
            if "nb_frames" in stream:
                with contextlib.suppress(ValueError):
                    frames = int(stream["nb_frames"])
                    if frames > 0:
                        return frames

        # Strategy B: Compute using absolute container duration and frame rate
        duration = None
        if "streams" in data and data["streams"] and "duration" in data["streams"][0]:
            with contextlib.suppress(ValueError):
                duration = float(data["streams"][0]["duration"])
        if duration is None and "format" in data and "duration" in data["format"]:
            with contextlib.suppress(ValueError):
                duration = float(data["format"]["duration"])

        fps = 30.0
        if "streams" in data and data["streams"] and "r_frame_rate" in data["streams"][0]:
            r_fps = data["streams"][0]["r_frame_rate"]
            if "/" in r_fps:
                try:
                    num, den = map(int, r_fps.split("/"))
                    if den != 0:
                        fps = num / den
                except ValueError:
                    pass

        if duration is not None and duration > 0:
            return int(duration * fps)

    except Exception:
        pass

    # Ultimate ultra-fallback if container headers are completely uncooperative: 
    # Only run the slow frame decoding count pass as a last resort.
    cmd_slow = [
        ffprobe,
        "-v", "error",
        "-count_frames",
        "-select_streams", "v:0",
        "-show_entries", "stream=nb_read_frames",
        "-of", "default=nokey=1:noprint_wrappers=1",
        str(input_path),
    ]
    result_slow = subprocess.run(cmd_slow, capture_output=True, text=True, check=True)
    output = result_slow.stdout.strip()
    for line in output.splitlines():
        if line.strip():
            with contextlib.suppress(ValueError):
                frames = int(line.strip())
                if frames > 0:
                    return frames

    raise SystemExit("Unable to determine total video frame count from input.")


def run_ffmpeg_extract_to_stream(input_path: Path, settings: EncodeSettings) -> subprocess.Popen[bytes]:
    ffmpeg = mivf_ffmpeg_path()
    ffmpeg_cmd = [
        ffmpeg,
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        str(input_path),
        "-vf",
        f"scale={settings.width}:{settings.height},format=yuv420p",
        "-vsync",
        "0",
        "-f",
        "rawvideo",
        "pipe:1",
    ]
    return subprocess.Popen(ffmpeg_cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)


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
        str(pcm_path),
    ]
    print("Running:", " ".join(cmd))
    subprocess.run(cmd, check=True)
    return pcm_path


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
    buf[28:32] = struct.pack("<I", streams)
    buf[36:44] = struct.pack("<Q", first)
    return bytes(buf)


def build_parallel_mivf(workdir: Path, source_path: Path, temp_video_only: Path, settings: EncodeSettings, source_is_raw_yuv: bool, make_m2y2: bool = False, inline_m2y2: bool = True) -> tuple[int, float, float, float]:
    # Run the helper binary directly from its safe extraction path instead of
    # copying it into a temp directory where Windows Defender may interfere.
    helper = mivf_helper_path()
    frame_size = settings.width * settings.height + (settings.width // 2) * (settings.height // 2) * 2

    if source_is_raw_yuv:
        total_bytes = source_path.stat().st_size
        total_frames = total_bytes // frame_size
    else:
        print("Parallel Engine: Probing source video frame count...")
        total_frames = probe_video_frame_count(source_path)

    cores = min(max(1, settings.jobs), cpu_count(), total_frames if total_frames > 0 else 1)
    print(f"Parallel Engine: Slicing raw data across {cores} CPU Core Clusters...")

    frames_per_core = total_frames // cores if cores else total_frames
    if frames_per_core == 0:
        cores = 1
        frames_per_core = total_frames

    chunk_outputs: list[Path] = []
    processes: list[subprocess.Popen[bytes]] = []
    chunk_frame_counts: list[int] = []

    for idx in range(cores):
        num_frames = frames_per_core if idx < cores - 1 else total_frames - (frames_per_core * idx)
        if num_frames <= 0:
            break
        chunk_frame_counts.append(num_frames)
        chunk_outputs.append(workdir / f"temp_slice_{idx}.mivf")

    if source_is_raw_yuv:
        with source_path.open("rb") as source:
            for idx, num_frames in enumerate(chunk_frame_counts):
                chunk_yuv = workdir / f"temp_slice_{idx}.yuv"
                chunk_yuv.write_bytes(source.read(num_frames * frame_size))

        print("Parallel Engine: Master raw cache split successfully. Flushing temporary disk space...")
        source_path.unlink(missing_ok=True)

        for idx in range(len(chunk_frame_counts)):
            chunk_yuv = workdir / f"temp_slice_{idx}.yuv"
            cmd = [
                str(helper),
                "--input",
                str(chunk_yuv),
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
                "--keep",
                str(settings.keep),
            ]
            processes.append(subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=workdir))
    else:
        print("Parallel Engine: Launching true parallel ffmpeg slice workers...")
        ffmpeg_processes = []

        start_frame = 0
        for idx, chunk_mivf in enumerate(chunk_outputs):
            num_frames = chunk_frame_counts[idx]
            start_time_sec = start_frame / float(settings.fps)
            start_frame += num_frames

            cmd = [
                str(helper),
                "--input",
                "-",
                "--output",
                str(chunk_mivf),
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
                "--keep",
                str(settings.keep),
            ]

            helper_proc = subprocess.Popen(
                cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                cwd=workdir,
            )
            processes.append(helper_proc)

            ffmpeg = mivf_ffmpeg_path()
            seek_preroll = float(settings.seek_preroll)
            seek_start_sec = max(0.0, start_time_sec - seek_preroll)
            trim_start_sec = start_time_sec - seek_start_sec

            vf_expr = (
                f"trim=start={trim_start_sec:.6f},"
                f"setpts=PTS-STARTPTS,"
                f"scale={settings.width}:{settings.height},"
                f"format=yuv420p"
            )

            ffmpeg_cmd = [
                ffmpeg,
                "-y",
                "-hide_banner",
                "-loglevel",
                "error",
                "-threads",
                "1",
                "-ss",
                f"{seek_start_sec:.6f}",
                "-i",
                str(source_path),
                "-an",
                "-sn",
                "-dn",
                "-vf",
                vf_expr,
                "-vsync",
                "0",
                "-frames:v",
                str(num_frames),
                "-f",
                "rawvideo",
                "pipe:1",
            ]

            if helper_proc.stdin is None:
                raise SystemExit("Helper stdin unavailable.")

            ffmpeg_proc = subprocess.Popen(
                ffmpeg_cmd,
                stdout=helper_proc.stdin,
                stderr=subprocess.DEVNULL,
            )

            helper_proc.stdin.close()
            ffmpeg_processes.append(ffmpeg_proc)

    print(f"Parallel Engine: Launching {len(processes)} concurrent compression instances at full throttle...")
    encode_start = time.perf_counter()
    progress_pct = 0.0

    while True:
        active = [proc for proc in processes if proc.poll() is None]
        active_count = len(active)
        done_count = len(processes) - active_count
        elapsed = time.perf_counter() - encode_start

        total_jobs = max(1, len(processes))
        if done_count > 0:
            est_total_time = elapsed * total_jobs / max(1, done_count)
            est_pct = min(85.0, 100.0 * elapsed / max(0.001, est_total_time))
            real_pct = 100.0 * done_count / total_jobs
            display_pct = max(progress_pct, real_pct if done_count > 0 else est_pct)
        else:
            eta_fps = max(1.0, float(getattr(settings, "eta_fps", 286.0)))
            est_total_time = max(0.001, total_frames / eta_fps)
            est_pct = min(85.0, 100.0 * elapsed / est_total_time)
            display_pct = max(progress_pct, est_pct)

        progress_pct = display_pct

        if active_count == 0:
            display_pct = 100.0
            eta_str = "00:00"
        else:
            eta_left = max(0.0, est_total_time - elapsed)
            eta_str = fmt_time(eta_left)

        bar_w = 28
        fill = int(bar_w * display_pct / 100.0)
        bar = "#" * fill + "-" * (bar_w - fill)

        sys.stdout.write(
            f"\r[Time: {fmt_time(elapsed)}] [{bar}] est {display_pct:5.1f}% | "
            f"chunks {done_count}/{len(processes)} | workers {active_count}/{len(processes)} | ETA {eta_str}                    "
        )
        sys.stdout.flush()

        if active_count == 0:
            break
        time.sleep(1)

    if "ffmpeg_processes" in locals():
        for _i, _fp in enumerate(ffmpeg_processes):
            _rc = _fp.wait()
            if _rc != 0:
                raise SystemExit(f"FFmpeg slice worker {_i} failed with exit code {_rc}.")

    encode_elapsed = time.perf_counter() - encode_start
    print("\n============================================================")
    print(f"TOTAL ENCODER RUNTIME: {fmt_time(encode_elapsed)} | Speed: {total_frames / max(encode_elapsed, 0.001):.1f} fps")
    print("============================================================")

    if make_m2y2 and inline_m2y2:
        print()
        print("============================================================")
        print("Parallel Engine: Range-coding video slices to M2Y2 in parallel")
        print("============================================================")

        def _m2y2_one(pair):
            idx, src_path = pair
            dst_path = src_path.with_name(src_path.stem + ".m2y2slice")
            transcode_to_m2y2(src_path, dst_path)
            src_path.unlink(missing_ok=True)
            return idx, dst_path

        new_chunk_outputs = [None] * len(chunk_outputs)
        m2_start = time.perf_counter()
        max_m2_jobs = min(len(chunk_outputs), max(1, getattr(settings, "jobs", len(chunk_outputs))))

        with ThreadPoolExecutor(max_workers=max_m2_jobs) as pool:
            futures = [pool.submit(_m2y2_one, (idx, path)) for idx, path in enumerate(chunk_outputs)]
            done = 0
            for fut in as_completed(futures):
                idx, dst_path = fut.result()
                new_chunk_outputs[idx] = dst_path
                done += 1
                elapsed_m2 = time.perf_counter() - m2_start
                print(f"M2Y2 slice {done}/{len(chunk_outputs)} done in {fmt_time(elapsed_m2)}", flush=True)

        chunk_outputs = new_chunk_outputs
        m2y2_elapsed = time.perf_counter() - m2_start
        print(f"Parallel Engine: Parallel slice M2Y2 complete in {fmt_time(m2y2_elapsed)}.")
    else:
        m2y2_elapsed = 0.0
        if make_m2y2:
            print("Parallel Engine: Inline M2Y2 disabled; running the final full-file M2Y2 pass later.")

    print("Parallel Engine: Patching headers and reconstructing container streams...")
    merge_start = time.perf_counter()

    with temp_video_only.open("wb") as out_file:
        first_chunk_path = chunk_outputs[0]
        first_payload_offset = read_mivf_first_page_offset(first_chunk_path)
        with first_chunk_path.open("rb") as first_chunk:
            header = bytearray(first_chunk.read(first_payload_offset))

        first_chunk_frames = chunk_frame_counts[0] if chunk_frame_counts else total_frames
        chunk_duration = first_chunk_frames * 30000 // settings.fps
        total_duration = total_frames * 30000 // settings.fps
        chunk_dur_bytes = struct.pack("<Q", chunk_duration)
        total_dur_bytes = struct.pack("<Q", total_duration)

        search_pos = 0
        while True:
            idx = header.find(chunk_dur_bytes, search_pos)
            if idx == -1:
                break
            header[idx:idx + 8] = total_dur_bytes
            search_pos = idx + 8

        out_file.write(header)

        running_frame_idx = 0
        for chunk_index, chunk_mivf in enumerate(chunk_outputs):
            with chunk_mivf.open("rb") as handle:
                handle.seek(first_payload_offset)
                while True:
                    page_header = handle.read(PAGE_HEADER_SIZE)
                    if not page_header:
                        break
                    if len(page_header) < PAGE_HEADER_SIZE:
                        raise SystemExit(f"Truncated MIVF page header in {chunk_mivf}")
                    if page_header[:2] != b"MP":
                        break

                    payload_size = struct.unpack_from("<I", page_header, 16)[0]
                    page_payload = handle.read(payload_size)
                    if len(page_payload) != payload_size:
                        raise SystemExit(
                            f"Bad MIVF page while merging chunk {chunk_index}: "
                            f"offset={handle.tell()} payload={payload_size} file_len={chunk_mivf.stat().st_size}"
                        )

                    page_header = bytearray(page_header)
                    wr_u32le(page_header, 4, running_frame_idx)
                    wr_u64le(page_header, 8, running_frame_idx * 30000 // settings.fps)
                    out_file.write(page_header)
                    out_file.write(page_payload)
                    running_frame_idx += 1

    for chunk_mivf in chunk_outputs:
        chunk_mivf.unlink(missing_ok=True)

    merge_elapsed = time.perf_counter() - merge_start
    print(f"Parallel Engine: Master container unified with {running_frame_idx} sequential frames.")

    if running_frame_idx != total_frames:
        print(f"WARNING: merged frame count {running_frame_idx} != expected {total_frames}")

    return total_frames, encode_elapsed, merge_elapsed, m2y2_elapsed


def mux_audio_into_mivf(video_mivf: Path, audio_src: Path, out_path: Path, rate: int, channels: int, workdir: Path) -> None:
    if channels != 1:
        raise SystemExit("IA4M mux currently supports mono only")

    with video_mivf.open("rb") as handle:
        header = handle.read(64)
        if header[:4] != b"MIVF":
            raise SystemExit("not MIVF")

        streams = le32(header, 28)
        first_old = le64(header, 36)
        if streams != 1:
            raise SystemExit(f"expected video-only MIVF with 1 stream, got {streams}")

        desc0 = handle.read(first_old - 64)
        fpsn = le16(desc0, 20) or DEFAULT_FPS
        fpsd = le16(desc0, 22) or 1
        samples_per_frame = rate * fpsd // fpsn

        extra = b"IA4M" + struct.pack("<IHHI", rate, channels, samples_per_frame, 0)
        if len(extra) != 16:
            raise AssertionError(len(extra))

        desc1 = make_stream_desc(1, 2, b"IA4M", rate, channels, samples_per_frame, 1, extra)
        first_new = HEADER_SIZE + len(desc0) + len(desc1)

        pcm_path = extract_pcm16(audio_src, rate, channels, workdir)
        pcm = read_pcm16(pcm_path)

        with out_path.open("wb") as out_handle:
            out_handle.write(wr_header(header, 2, first_new))
            out_handle.write(desc0)
            out_handle.write(desc1)

            frame_no = 0
            while True:
                page_header = handle.read(PAGE_HEADER_SIZE)
                if not page_header:
                    break
                if len(page_header) < PAGE_HEADER_SIZE:
                    raise SystemExit(f"truncated MIVF page header in {video_mivf}")
                if page_header[:2] != b"MP":
                    break

                payload_size = le32(page_header, 16)
                payload = handle.read(payload_size)
                if len(payload) != payload_size:
                    raise SystemExit(f"truncated MIVF page payload in {video_mivf}")

                start = frame_no * samples_per_frame
                end = start + samples_per_frame
                samples = pcm[start:end]
                if len(samples) < samples_per_frame:
                    samples += [0] * (samples_per_frame - len(samples))

                abody = encode_ia4m_packet(samples, frame_no)
                apkt = struct.pack("<BBHIII", 1, 0, PACKET_HEADER_SIZE, 0, len(abody), frame_no) + abody
                new_payload = payload + apkt
                crc = zlib.crc32(new_payload) & 0xFFFFFFFF

                page = struct.pack(
                    "<2sBBIQIHHII",
                    b"MP",
                    PAGE_HEADER_SIZE,
                    page_header[3],
                    le32(page_header, 4),
                    le64(page_header, 8),
                    len(new_payload),
                    le16(page_header, 20) + 1,
                    le16(page_header, 22),
                    crc,
                    0,
                )

                out_handle.write(page)
                out_handle.write(new_payload)
                frame_no += 1

    print(f"WROTE {out_path}")
    print(f"frames={frame_no} audio={rate}Hz channels={channels} samples/frame={samples_per_frame} bytes={out_path.stat().st_size}")


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
    """Losslessly range-code an M2Y1 .mivf into the smaller M2Y2 codec."""
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



def deploy_output(output_path):
    print(f"Deploy skipped: {output_path}")

def encode_one(input_path: Path, output_path: Path, settings: EncodeSettings, deploy_sd: bool, make_m2y2: bool = False, inline_m2y2: bool = True) -> None:
    workdir = make_temp_workdir()
    try:
        temp_video_only = workdir / "temp_video_only.mivf"
        source_is_raw_yuv = input_path.suffix.lower() == ".yuv"

        print("============================================================")
        print("1. Preparing Raw Master Frame Buffer")
        print("============================================================")
        if source_is_raw_yuv:
            temp_raw = workdir / "temp_master_raw.yuv"
            shutil.copy2(input_path, temp_raw)
            source_path = temp_raw
        else:
            source_path = input_path

        print()
        print("============================================================")
        print("2. Splitting and Compressing Video Streams (Multi-Core Cluster Engine)")
        print("============================================================")
        video_start = time.perf_counter()
        total_frames, video_encode_time, merge_time, m2y2_time = build_parallel_mivf(
            workdir,
            source_path,
            temp_video_only,
            settings,
            source_is_raw_yuv,
            make_m2y2=make_m2y2,
            inline_m2y2=inline_m2y2,
        )
        video_total_time = time.perf_counter() - video_start

        print()
        print("============================================================")
        print("3. Multiplexing Compressed 4-bit Audio (mivf_ia4m_mux.py)")
        print("============================================================")
        audio_start = time.perf_counter()
        mux_audio_into_mivf(temp_video_only, input_path, output_path, settings.audio_rate, settings.audio_channels, workdir)
        audio_time = time.perf_counter() - audio_start

        if make_m2y2 and not inline_m2y2:
            print()
            print("============================================================")
            print("4. Range-coding video to M2Y2 (lossless, smaller file)")
            print("============================================================")
            tmp_m2y2 = output_path.with_name(output_path.stem + ".m2y2tmp")
            m2y2_start = time.perf_counter()
            transcode_to_m2y2(output_path, tmp_m2y2)
            os.replace(tmp_m2y2, output_path)
            m2y2_time = time.perf_counter() - m2y2_start

        final_frame_count = count_mivf_frames(output_path)
        if abs(final_frame_count - total_frames) > 4:
            raise SystemExit(
                f"FRAME COUNT DRIFT: expected {total_frames} frames but output has {final_frame_count}"
            )
        if final_frame_count != total_frames:
            print(
                f"WARNING: final frame count differs by {abs(final_frame_count - total_frames)} frame(s)"
            )

        print()
        print("============================================================")
        print("Pipeline timing summary")
        print("============================================================")
        print(f"video encode time : {fmt_time(video_encode_time)}")
        print(f"merge time        : {fmt_time(merge_time)}")
        print(f"audio mux time    : {fmt_time(audio_time)}")
        print(f"M2Y2 time         : {fmt_time(m2y2_time)}")
        print(f"total pipeline    : {fmt_time(video_encode_time + merge_time + audio_time + m2y2_time)}")
        print(f"output size       : {output_path.stat().st_size / (1024 * 1024):.2f} MiB")
        print(f"final frame count : {final_frame_count}")

        if deploy_sd:
            deploy_output(output_path)
    finally:
        shutil.rmtree(workdir, ignore_errors=True)


def encode_folder(input_dir: Path, output_dir: Path, settings: EncodeSettings, make_m2y2: bool = False, inline_m2y2: bool = True) -> None:
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
        encode_one(source, target, settings, deploy_sd=False, make_m2y2=make_m2y2, inline_m2y2=inline_m2y2)

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
    parser.add_argument("--keyint", type=int, default=DEFAULT_KEYINT)
    parser.add_argument("--qp", type=int, default=DEFAULT_QP)
    parser.add_argument("--c-qp-offset", type=int, default=DEFAULT_C_QP_OFFSET)
    parser.add_argument("--lambda", dest="lambda_value", type=float, default=DEFAULT_LAMBDA)
    parser.add_argument("--y-skip", type=int, default=DEFAULT_Y_SKIP)
    parser.add_argument("--c-skip", type=int, default=DEFAULT_C_SKIP)
    parser.add_argument("--y-delta", type=int, default=DEFAULT_Y_DELTA)
    parser.add_argument("--c-delta", type=int, default=DEFAULT_C_DELTA)
    parser.add_argument("--mv-range", type=int, default=DEFAULT_MV_RANGE)
    parser.add_argument("--keep", type=int, default=DEFAULT_KEEP, choices=[4, 8, 16], help="transform coefficients kept per 4x4 quadrant: 16=HD detail (default), 4=small legacy files")
    parser.add_argument("--jobs", type=int, default=DEFAULT_JOBS, help=f"parallel slice workers, default {DEFAULT_JOBS}")
    parser.add_argument("--seek-preroll", type=float, default=DEFAULT_SEEK_PREROLL, help=f"seconds before each slice to seek for hybrid accurate slicing, default {DEFAULT_SEEK_PREROLL}")
    parser.add_argument("--eta-fps", type=float, default=DEFAULT_ETA_FPS, help=f"smooth ETA estimated encode fps, default {DEFAULT_ETA_FPS}")
    parser.add_argument("--no-deploy", action="store_true", help="skip SD card deployment")
    parser.add_argument("--m2y2", action="store_true", help="range-code video to the smaller M2Y2 codec (lossless, approximately 20 percent smaller)")
    parser.add_argument("--inline-m2y2", dest="inline_m2y2", action="store_true", help="use the per-slice inline M2Y2 path when --m2y2 is enabled")
    parser.add_argument("--no-inline-m2y2", dest="inline_m2y2", action="store_false", help="skip the per-slice inline M2Y2 path and use a final full-file pass instead")
    parser.set_defaults(inline_m2y2=True)
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
        seek_preroll=args.seek_preroll,
        eta_fps=args.eta_fps,
    )

    input_path = Path(args.input)
    output_path = Path(args.output)

    if input_path.is_dir():
        encode_folder(input_path, output_path, settings, make_m2y2=args.m2y2, inline_m2y2=args.inline_m2y2 and args.m2y2)
        return

    if not input_path.exists():
        raise SystemExit(f"Input file not found: {input_path}")

    encode_one(input_path, output_path, settings, deploy_sd=not args.no_deploy, make_m2y2=args.m2y2, inline_m2y2=args.inline_m2y2 and args.m2y2)


if __name__ == "__main__":
    main()