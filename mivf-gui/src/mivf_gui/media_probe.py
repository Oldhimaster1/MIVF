"""E.1.1: MediaProbe Foundation.

One authoritative desktop media-analysis service, reused by every later
feature that needs to know facts about a source file (Movie Information,
storage estimates, encode comparison, language/subtitle editions, queue
validation) instead of each building its own ffprobe wrapper.

Audited before writing this:

- encode_mivf.py never calls ffprobe at all -- only ffmpeg, for raw
  extraction pipes (mivf_ffmpeg_path(), run_ffmpeg_extract(),
  start_ffmpeg_raw_pipe(), start_ffmpeg_audio_pipe()). MediaProbe is a new
  tool dependency (ffprobe), not a reuse of an existing one.
- Video/audio stream selection in the real encoder is hardcoded to the
  first stream of each type (`-map 0:v:0`, `-map 0:a:0`, encode_mivf.py's
  start_ffmpeg_raw_pipe()/start_ffmpeg_audio_pipe()) -- there is no
  existing explicit stream-selection contract to reuse. A future
  E.3.2 phase needing to select a *different* stream must design and add
  that to the encoder CLI first; MediaProbe only reports what exists.
- Subtitles have no encoder-side representation at all -- the player loads
  them from a separate sidecar at runtime (source/main.c's
  hfix58s_subtitles_load_for_video()), not from the source video's own
  embedded subtitle streams. MediaProbe still reports embedded subtitle
  streams (useful metadata), but no code path here or in the encoder
  extracts or uses them.
- The encoder's own job-recovery fingerprint (encode_mivf.py's
  e0_settings_fingerprint()/e0_sha256()) already defines the real source
  identity model this project uses elsewhere: absolute path, size, mtime,
  and a full chunked SHA-256 (1 MiB blocks). SourceIdentity below mirrors
  that shape/hash algorithm exactly (not a divergent alternative) so a
  future E.5 queue can compare a MediaProbe identity against a job
  manifest's fingerprint directly.
- GUI worker-thread pattern (QThread + succeeded/failed signals) already
  established in theme_export_dialog.py/preflight_dialog.py -- a future
  GUI screen (E.1.2) should wrap probe_media_cached() the same way,
  not invent a new threading pattern.

This module never imports encode_mivf.py (mivf-gui only ever invokes it as
a subprocess -- see backend.py); the ffprobe/ffmpeg discovery logic below
is a deliberate, small, independent duplication of encode_mivf.py's own
mivf_ffmpeg_path() cascade, not a divergent reimplementation of it.
"""
from __future__ import annotations

import dataclasses
import datetime
import hashlib
import json
import shutil
import subprocess
import sys
from fractions import Fraction
from pathlib import Path
from typing import Any

PROBE_VERSION = "mivf-mediaprobe-v1"
DEFAULT_TIMEOUT_S = 30.0

TEXT_SUBTITLE_CODECS = {"subrip", "srt", "ass", "ssa", "webvtt", "mov_text", "text", "ttml"}
BITMAP_SUBTITLE_CODECS = {"dvd_subtitle", "dvb_subtitle", "hdmv_pgs_subtitle", "xsub"}


class MediaProbeError(RuntimeError):
    """Raised only for hard preconditions that make probing impossible at
    all (missing file, tool not found, ffprobe execution/timeout failure,
    unparseable JSON). Per-field anomalies inside otherwise-valid ffprobe
    output become entries in MediaProbeResult.warnings instead -- this
    module never crashes on a single missing/odd field, and never invents
    a value it can't measure."""


# --- data model --------------------------------------------------------

@dataclasses.dataclass(frozen=True)
class SourceIdentity:
    path: str
    canonical_path: str
    size_bytes: int
    mtime_ns: int
    sha256: str | None  # None when compute_hash=False was requested


@dataclasses.dataclass(frozen=True)
class VideoStreamInfo:
    absolute_index: int
    stream_index: int | None  # 0-based index among video streams only
    codec: str
    profile: str | None
    pixel_format: str | None
    width: int | None
    height: int | None
    display_aspect_ratio: str | None
    sample_aspect_ratio: str | None
    avg_frame_rate: str | None       # raw ffprobe string, e.g. "24000/1001"
    avg_frame_rate_num: int | None
    avg_frame_rate_den: int | None
    real_frame_rate: str | None      # ffprobe's r_frame_rate, raw string
    color_space: str | None
    color_primaries: str | None
    color_transfer: str | None
    bitrate_bps: int | None
    is_default: bool
    is_attached_pic: bool
    language: str | None
    title: str | None
    frame_count: int | None
    frame_count_source: str  # "reported" | "unavailable" -- never estimated


@dataclasses.dataclass(frozen=True)
class AudioStreamInfo:
    absolute_index: int
    stream_index: int | None
    codec: str
    sample_rate: int | None
    channels: int | None
    channel_layout: str | None
    bitrate_bps: int | None
    language: str | None
    title: str | None
    is_default: bool


@dataclasses.dataclass(frozen=True)
class SubtitleStreamInfo:
    absolute_index: int
    stream_index: int | None
    codec: str
    kind: str  # "text" | "bitmap" | "unknown"
    language: str | None
    title: str | None
    is_default: bool
    is_forced: bool
    is_hearing_impaired: bool


@dataclasses.dataclass(frozen=True)
class ChapterInfo:
    index: int
    time_base: str
    start: int
    end: int
    start_seconds: float
    end_seconds: float
    title: str | None


@dataclasses.dataclass(frozen=True)
class MediaProbeResult:
    identity: SourceIdentity
    container_format: str | None
    duration_seconds: float | None
    bitrate_bps: int | None
    container_tags: dict[str, str]
    video_streams: tuple[VideoStreamInfo, ...]
    audio_streams: tuple[AudioStreamInfo, ...]
    subtitle_streams: tuple[SubtitleStreamInfo, ...]
    chapters: tuple[ChapterInfo, ...]
    probe_version: str
    probe_timestamp: str
    ffprobe_version: str | None
    warnings: tuple[str, ...]
    errors: tuple[str, ...]
    raw: dict | None = dataclasses.field(default=None, repr=False, compare=False)

    @property
    def ok(self) -> bool:
        return not self.errors

    def to_json_dict(self) -> dict[str, Any]:
        """JSON-serializable representation (drops the raw ffprobe payload
        and any non-primitive fields; every dataclass here is already
        primitive-only aside from `raw`)."""
        d = dataclasses.asdict(self)
        d.pop("raw", None)
        return d


# --- tool discovery ------------------------------------------------------

def _resource_dir() -> Path:
    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        return Path(sys._MEIPASS)
    return Path(__file__).resolve().parent


def _discover_tool(name: str) -> str:
    """Mirrors encode_mivf.py's mivf_ffmpeg_path() candidate cascade
    exactly (bundled resource dir -> frozen exe dir -> PATH), generalized
    to any tool name so it covers both ffmpeg and ffprobe."""
    candidates = [
        _resource_dir() / f"{name}.exe",
        _resource_dir() / name,
        Path(sys.executable).resolve().parent / f"{name}.exe",
        Path(sys.executable).resolve().parent / name,
    ]
    if getattr(sys, "frozen", False):
        exe_dir = Path(sys.executable).resolve().parent
        candidates = [exe_dir / f"{name}.exe", exe_dir / name] + candidates
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    found = shutil.which(name) or shutil.which(f"{name}.exe")
    if found:
        return found
    raise MediaProbeError(
        f"{name} was not found. Install {name}, add it to PATH, or place {name}.exe "
        f"next to encode_mivf.exe / encode_mivf.py."
    )


def ffprobe_path() -> str:
    return _discover_tool("ffprobe")


# --- hashing (matches encode_mivf.py's e0_sha256 exactly) -----------------

def _hash_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def source_identity(path: str | Path, *, compute_hash: bool = True) -> SourceIdentity:
    p = Path(path)
    if not p.is_file():
        raise MediaProbeError(f"Source file does not exist: {p}")
    try:
        canonical = p.resolve()
    except OSError as e:
        raise MediaProbeError(f"Could not resolve path: {e}") from e
    st = p.stat()
    sha = _hash_file(p) if compute_hash else None
    return SourceIdentity(path=str(p), canonical_path=str(canonical),
                           size_bytes=st.st_size, mtime_ns=st.st_mtime_ns, sha256=sha)


# --- parsing ---------------------------------------------------------------

def _int(v: Any) -> int | None:
    try:
        if v is None:
            return None
        return int(v)
    except (TypeError, ValueError):
        return None


def _float(v: Any) -> float | None:
    try:
        if v is None:
            return None
        return float(v)
    except (TypeError, ValueError):
        return None


def _parse_rational(s: str | None) -> tuple[int | None, int | None]:
    """ffprobe reports rates like "24000/1001" or "0/0" for "unknown"."""
    if not s or s in ("0/0", "N/A"):
        return None, None
    try:
        if "/" in s:
            num_s, den_s = s.split("/", 1)
            num, den = int(num_s), int(den_s)
        else:
            frac = Fraction(s).limit_denominator(1_000_000)
            num, den = frac.numerator, frac.denominator
        if den == 0:
            return None, None
        return num, den
    except (ValueError, ZeroDivisionError):
        return None, None


def _subtitle_kind(codec: str) -> str:
    c = (codec or "").lower()
    if c in TEXT_SUBTITLE_CODECS:
        return "text"
    if c in BITMAP_SUBTITLE_CODECS:
        return "bitmap"
    return "unknown"


def parse_ffprobe_json(raw: dict[str, Any], identity: SourceIdentity, *,
                        ffprobe_version: str | None = None) -> MediaProbeResult:
    """Pure parsing, no subprocess/tool access -- the whole reason
    probe_media() and this function are split, so tests never need a real
    ffprobe binary (per the definition-of-done: 'no need to encode media
    during MediaProbe tests')."""
    warnings: list[str] = []
    errors: list[str] = []
    fmt = raw.get("format") or {}
    streams = raw.get("streams") or []
    chapters_raw = raw.get("chapters") or []
    # Container-level tags (title/genre/date/comment/etc, whatever the
    # source file actually embeds) -- E.1.2's only source of "real"
    # presentation metadata; never invented, never guessed.
    container_tags = {str(k): str(v) for k, v in (fmt.get("tags") or {}).items() if v is not None}

    video_streams: list[VideoStreamInfo] = []
    audio_streams: list[AudioStreamInfo] = []
    subtitle_streams: list[SubtitleStreamInfo] = []
    video_i = audio_i = subtitle_i = 0

    for s in streams:
        codec_type = s.get("codec_type")
        disposition = s.get("disposition") or {}
        tags = s.get("tags") or {}
        abs_index = _int(s.get("index"))
        if abs_index is None:
            warnings.append(f"A stream is missing its index and was skipped ({s.get('codec_name', 'unknown codec')})")
            continue

        if codec_type == "video":
            num, den = _parse_rational(s.get("avg_frame_rate"))
            if num is None:
                warnings.append(f"Video stream {abs_index}: avg_frame_rate unavailable/unparseable ({s.get('avg_frame_rate')!r})")
            nb_frames = _int(s.get("nb_frames"))
            is_attached_pic = bool(disposition.get("attached_pic"))
            video_streams.append(VideoStreamInfo(
                absolute_index=abs_index, stream_index=video_i,
                codec=s.get("codec_name") or "unknown", profile=s.get("profile"),
                pixel_format=s.get("pix_fmt"), width=_int(s.get("width")), height=_int(s.get("height")),
                display_aspect_ratio=s.get("display_aspect_ratio"), sample_aspect_ratio=s.get("sample_aspect_ratio"),
                avg_frame_rate=s.get("avg_frame_rate"), avg_frame_rate_num=num, avg_frame_rate_den=den,
                real_frame_rate=s.get("r_frame_rate"),
                color_space=s.get("color_space"), color_primaries=s.get("color_primaries"),
                color_transfer=s.get("color_transfer"),
                bitrate_bps=_int(s.get("bit_rate")), is_default=bool(disposition.get("default")),
                is_attached_pic=is_attached_pic,
                language=tags.get("language"), title=tags.get("title"),
                frame_count=nb_frames, frame_count_source="reported" if nb_frames is not None else "unavailable",
            ))
            video_i += 1

        elif codec_type == "audio":
            audio_streams.append(AudioStreamInfo(
                absolute_index=abs_index, stream_index=audio_i,
                codec=s.get("codec_name") or "unknown", sample_rate=_int(s.get("sample_rate")),
                channels=_int(s.get("channels")), channel_layout=s.get("channel_layout"),
                bitrate_bps=_int(s.get("bit_rate")), language=tags.get("language"), title=tags.get("title"),
                is_default=bool(disposition.get("default")),
            ))
            audio_i += 1

        elif codec_type == "subtitle":
            codec = s.get("codec_name") or "unknown"
            subtitle_streams.append(SubtitleStreamInfo(
                absolute_index=abs_index, stream_index=subtitle_i, codec=codec,
                kind=_subtitle_kind(codec), language=tags.get("language"), title=tags.get("title"),
                is_default=bool(disposition.get("default")), is_forced=bool(disposition.get("forced")),
                is_hearing_impaired=bool(disposition.get("hearing_impaired")),
            ))
            subtitle_i += 1
        # "data"/"attachment" streams intentionally unrepresented -- not needed by any selected feature yet.

    chapters: list[ChapterInfo] = []
    prev_end = None
    for idx, c in enumerate(chapters_raw):
        start = _int(c.get("start"))
        end = _int(c.get("end"))
        start_s = _float(c.get("start_time"))
        end_s = _float(c.get("end_time"))
        title = (c.get("tags") or {}).get("title")
        if start is not None and end is not None and end < start:
            warnings.append(f"Chapter {idx}: end ({end}) precedes start ({start})")
        if prev_end is not None and start is not None and start < prev_end:
            warnings.append(f"Chapter {idx}: overlaps the previous chapter")
        chapters.append(ChapterInfo(
            index=idx, time_base=c.get("time_base") or "1/1",
            start=start if start is not None else 0, end=end if end is not None else 0,
            start_seconds=start_s if start_s is not None else 0.0,
            end_seconds=end_s if end_s is not None else 0.0, title=title,
        ))
        prev_end = end

    if not video_streams and not audio_streams:
        warnings.append("No video or audio streams were found in this source")

    return MediaProbeResult(
        identity=identity,
        container_format=fmt.get("format_name"),
        duration_seconds=_float(fmt.get("duration")),
        bitrate_bps=_int(fmt.get("bit_rate")),
        container_tags=container_tags,
        video_streams=tuple(video_streams), audio_streams=tuple(audio_streams),
        subtitle_streams=tuple(subtitle_streams), chapters=tuple(chapters),
        probe_version=PROBE_VERSION,
        probe_timestamp=datetime.datetime.now().astimezone().isoformat(),
        ffprobe_version=ffprobe_version, warnings=tuple(warnings), errors=tuple(errors), raw=raw,
    )


# --- process invocation ----------------------------------------------------

def _tool_version(tool: str, timeout: float) -> str | None:
    try:
        proc = subprocess.run([tool, "-version"], capture_output=True, text=True,
                               timeout=min(timeout, 5.0), shell=False)
        text = (proc.stdout or proc.stderr or "").strip()
        return text.splitlines()[0] if text else None
    except Exception:  # noqa: BLE001 -- version string is a diagnostic nicety, never fatal
        return None


def probe_media(path: str | Path, *, timeout: float = DEFAULT_TIMEOUT_S,
                 compute_hash: bool = True, ffprobe: str | Path | None = None) -> MediaProbeResult:
    """Runs the real ffprobe (argument array, never shell=True) and parses
    its JSON output. Raises MediaProbeError for any precondition that makes
    probing impossible at all; per-field anomalies in otherwise-valid
    output become MediaProbeResult.warnings instead (see parse_ffprobe_json
    and MediaProbeError's own docstring)."""
    identity = source_identity(path, compute_hash=compute_hash)
    tool = str(ffprobe) if ffprobe else ffprobe_path()

    argv = [tool, "-v", "error", "-print_format", "json",
            "-show_format", "-show_streams", "-show_chapters", identity.canonical_path]
    try:
        proc = subprocess.run(argv, capture_output=True, text=True, timeout=timeout, shell=False)
    except subprocess.TimeoutExpired as e:
        raise MediaProbeError(f"ffprobe timed out after {timeout}s probing {identity.path}") from e
    except OSError as e:
        raise MediaProbeError(f"Could not run ffprobe: {e}") from e

    if proc.returncode != 0:
        raise MediaProbeError(
            f"ffprobe failed (exit {proc.returncode}) probing {identity.path}: {(proc.stderr or '').strip()[:2000]}"
        )
    try:
        raw = json.loads(proc.stdout)
    except json.JSONDecodeError as e:
        raise MediaProbeError(f"ffprobe produced invalid JSON for {identity.path}: {e}") from e
    if not isinstance(raw, dict):
        raise MediaProbeError(f"ffprobe produced a non-object JSON root for {identity.path}")

    return parse_ffprobe_json(raw, identity, ffprobe_version=_tool_version(tool, timeout))


# --- caching ---------------------------------------------------------------

_cache: dict[tuple, MediaProbeResult] = {}


def clear_cache() -> None:
    _cache.clear()


def probe_media_cached(path: str | Path, *, timeout: float = DEFAULT_TIMEOUT_S,
                        compute_hash: bool = True, ffprobe: str | Path | None = None) -> MediaProbeResult:
    """Same as probe_media(), cached by (canonical path, size, mtime,
    compute_hash, probe version) -- a source that hasn't changed on disk is
    never re-probed; any change to its identity (or a version bump of this
    module's own parsing) always produces a fresh probe."""
    p = Path(path)
    if not p.is_file():
        raise MediaProbeError(f"Source file does not exist: {p}")
    st = p.stat()
    key = (str(p.resolve()), st.st_size, st.st_mtime_ns, compute_hash, PROBE_VERSION)
    cached = _cache.get(key)
    if cached is not None:
        return cached
    result = probe_media(p, timeout=timeout, compute_hash=compute_hash, ffprobe=ffprobe)
    _cache[key] = result
    return result
