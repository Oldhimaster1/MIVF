"""E.3.1: Device Storage Planner tests. Pure model only -- no GUI, no real
encode, no ffprobe required (the source-probing path is exercised via
mocking probe_media_cached, same pattern as test_movie_info.py)."""
import json
import sys
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))

from PIL import Image  # noqa: E402
from mivf_gui.project import MivfProject  # noqa: E402
from mivf_gui.media_probe import MediaProbeResult, SourceIdentity, VideoStreamInfo  # noqa: E402
from mivf_gui import storage_plan as sp  # noqa: E402


def _fake_probe(duration_seconds, size_bytes=5_000_000, fps=(24000, 1001)) -> MediaProbeResult:
    identity = SourceIdentity(path="x", canonical_path="x", size_bytes=size_bytes, mtime_ns=0, sha256=None)
    video = VideoStreamInfo(
        absolute_index=0, stream_index=0, codec="h264", profile=None, pixel_format=None,
        width=400, height=240, display_aspect_ratio=None, sample_aspect_ratio=None,
        avg_frame_rate=f"{fps[0]}/{fps[1]}", avg_frame_rate_num=fps[0], avg_frame_rate_den=fps[1],
        real_frame_rate=None, color_space=None, color_primaries=None, color_transfer=None,
        bitrate_bps=None, is_default=True, is_attached_pic=False, language=None, title=None,
        frame_count=None, frame_count_source="unavailable",
    )
    return MediaProbeResult(
        identity=identity, container_format="matroska", duration_seconds=duration_seconds, bitrate_bps=None,
        container_tags={}, video_streams=(video,), audio_streams=(), subtitle_streams=(), chapters=(),
        probe_version="x", probe_timestamp="x", ffprobe_version=None, warnings=(), errors=(),
    )


def _make_project(tmp_path: Path, with_source=True, with_output=False, output_size=0) -> MivfProject:
    proj = MivfProject()
    if with_source:
        src = tmp_path / "movie.mkv"
        src.write_bytes(b"x" * 1000)
        proj.source_media = str(src)
    proj.output_path = str(tmp_path / "out.mivf")
    if with_output:
        Path(proj.output_path).write_bytes(b"y" * output_size)
    proj.save(tmp_path / "test.mivfproj")
    return proj


# --- classification correctness: unknowns don't crash, degrade honestly --

def test_no_source_no_output_everything_degrades_to_unknown(tmp_path):
    proj = MivfProject()
    proj.save(tmp_path / "empty.mivfproj")
    plan = sp.build_storage_plan(proj)
    assert plan.source_size.classification == sp.SizeClass.UNKNOWN
    assert plan.existing_output_size.classification == sp.SizeClass.UNKNOWN
    assert plan.estimated_video_bytes.classification == sp.SizeClass.UNKNOWN
    assert plan.working_space_required.classification == sp.SizeClass.UNKNOWN
    # theme sidecar: unsaved-relative-destination case still must not crash
    assert plan.theme_sidecar_size.bytes is None


def test_missing_source_file_is_unknown_not_a_crash(tmp_path):
    proj = MivfProject()
    proj.source_media = str(tmp_path / "does_not_exist.mkv")
    proj.save(tmp_path / "test.mivfproj")
    plan = sp.build_storage_plan(proj)
    assert plan.source_size.classification == sp.SizeClass.UNKNOWN


def test_real_source_size_is_known_exact(tmp_path):
    proj = _make_project(tmp_path)
    with patch.object(sp, "probe_media_cached", return_value=_fake_probe(100.0)):
        plan = sp.build_storage_plan(proj)
    assert plan.source_size.classification == sp.SizeClass.KNOWN_EXACT
    assert plan.source_size.bytes == 5_000_000


def test_existing_output_is_measured(tmp_path):
    proj = _make_project(tmp_path, with_output=True, output_size=12345)
    plan = sp.build_storage_plan(proj)
    assert plan.existing_output_size.classification == sp.SizeClass.MEASURED
    assert plan.existing_output_size.bytes == 12345


def test_video_estimate_is_unknown_without_a_calibration_anchor(tmp_path):
    proj = _make_project(tmp_path, with_output=False)
    with patch.object(sp, "probe_media_cached", return_value=_fake_probe(100.0)):
        plan = sp.build_storage_plan(proj)
    assert plan.estimated_video_bytes.classification == sp.SizeClass.UNKNOWN
    assert plan.estimated_output_size.classification == sp.SizeClass.UNKNOWN


def test_video_estimate_is_approximate_projection_with_a_real_existing_output(tmp_path):
    proj = _make_project(tmp_path, with_output=True, output_size=50_000_000)
    with patch.object(sp, "probe_media_cached", return_value=_fake_probe(100.0)):
        plan = sp.build_storage_plan(proj)
    assert plan.estimated_video_bytes.classification == sp.SizeClass.APPROXIMATE_PROJECTION
    assert plan.estimated_video_bytes.bytes == 50_000_000 - plan.estimated_audio_bytes.bytes
    assert plan.working_space_required.bytes == plan.estimated_video_bytes.bytes * 2
    assert any("calibrated from this project's own existing output" in a for a in plan.assumptions)


# --- audio formula: exact, hand-verifiable ---------------------------------

def test_pc16_audio_formula_is_exact():
    est = sp._estimate_audio_bytes(10.0, rate=48000, channels=2, codec="pc16", fps_num=24, fps_den=1)
    assert est.classification == sp.SizeClass.ESTIMATED
    assert est.bytes == 10 * 48000 * 2 * 2  # duration * rate * channels * 2 bytes/sample


def test_ia4m_audio_formula_matches_hand_computation():
    # duration=100s, fps=24000/1001 -> frame_count = round(100*24000/1001) = 2398
    # samples_per_frame = round(16000*1001/24000) = 667
    # per_packet = 10 + 2 + ceil((667-1)*4/8) = 12 + 333 = 345
    est = sp._estimate_audio_bytes(100.0, rate=16000, channels=1, codec="ia4m", fps_num=24000, fps_den=1001)
    assert est.bytes == 345 * 2398


def test_audio_estimate_unknown_without_frame_rate():
    est = sp._estimate_audio_bytes(100.0, rate=16000, channels=1, codec="ia4m", fps_num=0, fps_den=1)
    assert est.classification == sp.SizeClass.UNKNOWN


def test_audio_estimate_unknown_for_zero_duration():
    est = sp._estimate_audio_bytes(0.0, rate=16000, channels=1, codec="ia4m", fps_num=24, fps_den=1)
    assert est.classification == sp.SizeClass.UNKNOWN


# --- theme sidecar: reuses PackagePlan, never recomputes -------------------

def test_theme_sidecar_matches_packageplan_exactly(tmp_path):
    from mivf_gui import theme_plan as tp
    specs = {
        "dashboard_bg": (400, 300), "rewind_underlay": (120, 120),
        "play_pause_underlay": (140, 140), "fast_forward_underlay": (120, 120),
        "movie_menu_back": (300, 60),
    }
    artwork = {}
    for key, (w, h) in specs.items():
        p = tmp_path / f"{key}.png"
        Image.new("RGBA", (w, h), (120, 160, 200, 255)).save(p)
        artwork[key] = str(p)
    project_dict = {"schema": "mivf-toolkit-project-v1", "artwork": artwork,
                     "theme": {"accent_rgb": [70, 120, 210], "outline_rgb": [255, 255, 255]}}
    proj_path = tmp_path / "themed.mivfproj"
    proj_path.write_text(json.dumps(project_dict), encoding="utf-8")

    proj = MivfProject.load(proj_path)
    plan = sp.build_storage_plan(proj, theme_destination=tmp_path / "theme_out", theme_basename="themed")
    assert plan.theme_sidecar_size.classification == sp.SizeClass.KNOWN_EXACT

    # cross-check directly against theme_plan.build_plan()'s own numbers
    real_plan = tp.build_plan(proj_path, tmp_path / "theme_out", basename="themed")
    manifest_pf = next(pf for pf in real_plan.files if pf.role == "manifest")
    expected = real_plan.estimated_runtime_bytes + manifest_pf.size
    assert plan.theme_sidecar_size.bytes == expected


def test_theme_sidecar_unknown_when_artwork_incomplete(tmp_path):
    project_dict = {"schema": "mivf-toolkit-project-v1", "artwork": {}, "theme": {}}
    proj_path = tmp_path / "incomplete.mivfproj"
    proj_path.write_text(json.dumps(project_dict), encoding="utf-8")
    proj = MivfProject.load(proj_path)
    plan = sp.build_storage_plan(proj, theme_destination=tmp_path / "theme_out")
    assert plan.theme_sidecar_size.classification == sp.SizeClass.UNKNOWN
    assert "not yet fully configured" in plan.theme_sidecar_size.label


# --- volume / same-volume detection ----------------------------------------

def test_same_volume_warnings_pairwise():
    warnings = sp.same_volume_warnings({"source": 5, "destination": 5, "working": 9})
    assert len(warnings) == 1
    assert "source and destination" in warnings[0]


def test_same_volume_warnings_all_distinct():
    assert sp.same_volume_warnings({"source": 1, "destination": 2, "working": 3}) == []


def test_same_volume_warnings_ignores_unknown_devices():
    assert sp.same_volume_warnings({"source": None, "destination": None}) == []


def test_volume_info_real_directory(tmp_path):
    info = sp._volume_info(tmp_path)
    assert info is not None
    assert info.total_bytes > 0 and info.free_bytes >= 0
    assert info.device_id is not None


def test_volume_info_walks_up_to_existing_ancestor(tmp_path):
    nonexistent = tmp_path / "does" / "not" / "exist" / "yet"
    info = sp._volume_info(nonexistent)
    assert info is not None  # must resolve to tmp_path itself, not fail


# --- insufficient space: projections may legitimately go negative ---------

def test_projected_free_space_can_go_negative():
    tiny_volume = sp.VolumeInfo(path="X:", total_bytes=10_000_000, used_bytes=9_999_000, free_bytes=1000, device_id=1)
    consumed = sp.SizeEstimate(sp.SizeClass.APPROXIMATE_PROJECTION, bytes=50_000_000, label="")
    result = sp._projected_free(tiny_volume, consumed)
    assert result.bytes == 1000 - 50_000_000
    assert result.bytes < 0


def test_projected_free_is_unknown_without_a_volume():
    consumed = sp.SizeEstimate(sp.SizeClass.ESTIMATED, bytes=1000, label="")
    assert sp._projected_free(None, consumed).classification == sp.SizeClass.UNKNOWN


def test_projected_free_is_unknown_without_a_size_estimate():
    volume = sp.VolumeInfo(path="X:", total_bytes=10, used_bytes=5, free_bytes=5, device_id=1)
    assert sp._projected_free(volume, sp.SizeEstimate.unknown("x")).classification == sp.SizeClass.UNKNOWN


# --- FAT32 advisory: pure byte-count checks, never writes a real file -----

def test_fat32_warning_fires_near_4gib():
    warning = sp.fat32_warning_for("estimated output", int(sp.FAT32_MAX_FILE_BYTES * 0.95))
    assert warning is not None and "FAT32" in warning


def test_fat32_warning_does_not_fire_for_small_files():
    assert sp.fat32_warning_for("estimated output", 1000) is None


def test_fat32_warning_integrated_via_a_measured_existing_output(tmp_path):
    # Confirms the integration point without ever writing a multi-GB file:
    # patch existing_output_size classification/bytes onto a tiny real file
    # by asserting the threshold function is what build_storage_plan calls.
    proj = _make_project(tmp_path, with_output=True, output_size=1000)
    with patch.object(sp, "fat32_warning_for", return_value="FAT32 STUBBED WARNING") as mocked:
        plan = sp.build_storage_plan(proj)
    assert mocked.called
    assert "FAT32 STUBBED WARNING" in plan.filesystem_warnings


# --- never deletes, never invents, never encodes ---------------------------

def test_build_storage_plan_never_touches_source_or_output_files(tmp_path):
    proj = _make_project(tmp_path, with_output=True, output_size=999)
    src_bytes_before = Path(proj.resolve(proj.source_media)).read_bytes()
    out_bytes_before = Path(proj.resolve(proj.output_path)).read_bytes()
    sp.build_storage_plan(proj)
    assert Path(proj.resolve(proj.source_media)).read_bytes() == src_bytes_before
    assert Path(proj.resolve(proj.output_path)).read_bytes() == out_bytes_before
