#!/usr/bin/env python3
"""MIVF DVD bonus-feature pipeline (planner + smoke builder + MIVF encoder + audit).

This tool is deliberately conservative:
- It never modifies source media.
- It operates only on enabled ordinary media files in a selection manifest.
- DVD-title entries without an ordinary file path are reported but not extracted.
- It creates a short normalized smoke compilation before any full compilation.
- Full compilation is gated behind --allow-full.
- Player/menu source changes are NOT attempted; section metadata is generated for that later patch.

Typical workflow:
  python tools/mivf_dvd_pipeline.py plan --manifest les_miserables_dvd_selection.json
  python tools/mivf_dvd_pipeline.py smoke --manifest les_miserables_dvd_selection.json
  python tools/mivf_dvd_pipeline.py encode-smoke --manifest les_miserables_dvd_selection.json
  python tools/mivf_dvd_pipeline.py audit-smoke --manifest les_miserables_dvd_selection.json

The script expects ffmpeg/ffprobe on PATH and the existing encode_mivf.py in the repo root.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path
from typing import Any

SCHEMA_SELECTION = "mivf-dvd-selection-v1"
SCHEMA_PLAN = "mivf-dvd-compile-plan-v1"
SCHEMA_SECTIONS = "mivf-sections-v1"


def die(message: str) -> "NoReturn":
    raise SystemExit(f"ERROR: {message}")


def load_json(path: Path) -> dict[str, Any]:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        die(f"missing JSON file: {path}")
    except json.JSONDecodeError as exc:
        die(f"invalid JSON in {path}: {exc}")


def write_json(path: Path, value: Any) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")


def run(command: list[str], *, capture: bool = False, log: Path | None = None) -> subprocess.CompletedProcess[str]:
    printable = subprocess.list2cmdline(command)
    print(f"+ {printable}", flush=True)
    if capture:
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
        if log:
            log.write_text((result.stdout or "") + (result.stderr or ""), encoding="utf-8")
    elif log:
        with log.open("w", encoding="utf-8", newline="\n") as handle:
            result = subprocess.run(command, stdout=handle, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
    else:
        result = subprocess.run(command, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        die(f"command failed with status {result.returncode}: {printable}")
    return result


def require_tool(name: str, explicit: str | None = None) -> str:
    value = explicit or shutil.which(name)
    if not value:
        die(f"required tool not found: {name}")
    return value


def parse_fraction(value: str) -> Fraction:
    try:
        result = Fraction(value)
    except Exception as exc:
        die(f"invalid rational value {value!r}: {exc}")
    if result <= 0:
        die(f"rational value must be positive: {value}")
    return result


def decimal_to_fraction(value: str | float | int) -> Fraction:
    return Fraction(str(value))


def ceil_frames(duration: Fraction, fps: Fraction) -> int:
    value = duration * fps
    return (value.numerator + value.denominator - 1) // value.denominator


def floor_frame(time_value: Fraction, fps: Fraction) -> int:
    value = time_value * fps
    return value.numerator // value.denominator


def fmt_time(seconds: Fraction | float) -> str:
    f = float(seconds)
    hours = int(f // 3600)
    minutes = int((f % 3600) // 60)
    secs = f % 60
    return f"{hours:02d}:{minutes:02d}:{secs:06.3f}"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def ffprobe_json(ffprobe: str, source: str) -> dict[str, Any]:
    result = run([
        ffprobe, "-v", "error", "-show_format", "-show_streams", "-show_chapters", "-of", "json", source
    ], capture=True)
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        die(f"ffprobe returned invalid JSON for {source}: {exc}")


def first_video(probe: dict[str, Any]) -> dict[str, Any]:
    for stream in probe.get("streams", []):
        if stream.get("codec_type") == "video":
            return stream
    die("source has no video stream")


def stream_by_absolute_index(probe: dict[str, Any], index: int) -> dict[str, Any]:
    for stream in probe.get("streams", []):
        if stream.get("index") == index:
            return stream
    die(f"declared absolute stream index {index} does not exist")


def source_duration(probe: dict[str, Any]) -> Fraction:
    raw = probe.get("format", {}).get("duration")
    if raw is None:
        die("source has no format duration")
    return decimal_to_fraction(raw)


def display_ratio(video: dict[str, Any]) -> Fraction:
    dar = video.get("display_aspect_ratio")
    if dar and dar not in {"0:1", "N/A"}:
        a, b = dar.split(":", 1)
        return Fraction(int(a), int(b))
    width = int(video.get("width") or 0)
    height = int(video.get("height") or 0)
    if not width or not height:
        die("unable to determine video dimensions")
    sar = video.get("sample_aspect_ratio")
    if sar and sar not in {"0:1", "N/A"}:
        a, b = sar.split(":", 1)
        return Fraction(width * int(a), height * int(b))
    return Fraction(width, height)


def fit_even_canvas(dar: Fraction, canvas_w: int, canvas_h: int) -> tuple[int, int, int, int]:
    canvas_dar = Fraction(canvas_w, canvas_h)
    if dar >= canvas_dar:
        scaled_w = canvas_w
        scaled_h = int(Fraction(canvas_w, 1) / dar)
    else:
        scaled_h = canvas_h
        scaled_w = int(Fraction(canvas_h, 1) * dar)
    scaled_w = max(2, scaled_w - scaled_w % 2)
    scaled_h = max(2, scaled_h - scaled_h % 2)
    x = (canvas_w - scaled_w) // 2
    y = (canvas_h - scaled_h) // 2
    x -= x % 2
    y -= y % 2
    return scaled_w, scaled_h, x, y


def enabled_items(selection: dict[str, Any]) -> list[dict[str, Any]]:
    items = [selection["main_feature"]]
    items.extend(item for item in selection.get("bonus_features", []) if item.get("enabled", True))
    return items


def validate_selection(selection: dict[str, Any]) -> None:
    if selection.get("schema") != SCHEMA_SELECTION:
        die(f"selection schema must be {SCHEMA_SELECTION}")
    required_policy = ["output_width", "output_height", "output_fps", "output_audio_rate"]
    for key in required_policy:
        if key not in selection.get("timeline_policy", {}):
            die(f"timeline_policy missing {key}")
    ids: set[str] = set()
    for item in enabled_items(selection):
        item_id = item.get("id")
        if not item_id or item_id in ids:
            die(f"missing or duplicate section id: {item_id!r}")
        ids.add(item_id)
        if item.get("source_type") != "file":
            die(f"enabled item {item_id} is not an ordinary file source")
        if not item.get("path"):
            die(f"enabled item {item_id} has no path")
        if "preferred_audio_absolute_index" not in item:
            die(f"enabled item {item_id} lacks preferred_audio_absolute_index")


@dataclass
class PlannedSection:
    raw: dict[str, Any]
    source: str
    probe: dict[str, Any]
    duration: Fraction
    source_fps: Fraction
    output_frames: int
    start_frame: int
    end_frame: int
    scale_w: int
    scale_h: int
    pad_x: int
    pad_y: int
    audio_index: int


def build_plan(selection_path: Path, ffprobe: str, workdir: Path) -> tuple[dict[str, Any], dict[str, Any]]:
    selection = load_json(selection_path)
    validate_selection(selection)
    policy = selection["timeline_policy"]
    output_fps = parse_fraction(policy["output_fps"])
    canvas_w = int(policy["output_width"])
    canvas_h = int(policy["output_height"])
    audio_rate = int(policy["output_audio_rate"])
    start = 0
    planned: list[PlannedSection] = []

    for raw in enabled_items(selection):
        source = raw["path"]
        path = Path(source)
        if not path.exists():
            die(f"enabled source does not exist: {source}")
        probe = ffprobe_json(ffprobe, source)
        video = first_video(probe)
        source_fps = parse_fraction(video.get("avg_frame_rate") or video.get("r_frame_rate"))
        duration = source_duration(probe)
        audio_index = int(raw["preferred_audio_absolute_index"])
        audio = stream_by_absolute_index(probe, audio_index)
        if audio.get("codec_type") != "audio":
            die(f"section {raw['id']} stream {audio_index} is not audio")
        if int(audio.get("sample_rate") or 0) != audio_rate:
            print(f"WARNING: {raw['id']} audio will be resampled from {audio.get('sample_rate')} to {audio_rate}")
        frames = ceil_frames(duration, output_fps)
        dar = display_ratio(video)
        sw, sh, px, py = fit_even_canvas(dar, canvas_w, canvas_h)
        planned.append(PlannedSection(raw, source, probe, duration, source_fps, frames, start, start + frames, sw, sh, px, py, audio_index))
        start += frames

    sections: list[dict[str, Any]] = []
    plan_sections: list[dict[str, Any]] = []
    chapters: list[dict[str, Any]] = []
    for index, item in enumerate(planned):
        section_type = "main" if index == 0 else "bonus"
        return_page = "main" if section_type == "main" else "bonus"
        conversion = (
            f"fps={output_fps.numerator}/{output_fps.denominator}"
        )
        video = first_video(item.probe)
        audio = stream_by_absolute_index(item.probe, item.audio_index)
        section = {
            "id": item.raw["id"], "type": section_type, "name": item.raw.get("output_name") or item.raw.get("name") or item.raw["id"],
            "source": item.source, "source_dvd_title": item.raw.get("dvd_title"),
            "start_frame": item.start_frame, "end_frame_exclusive": item.end_frame,
            "frame_count": item.output_frames, "return_page": return_page,
        }
        sections.append(section)
        plan_sections.append({
            **section,
            "source_duration_seconds": str(float(item.duration)),
            "source_duration_hms": fmt_time(item.duration),
            "source_video": {
                "codec": video.get("codec_name"), "width": video.get("width"), "height": video.get("height"),
                "display_aspect_ratio": video.get("display_aspect_ratio"), "sample_aspect_ratio": video.get("sample_aspect_ratio"),
                "avg_frame_rate": video.get("avg_frame_rate"), "field_order": video.get("field_order"),
            },
            "video_normalization": {
                "fps_filter": conversion,
                "scale_width": item.scale_w, "scale_height": item.scale_h,
                "pad_x": item.pad_x, "pad_y": item.pad_y,
                "output_width": canvas_w, "output_height": canvas_h,
                "filter": f"scale={item.scale_w}:{item.scale_h}:flags=lanczos,pad={canvas_w}:{canvas_h}:{item.pad_x}:{item.pad_y}:color=black,setsar=1" + (f",{conversion}" if conversion != "none" else ""),
            },
            "audio": {
                "absolute_stream_index": item.audio_index, "codec": audio.get("codec_name"),
                "language": audio.get("tags", {}).get("language"), "channels": audio.get("channels"),
                "sample_rate": audio.get("sample_rate"), "normalize_to_rate": audio_rate, "normalize_to_channels": 2,
            },
        })
        if index == 0:
            for chapter_number, chapter in enumerate(item.probe.get("chapters", []), 1):
                timestamp = decimal_to_fraction(chapter["start_time"])
                local_frame = floor_frame(timestamp, output_fps)
                chapters.append({
                    "chapter": chapter_number, "name": chapter.get("tags", {}).get("title") or f"Chapter {chapter_number}",
                    "source_time_seconds": chapter["start_time"], "local_frame": local_frame,
                    "global_frame": item.start_frame + local_frame,
                })

    plan = {
        "schema": SCHEMA_PLAN,
        "selection_manifest": str(selection_path),
        "ready": True,
        "output": {
            "width": canvas_w, "height": canvas_h, "fps": f"{output_fps.numerator}/{output_fps.denominator}",
            "audio_rate": audio_rate, "intermediate_audio_channels": 2,
            "total_frames": start, "projected_duration_seconds": float(Fraction(start, 1) / output_fps),
            "projected_duration_hms": fmt_time(Fraction(start, 1) / output_fps),
        },
        "sections": plan_sections,
        "chapters": chapters,
        "warnings": [
            "Projected frame counts are calculated from container duration; real normalized output must be probed and reconciled.",
            "Title 26 is disabled and is not included.",
            "The fps filter is deterministic but visual cadence must be reviewed on the generated previews.",
            "No player/menu source changes are performed by this pipeline.",
        ],
    }
    section_doc = {
        "schema": SCHEMA_SECTIONS,
        "fps": plan["output"]["fps"],
        "total_frames": start,
        "sections": sections,
        "chapters": chapters,
    }
    workdir.mkdir(parents=True, exist_ok=True)
    write_json(workdir / "les_miserables_compile_plan.json", plan)
    write_json(workdir / "les_miserables_sections.json", section_doc)
    lines = [
        "MIVF DVD Compilation Plan", "=" * 88,
        f"Ready: {plan['ready']}",
        f"Output: {canvas_w}x{canvas_h} @ {plan['output']['fps']}",
        f"Projected duration: {plan['output']['projected_duration_hms']}",
        f"Projected frames: {start}", "",
    ]
    for section in plan_sections:
        lines += [
            f"[{section['id']}] {section['name']}",
            f"  Source: {section['source']}",
            f"  Source duration: {section['source_duration_hms']}",
            f"  Frames: [{section['start_frame']}, {section['end_frame_exclusive']}) ({section['frame_count']})",
            f"  Video: {section['source_video']['width']}x{section['source_video']['height']} @ {section['source_video']['avg_frame_rate']}",
            f"  Filter: {section['video_normalization']['filter']}",
            f"  Audio absolute index: {section['audio']['absolute_stream_index']}", "",
        ]
    lines += ["Chapters", "-" * 88]
    for chapter in chapters:
        lines.append(f"{chapter['chapter']:2d}. frame={chapter['global_frame']:8d} source_time={chapter['source_time_seconds']} {chapter['name']}")
    lines += ["", "Warnings", "-" * 88]
    lines.extend(f"- {warning}" for warning in plan["warnings"])
    (workdir / "les_miserables_compile_plan.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return plan, section_doc


def choose_smoke_windows(plan: dict[str, Any], main_seconds: int, bonus_seconds: int) -> list[tuple[dict[str, Any], int, int]]:
    windows: list[tuple[dict[str, Any], int, int]] = []
    for index, section in enumerate(plan["sections"]):
        duration = float(section["source_duration_seconds"])
        requested = main_seconds if index == 0 else bonus_seconds
        length = min(requested, max(1, int(duration)))
        start = 60 if duration >= length + 120 else max(0, int((duration - length) / 2))
        windows.append((section, start, length))
    return windows


def build_smoke(manifest: Path, ffmpeg: str, ffprobe: str, workdir: Path, main_seconds: int, bonus_seconds: int) -> None:
    plan, _ = build_plan(manifest, ffprobe, workdir)
    windows = choose_smoke_windows(plan, main_seconds, bonus_seconds)
    segment_dir = workdir / "segments"
    segment_dir.mkdir(parents=True, exist_ok=True)
    segment_paths: list[Path] = []
    real_sections: list[dict[str, Any]] = []
    output_fps = parse_fraction(plan["output"]["fps"])
    cursor = 0

    for idx, (section, start_seconds, length_seconds) in enumerate(windows):
        segment = segment_dir / f"{idx:02d}_{section['id']}.mkv"
        video_filter = section["video_normalization"]["filter"]
        command = [
            ffmpeg, "-y", "-hide_banner", "-loglevel", "warning",
            "-ss", str(start_seconds), "-t", str(length_seconds), "-i", section["source"],
            "-map", "0:v:0", "-map", f"0:{section['audio']['absolute_stream_index']}",
            "-vf", video_filter,
            "-af", "aresample=48000:async=1:first_pts=0,asetpts=PTS-STARTPTS",
            "-c:v", "ffv1", "-level", "3", "-g", "1", "-pix_fmt", "yuv420p",
            "-c:a", "pcm_s16le", "-ar", "48000", "-ac", "2",
            "-fps_mode", "cfr", str(segment),
        ]
        run(command, log=segment.with_suffix(".log"))
        probe = ffprobe_json(ffprobe, str(segment))
        frames_raw = first_video(probe).get("nb_frames")
        if frames_raw is not None:
            frames = int(frames_raw)
        else:
            frames = ceil_frames(source_duration(probe), output_fps)
        real_sections.append({
            "id": section["id"], "type": section["type"], "name": section["name"],
            "source": section["source"], "sample_start_seconds": start_seconds,
            "sample_length_seconds": length_seconds,
            "start_frame": cursor, "end_frame_exclusive": cursor + frames,
            "frame_count": frames, "return_page": section["return_page"],
        })
        cursor += frames
        segment_paths.append(segment)

    concat_file = workdir / "smoke_concat.txt"
    concat_file.write_text("".join(f"file '{path.as_posix()}'\n" for path in segment_paths), encoding="utf-8")
    output = workdir / "les_miserables_bonus_smoke.mkv"
    run([
        ffmpeg, "-y", "-hide_banner", "-loglevel", "warning",
        "-f", "concat", "-safe", "0", "-i", str(concat_file),
        "-c", "copy", str(output),
    ], log=workdir / "smoke_concat.log")
    smoke_probe = ffprobe_json(ffprobe, str(output))
    smoke_sections = {
        "schema": SCHEMA_SECTIONS, "kind": "smoke",
        "fps": plan["output"]["fps"], "total_frames": cursor,
        "sections": real_sections, "chapters": [],
        "output": str(output), "sha256": sha256(output),
    }
    write_json(workdir / "les_miserables_smoke_sections.json", smoke_sections)
    write_json(workdir / "les_miserables_smoke_probe.json", smoke_probe)
    print(f"WROTE {output}")
    print(f"WROTE {workdir / 'les_miserables_smoke_sections.json'}")


def encode_smoke(manifest: Path, python_exe: str, repo: Path, workdir: Path, encoder_args: list[str]) -> None:
    source = workdir / "les_miserables_bonus_smoke.mkv"
    if not source.exists():
        die("smoke MKV is missing; run the smoke command first")
    output = workdir / "les_miserables_bonus_smoke.mivf"
    command = [
        python_exe, str(repo / "encode_mivf.py"), str(source), str(output),
        "--width", "400", "--height", "240", "--fps", "24000/1001",
        "--audio-rate", "48000", "--audio-channels", "1", "--audio-codec", "ia4m",
        "--audio-offset-ms", "0", "--keep", "16", "--qp", "24", "--c-qp-offset", "3",
        "--lambda", "10", "--motion-search", "full", "--mv-range", "8",
        "--jobs", "8", "--chunk-frames", "240", "--m2y2", "--no-deploy", "--report-packet-sizes",
    ]
    command.extend(encoder_args)
    run(command, log=workdir / "les_miserables_bonus_smoke_encode.log")
    print(f"WROTE {output}")


def audit_mivf(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    if data[:4] != b"MIVF":
        die(f"bad MIVF magic: {path}")
    u16 = lambda off: struct.unpack_from("<H", data, off)[0]
    u32 = lambda off: struct.unpack_from("<I", data, off)[0]
    u64 = lambda off: struct.unpack_from("<Q", data, off)[0]
    stream_count = u32(12)
    duration_ticks = u64(20)
    first_page = u64(36)
    pos = 64
    video_sid = audio_sid = None
    fps_num = fps_den = None
    audio_descriptor = None
    codec = None
    for _ in range(stream_count):
        sid = data[pos]
        kind = data[pos + 1]
        size = u16(pos + 2)
        if kind == 1:
            video_sid = sid
            codec = data[pos + 4:pos + 8].decode("ascii", errors="replace")
            fps_num, fps_den = u16(pos + 20), u16(pos + 22)
        elif kind == 2:
            audio_sid = sid
            extra = pos + 32
            audio_descriptor = (u32(extra + 4), u16(extra + 8), u16(extra + 10))
        pos += size
    if pos != first_page or video_sid is None or audio_sid is None:
        die("invalid stream descriptors or first-page offset")
    page_pos = first_page
    video_packets = audio_packets = pages = 0
    while page_pos + 32 <= len(data) and data[page_pos:page_pos + 2] == b"MP":
        sequence = u32(page_pos + 4)
        pts = u64(page_pos + 8)
        if pts != sequence * 30000 * 1001 // 24000:
            die(f"PTS mismatch at page {sequence}")
        payload = u32(page_pos + 16)
        packet_count = u16(page_pos + 20)
        packet_pos = page_pos + 32
        page_end = packet_pos + payload
        for _ in range(packet_count):
            sid = data[packet_pos]
            header_size = u16(packet_pos + 2)
            packet_size = u32(packet_pos + 8)
            body = packet_pos + header_size
            packet_end = body + packet_size
            if packet_end > page_end:
                die("packet crosses page boundary")
            if sid == video_sid:
                video_packets += 1
            elif sid == audio_sid:
                if data[body:body + 4] != b"IA4M" or u16(body + 8) != 2002:
                    die("invalid IA4M packet or sample count")
                audio_packets += 1
            packet_pos = packet_end
        if packet_pos != page_end:
            die("page payload size mismatch")
        pages += 1
        page_pos = page_end
    expected_duration = pages * 30000 * 1001 // 24000
    if duration_ticks != expected_duration:
        die(f"duration mismatch: {duration_ticks} != {expected_duration}")
    if video_packets != pages or audio_packets != pages:
        die(f"packet count mismatch: pages={pages} video={video_packets} audio={audio_packets}")
    return {
        "path": str(path), "bytes": len(data), "sha256": sha256(path), "codec": codec,
        "fps": f"{fps_num}/{fps_den}", "audio_descriptor": audio_descriptor,
        "pages": pages, "video_packets": video_packets, "audio_packets": audio_packets,
        "duration_ticks": duration_ticks, "duration_seconds": duration_ticks / 30000.0,
        "result": "PASS",
    }


def audit_smoke(workdir: Path) -> None:
    mkv = workdir / "les_miserables_bonus_smoke.mkv"
    mivf = workdir / "les_miserables_bonus_smoke.mivf"
    sections_path = workdir / "les_miserables_smoke_sections.json"
    for path in (mkv, mivf, sections_path):
        if not path.exists():
            die(f"missing smoke artifact: {path}")
    sections = load_json(sections_path)
    previous = 0
    for section in sections["sections"]:
        if section["start_frame"] != previous:
            die(f"noncontiguous section start for {section['id']}")
        if section["end_frame_exclusive"] <= section["start_frame"]:
            die(f"empty section {section['id']}")
        previous = section["end_frame_exclusive"]
    if previous != sections["total_frames"]:
        die("section total does not match final boundary")
    mivf_report = audit_mivf(mivf)

    if sections["total_frames"] != mivf_report["pages"]:
        die(
            "section/MIVF frame-count mismatch: "
            f"sections={sections['total_frames']} "
            f"MIVF={mivf_report['pages']}"
        )

    for section in sections["sections"]:
        if section["end_frame_exclusive"] > mivf_report["pages"]:
            die(
                f"section {section['id']} extends past MIVF: "
                f"{section['end_frame_exclusive']} > "
                f"{mivf_report['pages']}"
            )

    report = {
        "schema": "mivf-dvd-smoke-audit-v1",
        "mkv": {
            "path": str(mkv),
            "bytes": mkv.stat().st_size,
            "sha256": sha256(mkv),
        },
        "mivf": mivf_report,
        "sections": {
            "count": len(sections["sections"]),
            "total_frames": sections["total_frames"],
            "contiguous": True,
            "matches_mivf_pages": True,
        },
        "result": "PASS",
    }
    write_json(workdir / "les_miserables_smoke_audit.json", report)
    print(json.dumps(report, indent=2))


def full_build(manifest: Path, ffmpeg: str, ffprobe: str, workdir: Path, allow_full: bool) -> None:
    if not allow_full:
        die("full compilation is gated; rerun with --allow-full only after smoke validation and visual review")
    die("full compilation is intentionally not implemented in v1; use the validated smoke outputs to design the streaming/full builder")


def main() -> int:
    parser = argparse.ArgumentParser(description="Plan and validate a one-file MIVF DVD bonus-feature compilation")
    parser.add_argument("command", choices=["plan", "smoke", "encode-smoke", "audit-smoke", "full"])
    parser.add_argument("--manifest", default="les_miserables_dvd_selection.json")
    parser.add_argument("--workdir", default="converted_dvd_bonus_smoke")
    parser.add_argument("--ffmpeg")
    parser.add_argument("--ffprobe")
    parser.add_argument("--python", dest="python_exe", default=sys.executable)
    parser.add_argument("--repo", default=".")
    parser.add_argument("--main-smoke-seconds", type=int, default=120)
    parser.add_argument("--bonus-smoke-seconds", type=int, default=60)
    parser.add_argument("--allow-full", action="store_true")
    parser.add_argument("--encoder-arg", action="append", default=[], help="extra argument appended to encode_mivf.py; repeatable")
    args = parser.parse_args()

    manifest = Path(args.manifest).resolve()
    workdir = Path(args.workdir).resolve()
    repo = Path(args.repo).resolve()
    ffmpeg = require_tool("ffmpeg", args.ffmpeg)
    ffprobe = require_tool("ffprobe", args.ffprobe)

    if args.command == "plan":
        plan, _ = build_plan(manifest, ffprobe, workdir)
        print((workdir / "les_miserables_compile_plan.txt").read_text(encoding="utf-8"))
    elif args.command == "smoke":
        build_smoke(manifest, ffmpeg, ffprobe, workdir, args.main_smoke_seconds, args.bonus_smoke_seconds)
    elif args.command == "encode-smoke":
        encode_smoke(manifest, args.python_exe, repo, workdir, args.encoder_arg)
    elif args.command == "audit-smoke":
        audit_smoke(workdir)
    elif args.command == "full":
        full_build(manifest, ffmpeg, ffprobe, workdir, args.allow_full)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
