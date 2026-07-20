#!/usr/bin/env python3
"""Read-only DVD/extra inventory tool for the MIVF project.

This first-phase probe does not rip, decrypt, concatenate, or encode anything.
It uses FFprobe's dvdvideo demuxer to inspect logical DVD titles in an ISO or
VIDEO_TS tree, and ordinary FFprobe probing for files in an extras directory.

Example (Windows Python from MSYS):
  python tools/mivf_dvd_probe.py \
    --dvd "\\\\192.168.1.118\\arm\\media\\completed\\movies\\LES_MISERABLES\\LES_MISERABLES.iso" \
    --extras "\\\\192.168.1.118\\arm\\media\\completed\\movies\\Les-Misérables (2012)\\extras" \
    --output les_miserables_dvd_inventory.json
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

MEDIA_EXTENSIONS = {
    ".3g2", ".3gp", ".aac", ".ac3", ".avi", ".flac", ".m2ts", ".m4a",
    ".m4v", ".mka", ".mkv", ".mov", ".mp2", ".mp3", ".mp4", ".mpeg",
    ".mpg", ".mts", ".ogg", ".opus", ".ts", ".vob", ".wav", ".webm",
}


def run_json(command: list[str], timeout: int) -> tuple[dict[str, Any] | None, str, float]:
    started = time.monotonic()
    try:
        proc = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        elapsed = time.monotonic() - started
        return None, f"timeout after {timeout}s: {exc}", elapsed
    elapsed = time.monotonic() - started
    if proc.returncode != 0:
        detail = proc.stderr.strip() or proc.stdout.strip() or f"exit code {proc.returncode}"
        return None, detail, elapsed
    try:
        return json.loads(proc.stdout), proc.stderr.strip(), elapsed
    except json.JSONDecodeError as exc:
        return None, f"invalid JSON from ffprobe: {exc}; stderr={proc.stderr.strip()}", elapsed


def rational_to_float(value: str | None) -> float | None:
    if not value or value in {"0/0", "N/A"}:
        return None
    try:
        if "/" in value:
            a, b = value.split("/", 1)
            return float(a) / float(b)
        return float(value)
    except (ValueError, ZeroDivisionError):
        return None


def duration_seconds(probe: dict[str, Any]) -> float | None:
    raw = probe.get("format", {}).get("duration")
    try:
        return float(raw)
    except (TypeError, ValueError):
        pass
    ends: list[float] = []
    for stream in probe.get("streams", []):
        try:
            ends.append(float(stream.get("duration")))
        except (TypeError, ValueError):
            continue
    return max(ends) if ends else None


def stream_summary(stream: dict[str, Any]) -> dict[str, Any]:
    item: dict[str, Any] = {
        "index": stream.get("index"),
        "codec_type": stream.get("codec_type"),
        "codec_name": stream.get("codec_name"),
        "codec_long_name": stream.get("codec_long_name"),
        "language": stream.get("tags", {}).get("language"),
        "disposition": stream.get("disposition", {}),
    }
    kind = stream.get("codec_type")
    if kind == "video":
        item.update({
            "width": stream.get("width"),
            "height": stream.get("height"),
            "display_aspect_ratio": stream.get("display_aspect_ratio"),
            "sample_aspect_ratio": stream.get("sample_aspect_ratio"),
            "field_order": stream.get("field_order"),
            "pix_fmt": stream.get("pix_fmt"),
            "r_frame_rate": stream.get("r_frame_rate"),
            "avg_frame_rate": stream.get("avg_frame_rate"),
            "fps_approx": rational_to_float(stream.get("avg_frame_rate")),
        })
    elif kind == "audio":
        item.update({
            "sample_rate": stream.get("sample_rate"),
            "channels": stream.get("channels"),
            "channel_layout": stream.get("channel_layout"),
            "bit_rate": stream.get("bit_rate"),
        })
    elif kind == "subtitle":
        item.update({"width": stream.get("width"), "height": stream.get("height")})
    return item


def summarize_probe(probe: dict[str, Any]) -> dict[str, Any]:
    streams = [stream_summary(s) for s in probe.get("streams", [])]
    videos = [s for s in streams if s.get("codec_type") == "video"]
    audios = [s for s in streams if s.get("codec_type") == "audio"]
    subtitles = [s for s in streams if s.get("codec_type") == "subtitle"]
    chapters = []
    for chapter in probe.get("chapters", []):
        chapters.append({
            "id": chapter.get("id"),
            "start_time": chapter.get("start_time"),
            "end_time": chapter.get("end_time"),
            "tags": chapter.get("tags", {}),
        })
    return {
        "duration_seconds": duration_seconds(probe),
        "format_name": probe.get("format", {}).get("format_name"),
        "format_long_name": probe.get("format", {}).get("format_long_name"),
        "format_tags": probe.get("format", {}).get("tags", {}),
        "chapter_count": len(chapters),
        "chapters": chapters,
        "video_streams": videos,
        "audio_streams": audios,
        "subtitle_streams": subtitles,
        "other_streams": [s for s in streams if s.get("codec_type") not in {"video", "audio", "subtitle"}],
    }


def dvd_probe_command(ffprobe: str, source: str, title: int, preindex: bool) -> list[str]:
    command = [
        ffprobe, "-v", "error", "-f", "dvdvideo",
        "-title", str(title), "-angle", "1",
        "-chapter_start", "1", "-chapter_end", "0",
        "-trim", "1",
    ]
    if preindex:
        command += ["-preindex", "1"]
    command += [
        "-show_format", "-show_streams", "-show_chapters",
        "-of", "json", source,
    ]
    return command


def file_probe_command(ffprobe: str, source: str) -> list[str]:
    return [
        ffprobe, "-v", "error", "-show_format", "-show_streams",
        "-show_chapters", "-of", "json", source,
    ]


def classify_duration(seconds: float | None) -> str:
    if seconds is None:
        return "unknown"
    if seconds < 30:
        return "very_short_or_menu_transition"
    if seconds < 5 * 60:
        return "short_extra_or_trailer"
    if seconds < 45 * 60:
        return "featurette_interview_or_episode_extra"
    return "feature_or_episode_candidate"


def rank_titles(titles: list[dict[str, Any]]) -> None:
    valid = [t for t in titles if t.get("ok") and t.get("duration_seconds")]
    if not valid:
        return
    maximum = max(float(t["duration_seconds"]) for t in valid)
    for title in valid:
        duration = float(title["duration_seconds"])
        chapters = int(title.get("chapter_count") or 0)
        video = title.get("video_streams") or []
        audio = title.get("audio_streams") or []
        score = 0.0
        score += 70.0 * (duration / maximum if maximum else 0.0)
        score += min(chapters, 30) * 0.7
        if video:
            score += 3.0
        if any((a.get("channels") or 0) >= 6 for a in audio):
            score += 3.0
        title["classification"] = classify_duration(duration)
        title["main_feature_score"] = round(score, 3)
    valid.sort(key=lambda t: (t.get("main_feature_score", 0), t.get("duration_seconds", 0)), reverse=True)
    for rank, title in enumerate(valid, 1):
        title["main_feature_rank"] = rank
        title["recommended_main_feature"] = rank == 1


def probe_dvd(ffprobe: str, source: str, max_title: int, miss_limit: int, timeout: int, preindex: bool) -> dict[str, Any]:
    result: dict[str, Any] = {
        "source": source,
        "source_exists": os.path.exists(source),
        "titles": [],
        "scan": {"max_title": max_title, "miss_limit": miss_limit, "preindex": preindex},
    }
    misses = 0
    found = 0
    for title_number in range(1, max_title + 1):
        sys.stderr.write(f"Probing DVD title {title_number}...\n")
        probe, diagnostic, elapsed = run_json(
            dvd_probe_command(ffprobe, source, title_number, preindex), timeout
        )
        if probe is None:
            result["titles"].append({
                "title": title_number,
                "ok": False,
                "probe_seconds": round(elapsed, 3),
                "error": diagnostic[-2000:],
            })
            misses += 1
            if found and misses >= miss_limit:
                result["scan"]["stopped_after_consecutive_misses"] = misses
                break
            continue
        summary = summarize_probe(probe)
        summary.update({
            "title": title_number,
            "ok": True,
            "probe_seconds": round(elapsed, 3),
            "ffprobe_diagnostic": diagnostic[-2000:] if diagnostic else "",
        })
        result["titles"].append(summary)
        found += 1
        misses = 0
    rank_titles(result["titles"])
    result["scan"]["successful_titles"] = found
    return result


def probe_extras(ffprobe: str, directory: str, timeout: int) -> dict[str, Any]:
    root = Path(directory)
    result: dict[str, Any] = {
        "directory": directory,
        "directory_exists": root.exists(),
        "files": [],
    }
    if not root.exists():
        return result
    files = sorted(
        (p for p in root.rglob("*") if p.is_file() and p.suffix.lower() in MEDIA_EXTENSIONS),
        key=lambda p: str(p).casefold(),
    )
    for path in files:
        sys.stderr.write(f"Probing extra file: {path}\n")
        probe, diagnostic, elapsed = run_json(file_probe_command(ffprobe, str(path)), timeout)
        base = {
            "path": str(path),
            "relative_path": str(path.relative_to(root)),
            "name": path.stem,
            "extension": path.suffix.lower(),
            "bytes": path.stat().st_size,
            "probe_seconds": round(elapsed, 3),
        }
        if probe is None:
            base.update({"ok": False, "error": diagnostic[-2000:]})
        else:
            base.update({"ok": True, **summarize_probe(probe)})
            base["classification"] = classify_duration(base.get("duration_seconds"))
            if diagnostic:
                base["ffprobe_diagnostic"] = diagnostic[-2000:]
        result["files"].append(base)
    result["file_count"] = len(files)
    return result


def human_duration(seconds: float | None) -> str:
    if seconds is None:
        return "unknown"
    whole = int(round(seconds))
    h, rem = divmod(whole, 3600)
    m, s = divmod(rem, 60)
    return f"{h:02d}:{m:02d}:{s:02d}"


def build_text_report(inventory: dict[str, Any]) -> str:
    lines = [
        "MIVF DVD and Bonus-Feature Inventory",
        "=" * 78,
        f"Generated: {inventory['generated_utc']}",
        f"FFprobe: {inventory['ffprobe']}",
        "",
        "DVD SOURCE",
        "-" * 78,
        inventory["dvd"]["source"],
    ]
    successful = [t for t in inventory["dvd"]["titles"] if t.get("ok")]
    if not successful:
        lines.append("No logical DVD titles were successfully probed.")
    for title in successful:
        videos = title.get("video_streams") or []
        v = videos[0] if videos else {}
        recommendation = "  [RECOMMENDED MAIN FEATURE]" if title.get("recommended_main_feature") else ""
        lines += [
            "",
            f"Title {title['title']}{recommendation}",
            f"  Duration: {human_duration(title.get('duration_seconds'))}",
            f"  Chapters: {title.get('chapter_count', 0)}",
            f"  Classification: {title.get('classification', 'unknown')}",
            f"  Score/rank: {title.get('main_feature_score', 'n/a')} / {title.get('main_feature_rank', 'n/a')}",
            f"  Video: {v.get('codec_name', 'unknown')} {v.get('width', '?')}x{v.get('height', '?')} "
            f"DAR={v.get('display_aspect_ratio', '?')} FPS={v.get('avg_frame_rate', '?')} field={v.get('field_order', '?')}",
            f"  Audio tracks: {len(title.get('audio_streams') or [])}",
            f"  Subtitle tracks: {len(title.get('subtitle_streams') or [])}",
        ]
        for audio in title.get("audio_streams") or []:
            lines.append(
                f"    Audio {audio.get('index')}: {audio.get('codec_name')} "
                f"{audio.get('language') or 'und'} {audio.get('channels') or '?'}ch "
                f"{audio.get('sample_rate') or '?'}Hz"
            )
        for sub in title.get("subtitle_streams") or []:
            lines.append(
                f"    Subtitle {sub.get('index')}: {sub.get('codec_name')} {sub.get('language') or 'und'}"
            )

    lines += ["", "PRE-RIPPED EXTRAS", "-" * 78, inventory["extras"]["directory"]]
    files = inventory["extras"].get("files") or []
    if not files:
        lines.append("No supported media files found, or extras directory unavailable.")
    for item in files:
        if not item.get("ok"):
            lines.append(f"- {item['relative_path']}: PROBE FAILED")
            continue
        videos = item.get("video_streams") or []
        v = videos[0] if videos else {}
        lines += [
            f"- {item['relative_path']}",
            f"    Duration: {human_duration(item.get('duration_seconds'))}",
            f"    Classification: {item.get('classification', 'unknown')}",
            f"    Video: {v.get('codec_name', 'unknown')} {v.get('width', '?')}x{v.get('height', '?')} "
            f"FPS={v.get('avg_frame_rate', '?')}",
            f"    Audio tracks: {len(item.get('audio_streams') or [])}",
            f"    Subtitle tracks: {len(item.get('subtitle_streams') or [])}",
        ]

    lines += [
        "",
        "PLANNING NOTES",
        "-" * 78,
        "This inventory is advisory. It does not automatically decide that the longest title is correct.",
        "The next step is to review the JSON and create an explicit selection manifest.",
        "Pre-ripped extras should usually be preferred over re-extracting identical DVD titles.",
        "No source media was modified by this probe.",
    ]
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Inventory a DVD ISO/VIDEO_TS source and a pre-ripped extras folder")
    parser.add_argument("--dvd", required=True, help="DVD ISO, VIDEO_TS directory, or mounted DVD path")
    parser.add_argument("--extras", required=True, help="directory containing pre-ripped extras")
    parser.add_argument("--output", default="dvd_inventory.json", help="JSON output path")
    parser.add_argument("--ffprobe", default=None, help="ffprobe executable path; defaults to PATH lookup")
    parser.add_argument("--max-title", type=int, default=30, help="highest DVD title number to attempt")
    parser.add_argument("--miss-limit", type=int, default=5, help="stop after this many consecutive misses after finding a title")
    parser.add_argument("--timeout", type=int, default=120, help="timeout in seconds per title/file probe")
    parser.add_argument("--preindex", action="store_true", help="request accurate DVD chapter markers; much slower")
    args = parser.parse_args()

    ffprobe = args.ffprobe or shutil.which("ffprobe")
    if not ffprobe:
        raise SystemExit("ERROR: ffprobe was not found on PATH; pass --ffprobe explicitly")
    if args.max_title < 1 or args.max_title > 99:
        raise SystemExit("ERROR: --max-title must be between 1 and 99")
    if args.miss_limit < 1:
        raise SystemExit("ERROR: --miss-limit must be at least 1")

    output = Path(args.output)
    inventory = {
        "schema": "mivf-dvd-inventory-v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "ffprobe": ffprobe,
        "read_only": True,
        "dvd": probe_dvd(ffprobe, args.dvd, args.max_title, args.miss_limit, args.timeout, args.preindex),
        "extras": probe_extras(ffprobe, args.extras, args.timeout),
    }
    output.write_text(json.dumps(inventory, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    text_output = output.with_suffix(".txt")
    text_output.write_text(build_text_report(inventory), encoding="utf-8")
    print(f"WROTE {output}")
    print(f"WROTE {text_output}")
    print(f"Successful DVD titles: {inventory['dvd']['scan'].get('successful_titles', 0)}")
    print(f"Pre-ripped extras: {inventory['extras'].get('file_count', 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
