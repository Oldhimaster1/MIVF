"""E.3.1: Device Storage Planner -- a pure planning model, no GUI.

Audited before writing this (encode_mivf.py):

- Working directory: make_temp_workdir() -> tempfile.mkdtemp(prefix=
  "mivf_encode_") in the OS temp dir, UNLESS --job-dir is given (then a
  persistent, user-chosen directory). Exactly one workdir per encode.
- The LIVE video pipeline is build_streaming_parallel_mivf() (called at
  encode_mivf.py:2465) -- NOT build_parallel_mivf() (defined but never
  called; a full "master YUV" dump is dead code, confirmed by grep). Raw
  video chunks are piped from ffmpeg straight into memory
  (read_frame_chunk()), never written to disk as a raw file.
- Encoded output accumulates as per-chunk `segment_NNNNN.mivf` files in the
  workdir. ALL segments coexist simultaneously with a merged
  `temp_video_only.mivf` for the window between merge_video_segments()
  succeeding and the subsequent `segment.unlink()` cleanup loop
  (encode_mivf.py:1888-1940) -- so PEAK working-directory usage is
  approximately 2x the encoded video-only size, not 1x.
  `segment_NNNNN.reconlast` (warm-start) files are read and deleted
  immediately per segment (encode_mivf.py:1814-1818) -- not a sustained
  contributor.
- Audio is muxed by streaming ffmpeg's PCM output directly into the final
  output file (mux_audio_into_mivf(), start_ffmpeg_audio_pipe()) -- no
  separate raw-audio temp file exists in the live path either.
- Output write is NOT atomic: mux_audio_into_mivf() opens `out_path`
  directly in "wb" mode and writes incrementally (encode_mivf.py:2101) --
  a crash or kill mid-mux leaves a partial, invalid file at the real
  destination path. The optional --m2y2 step IS atomic (writes a
  `.m2y2tmp` file *on the destination volume*, then os.replace()s it over
  the real output, encode_mivf.py:2486-2488) -- meaning that step
  temporarily needs roughly 2x the output size of *destination* free
  space, separate from the working-directory requirement above.
- Destination overwrite: NO existence check anywhere in encode_mivf.py --
  `output_path` is opened and truncated unconditionally. There is no
  confirmation, no backup, no guard.
- Job recovery: --job-dir sets E0_KEEP_INTERMEDIATES=True implicitly
  (encode_mivf.py:2696) and the workdir is never shutil.rmtree()'d
  (encode_mivf.py:2577-2578) -- segments/temp_video_only persist
  indefinitely on that volume after the run (success or failure) until a
  user manually cleans it up. --resume-job reuses a valid
  temp_video_only.mivf and skips re-encoding video.
- Audio size IS exactly formula-computable: IA4M is real fixed-rate IMA
  ADPCM at 4 bits/sample after a 16-bit first-sample predictor
  (encode_ia4m_packet(), encode_mivf.py:1202-1226) plus a fixed 10-byte
  packet header ("IA4M" + frame_no u32 + sample-count u16); PC16 is
  uncompressed 16-bit PCM. Per-page/stream container framing beyond that
  was not independently re-verified byte-for-byte here, so the audio
  estimate below is labelled ESTIMATED, not KNOWN EXACT.
- Video size has NO exploitable formula without either running a real
  encode (out of scope for a pure planning model) or a real measured prior
  output to calibrate from -- see build_storage_plan()'s calibration logic.
- shutil.disk_usage(path) is the free-space API; confirmed working on
  this system. Same-volume detection uses os.stat().st_dev (correct and
  identical on Windows and POSIX), not path.drive string comparison.
- FAT32's 4 GiB (4,294,967,295-byte) single-file limit is NOT documented
  or enforced anywhere in this codebase today -- it is included here only
  as a general, well-known filesystem fact, explicitly labelled as
  advisory, never as something this codebase currently checks.

This module NEVER deletes anything, NEVER invents a compression ratio, and
NEVER encodes media. It reuses theme_plan.PackagePlan for exact theme/
sidecar sizes and media_probe.MediaProbeResult for exact source facts --
it does not recompute either.
"""
from __future__ import annotations

import dataclasses
import os
import shutil
import tempfile
from enum import Enum
from pathlib import Path

from . import theme_plan
from .media_probe import MediaProbeError, probe_media_cached

# Real audio-frame constants, cited from encode_mivf.py's encode_ia4m_packet().
IA4M_PACKET_HEADER_BYTES = 10  # "IA4M"(4) + frame_no u32(4) + sample_count u16(2)
IA4M_FIRST_SAMPLE_BYTES = 2    # predictor0, stored as a raw s16
IA4M_BITS_PER_SAMPLE_AFTER_FIRST = 4

FAT32_MAX_FILE_BYTES = 4_294_967_295  # 2**32 - 1; general filesystem fact, not codebase-enforced.


class SizeClass(str, Enum):
    KNOWN_EXACT = "known_exact"
    MEASURED = "measured"
    ESTIMATED = "estimated"
    APPROXIMATE_PROJECTION = "approximate_projection"
    UNKNOWN = "unknown"


@dataclasses.dataclass(frozen=True)
class SizeEstimate:
    classification: SizeClass
    bytes: int | None = None
    label: str = ""
    formula: str | None = None
    low_bytes: int | None = None
    high_bytes: int | None = None

    @staticmethod
    def unknown(reason: str) -> "SizeEstimate":
        return SizeEstimate(classification=SizeClass.UNKNOWN, bytes=None, label=reason)


@dataclasses.dataclass(frozen=True)
class VolumeInfo:
    path: str
    total_bytes: int
    used_bytes: int
    free_bytes: int
    device_id: int | None  # os.stat().st_dev -- correct cross-platform "same volume" key


@dataclasses.dataclass(frozen=True)
class StoragePlan:
    source_size: SizeEstimate
    theme_sidecar_size: SizeEstimate
    existing_output_size: SizeEstimate
    estimated_audio_bytes: SizeEstimate
    estimated_video_bytes: SizeEstimate
    estimated_output_size: SizeEstimate
    working_space_required: SizeEstimate
    job_recovery_allowance: SizeEstimate
    destination_volume: VolumeInfo | None
    working_volume: VolumeInfo | None
    projected_destination_free: SizeEstimate
    projected_working_free: SizeEstimate
    safety_margin_bytes: int
    same_volume_warnings: tuple[str, ...]
    filesystem_warnings: tuple[str, ...]
    assumptions: tuple[str, ...]


def _volume_info(path: Path) -> VolumeInfo | None:
    """shutil.disk_usage() requires an existing directory -- walk up to the
    nearest existing ancestor (a destination directory that doesn't exist
    yet is a completely normal case: the encoder creates it)."""
    p = Path(path).resolve()
    probe = p if p.is_dir() else p.parent
    while not probe.exists():
        parent = probe.parent
        if parent == probe:
            return None
        probe = parent
    try:
        usage = shutil.disk_usage(str(probe))
        device_id = os.stat(str(probe)).st_dev
    except OSError:
        return None
    return VolumeInfo(path=str(probe), total_bytes=usage.total, used_bytes=usage.used,
                       free_bytes=usage.free, device_id=device_id)


def _projected_free(volume: VolumeInfo | None, consumed: SizeEstimate) -> SizeEstimate:
    """Free space is allowed to go negative -- that IS the "insufficient
    space" signal a GUI should show as a warning, not something this pure
    model clamps away."""
    if volume is None:
        return SizeEstimate.unknown("volume free space unavailable")
    if consumed.bytes is None:
        return SizeEstimate.unknown("cannot project remaining space without a size estimate")
    classification = consumed.classification if consumed.classification in (
        SizeClass.APPROXIMATE_PROJECTION, SizeClass.ESTIMATED) else SizeClass.APPROXIMATE_PROJECTION
    remaining = volume.free_bytes - consumed.bytes
    return SizeEstimate(classification, bytes=remaining,
                         label=f"~{remaining:,} bytes remaining after this operation (projection)")


def fat32_warning_for(label: str, size_bytes: int) -> str | None:
    """General FAT32 filesystem fact (the well-known 4 GiB / 2**32-1
    single-file limit), not something this codebase currently checks or
    enforces -- a pure, directly testable function so tests never need to
    actually write a multi-gigabyte file to exercise this threshold."""
    if size_bytes < FAT32_MAX_FILE_BYTES * 0.9:
        return None
    return (
        f"The {label} ({size_bytes:,} bytes) is approaching or exceeds FAT32's 4 GiB "
        f"({FAT32_MAX_FILE_BYTES:,} byte) single-file limit -- relevant if the destination "
        f"(e.g. a 3DS SD card) is FAT32-formatted. This is a general filesystem fact; this "
        f"codebase does not currently check or enforce it."
    )


def same_volume_warnings(device_ids: dict[str, int | None]) -> list[str]:
    """Pairwise comparison of named device IDs (from os.stat().st_dev) --
    a standalone, directly testable function, since genuinely constructing
    two different real volumes isn't possible on every test machine."""
    out: list[str] = []
    names = [n for n, d in device_ids.items() if d is not None]
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            a, b = names[i], names[j]
            if device_ids[a] == device_ids[b]:
                out.append(f"{a} and {b} are on the same volume -- their free space is shared, not independent.")
    return out


def _estimate_audio_bytes(duration_seconds: float, rate: int, channels: int, codec: str,
                          fps_num: int, fps_den: int) -> SizeEstimate:
    """Formula-based, using the real per-packet layout confirmed in
    encode_ia4m_packet(). Container/page framing beyond the packet body
    itself was not independently re-verified, so this stays ESTIMATED
    rather than KNOWN EXACT."""
    if duration_seconds <= 0:
        return SizeEstimate.unknown("source duration unavailable")
    total_samples = int(round(duration_seconds * rate)) * max(1, channels)
    if codec == "pc16":
        raw = total_samples * 2
        return SizeEstimate(SizeClass.ESTIMATED, bytes=raw,
                             label=f"~{raw:,} bytes (PC16: {total_samples:,} samples x 2 bytes, uncompressed)",
                             formula="duration_seconds * rate * channels * 2")
    # ia4m: fixed-rate 4-bit ADPCM per sample after a 16-bit first sample,
    # plus a fixed per-packet header; samples_per_frame determines packet
    # count and is itself derived from the real fixed_audio_samples_per_frame()
    # rule (rate/fps) -- approximated here by frame count at the source fps
    # if known, else treated as one packet per second as a coarse fallback.
    samples_per_frame = None
    if fps_num and fps_den:
        samples_per_frame = round(rate * fps_den / fps_num)
    if not samples_per_frame or samples_per_frame <= 0:
        return SizeEstimate.unknown("cannot estimate IA4M packet count without a known frame rate")
    frame_count = max(1, int(round(duration_seconds * fps_num / fps_den)))
    per_packet = IA4M_PACKET_HEADER_BYTES + IA4M_FIRST_SAMPLE_BYTES + \
        (max(0, samples_per_frame - 1) * IA4M_BITS_PER_SAMPLE_AFTER_FIRST + 7) // 8
    total = per_packet * frame_count
    return SizeEstimate(
        SizeClass.ESTIMATED, bytes=total,
        label=f"~{total:,} bytes (IA4M: {frame_count:,} packets x ~{per_packet} bytes)",
        formula="frame_count * (10-byte header + 2-byte predictor + ceil((samples_per_frame-1)*4 bits))",
    )


def build_storage_plan(project, *, theme_destination: str | Path | None = None,
                       theme_basename: str | None = None, safety_margin_fraction: float = 0.05,
                       safety_margin_floor_bytes: int = 100 * 1024 * 1024) -> StoragePlan:
    """Pure model: takes a MivfProject (and, optionally, where a theme
    package would be exported), returns a fully-classified StoragePlan.
    Never writes, never deletes, never invents a compression ratio.
    Reuses theme_plan.PackagePlan and media_probe.MediaProbeResult as the
    sole sources of truth for theme sizes and source facts."""
    assumptions: list[str] = []
    fs_warnings: list[str] = []

    # --- source ---
    source_path = project.resolve(project.source_media) if project.source_media else None
    probe = None
    if source_path and Path(source_path).is_file():
        try:
            probe = probe_media_cached(source_path, compute_hash=False)
        except MediaProbeError as e:
            probe = None
            assumptions.append(f"Could not probe source media: {e}")
    if probe is not None:
        source_size = SizeEstimate(SizeClass.KNOWN_EXACT, bytes=probe.identity.size_bytes,
                                    label=f"{probe.identity.size_bytes:,} bytes (measured)")
    elif source_path:
        source_size = SizeEstimate.unknown(f"source file not found or unreadable: {source_path}")
    else:
        source_size = SizeEstimate.unknown("no source media set")

    # --- theme/sidecar exact size, via PackagePlan (never recomputed here) ---
    dest_for_theme = theme_destination
    if dest_for_theme is None and project.project_path:
        dest_for_theme = project.project_path.parent
    if project.project_path and dest_for_theme:
        try:
            plan = theme_plan.build_plan(project.project_path, dest_for_theme,
                                          basename=theme_basename or project.project_path.stem)
        except theme_plan.ThemeExportError as e:
            plan = None
            theme_sidecar_size = SizeEstimate.unknown(f"could not build a theme plan: {e}")
        if plan is not None:
            if plan.ok_to_export:
                manifest_pf = next((pf for pf in plan.files if pf.role == "manifest"), None)
                total = plan.estimated_runtime_bytes + (manifest_pf.size if manifest_pf else 0)
                theme_sidecar_size = SizeEstimate(
                    SizeClass.KNOWN_EXACT, bytes=total,
                    label=f"{total:,} bytes (PackagePlan: {len(plan.files)} planned files)",
                )
            else:
                theme_sidecar_size = SizeEstimate.unknown(
                    "theme/artwork is not yet fully configured: " + plan.errors[0].message
                )
    else:
        theme_sidecar_size = SizeEstimate.unknown("project is unsaved or has no theme destination to plan against")

    # --- existing output (measured, if present) ---
    output_path = project.resolve(project.output_path) if project.output_path else None
    if output_path and Path(output_path).is_file():
        size = Path(output_path).stat().st_size
        existing_output_size = SizeEstimate(SizeClass.MEASURED, bytes=size, label=f"{size:,} bytes (measured)")
    elif output_path:
        existing_output_size = SizeEstimate.unknown("no existing output at this project's output path yet")
    else:
        existing_output_size = SizeEstimate.unknown("no output path set")

    # --- estimated final .mivf size: audio (formula) + video (calibrated or unknown) ---
    audio_codec = (project.advanced_overrides or {}).get("audio_codec", "ia4m")
    audio_rate = (project.advanced_overrides or {}).get("audio_rate", 16000)
    audio_channels = (project.advanced_overrides or {}).get("audio_channels", 1)
    if probe is not None and probe.duration_seconds and probe.video_streams:
        v = probe.video_streams[0]
        fps_num = v.avg_frame_rate_num or 0
        fps_den = v.avg_frame_rate_den or 1
        estimated_audio_bytes = _estimate_audio_bytes(probe.duration_seconds, audio_rate, audio_channels,
                                                       audio_codec, fps_num, fps_den)
    else:
        estimated_audio_bytes = SizeEstimate.unknown("source duration/frame rate unavailable")

    if existing_output_size.classification == SizeClass.MEASURED and estimated_audio_bytes.bytes is not None:
        # Calibration anchor: this project's OWN prior completed output,
        # assumed to be for the same source (a genuinely common case --
        # re-encoding the same movie with different settings). This is a
        # real measured data point, not an invented ratio -- but it still
        # assumes similar content/settings, so it stays a labelled
        # projection, never KNOWN EXACT or MEASURED for the *new* run.
        video_estimate = max(0, existing_output_size.bytes - estimated_audio_bytes.bytes)
        estimated_video_bytes = SizeEstimate(
            SizeClass.APPROXIMATE_PROJECTION, bytes=video_estimate,
            label=f"~{video_estimate:,} bytes, projected from this project's existing output at {output_path}",
            formula="existing_output_bytes - estimated_audio_bytes (assumes similar source/settings)",
        )
        assumptions.append("Video size projection is calibrated from this project's own existing output file, "
                            "not an invented compression ratio -- it assumes a similar source and similar settings.")
    else:
        estimated_video_bytes = SizeEstimate.unknown(
            "no real prior output exists to calibrate from; run a real encode (or a future Encode Comparison "
            "sample) to get a measured basis -- no compression ratio is assumed here"
        )

    if estimated_video_bytes.bytes is not None and estimated_audio_bytes.bytes is not None:
        total = estimated_video_bytes.bytes + estimated_audio_bytes.bytes
        combined_class = SizeClass.APPROXIMATE_PROJECTION if estimated_video_bytes.classification == SizeClass.APPROXIMATE_PROJECTION else SizeClass.ESTIMATED
        estimated_output_size = SizeEstimate(combined_class, bytes=total,
                                              label=f"~{total:,} bytes (audio + video components)")
    else:
        estimated_output_size = SizeEstimate.unknown("insufficient data to estimate a final output size")

    # --- working space: peak ~= 2x video-only size (segments + merged copy) ---
    if estimated_video_bytes.bytes is not None:
        working_bytes = estimated_video_bytes.bytes * 2
        working_space_required = SizeEstimate(
            estimated_video_bytes.classification if estimated_video_bytes.classification != SizeClass.APPROXIMATE_PROJECTION else SizeClass.APPROXIMATE_PROJECTION,
            bytes=working_bytes,
            label=f"~{working_bytes:,} bytes (peak: all encoded segments + one merged copy, per audit of build_streaming_parallel_mivf())",
            formula="2 * estimated_video_bytes",
        )
        job_recovery_allowance = SizeEstimate(
            working_space_required.classification, bytes=working_bytes,
            label="Same magnitude as working space -- only persists on disk if --job-dir is used "
                  "(otherwise the working directory is deleted automatically when the encode ends).",
        )
    else:
        working_space_required = SizeEstimate.unknown("cannot size working space without a video estimate")
        job_recovery_allowance = SizeEstimate.unknown("cannot size job-recovery allowance without a video estimate")

    # --- volumes ---
    destination_volume = _volume_info(Path(output_path).parent) if output_path else None
    job_dir = project.resolve(project.job_dir) if project.job_dir else None
    working_dir_for_volume = job_dir if job_dir else Path(tempfile.gettempdir())
    working_volume = _volume_info(working_dir_for_volume)

    ids: dict[str, int | None] = {}
    if source_path:
        sv = _volume_info(Path(source_path).parent)
        if sv:
            ids["source"] = sv.device_id
    if destination_volume:
        ids["destination"] = destination_volume.device_id
    if working_volume:
        ids["working"] = working_volume.device_id
    volume_warnings = same_volume_warnings(ids)

    # Destination consumption: final output + theme sidecar (both land there).
    dest_consumption_bytes = 0
    dest_consumption_known = True
    for est in (estimated_output_size, theme_sidecar_size):
        if est.bytes is None:
            dest_consumption_known = False
        else:
            dest_consumption_bytes += est.bytes
    dest_consumption = (SizeEstimate(SizeClass.APPROXIMATE_PROJECTION, bytes=dest_consumption_bytes, label="")
                         if dest_consumption_known else SizeEstimate.unknown("incomplete data"))
    projected_destination_free = _projected_free(destination_volume, dest_consumption)
    projected_working_free = _projected_free(working_volume, working_space_required)

    safety_margin_bytes = 0
    if estimated_output_size.bytes is not None:
        safety_margin_bytes = max(int(estimated_output_size.bytes * safety_margin_fraction), safety_margin_floor_bytes)
    else:
        safety_margin_bytes = safety_margin_floor_bytes
    assumptions.append(
        f"Safety margin is a heuristic buffer ({safety_margin_fraction:.0%} of the estimated output, "
        f"floor {safety_margin_floor_bytes:,} bytes) -- not a measured requirement."
    )

    for est, label in ((estimated_output_size, "estimated output"), (existing_output_size, "existing output")):
        if est.bytes is not None:
            warning = fat32_warning_for(label, est.bytes)
            if warning:
                fs_warnings.append(warning)

    return StoragePlan(
        source_size=source_size, theme_sidecar_size=theme_sidecar_size,
        existing_output_size=existing_output_size,
        estimated_audio_bytes=estimated_audio_bytes, estimated_video_bytes=estimated_video_bytes,
        estimated_output_size=estimated_output_size,
        working_space_required=working_space_required, job_recovery_allowance=job_recovery_allowance,
        destination_volume=destination_volume, working_volume=working_volume,
        projected_destination_free=projected_destination_free, projected_working_free=projected_working_free,
        safety_margin_bytes=safety_margin_bytes,
        same_volume_warnings=tuple(volume_warnings), filesystem_warnings=tuple(fs_warnings),
        assumptions=tuple(assumptions),
    )
