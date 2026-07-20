"""GUI/CLI equivalence tests.

Implements the two tests required by
mivf_customization_gui_20260716/ENCODER_GUI_BACKEND_CONTRACT.md:
  1. Argument equivalence: a fixed .mivfproj produces a byte-identical
     argument list to a hand-written expected list (golden-file test).
  2. (Documented, not run here) Output equivalence: running the GUI-driven
     encode and an equivalent direct CLI invocation on the same input must
     produce byte-identical .mivf output. That test needs a real media file
     and a real encode run (minutes, not a unit test) -- it is specified in
     ENCODER_GUI_VALIDATION_SPEC.md as a Phase C hardware/build gate, not
     something this fast unit-test file attempts.

No PySide6 import anywhere in this file -- build_argv() is pure Python,
runnable with the plain interpreter, matching backend.py's own design goal
of being testable without a GUI or a display.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))

from mivf_gui.backend import (EncodeProgressTracker, build_argv, format_command_preview)  # noqa: E402
from mivf_gui.project import MivfProject, ProjectArtwork, ProjectTheme  # noqa: E402


def make_project(preset: str, source: str, output: str, overrides=None) -> MivfProject:
    return MivfProject(
        source_media=source,
        output_path=output,
        preset=preset,
        advanced_overrides=overrides or {},
        artwork=ProjectArtwork(),
        theme=ProjectTheme(),
    )


def test_balanced_preset_emits_no_extra_flags():
    project = make_project("balanced", "movie.mkv", "movie.mivf")
    argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")
    assert argv == ["python", "encode_mivf.py", "movie.mkv", "movie.mivf"]


def test_high_quality_preset_matches_hand_written_cli_command():
    """Golden-file test: the exact argv a user would type by hand for the
    High Quality preset, per ENCODER_GUI_INFORMATION_ARCHITECTURE.md's
    preset table ({"m2y2": True, "keep": 16})."""
    project = make_project("high_quality", "movie.mkv", "movie.mivf")
    argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")

    expected = [
        "python", "encode_mivf.py", "movie.mkv", "movie.mivf",
        "--keep", "16",
        "--m2y2",
    ]
    assert argv == expected, f"GUI argv diverged from the hand-written direct CLI command:\n{argv}\n!=\n{expected}"


def test_lower_performance_hardware_preset_uses_real_3ds_fast_flag():
    project = make_project("lower_performance_hardware", "movie.mkv", "movie.mivf")
    argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")
    assert "--profile" in argv
    assert argv[argv.index("--profile") + 1] == "3ds-fast"


def test_advanced_override_wins_over_preset():
    project = make_project("balanced", "movie.mkv", "movie.mivf", overrides={"qp": 40})
    argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")
    assert "--qp" in argv
    assert argv[argv.index("--qp") + 1] == "40"


def test_no_experimental_flag_in_any_basic_preset():
    """Binding rule: Basic mode must never enable an EXPERIMENTAL flag."""
    for preset in ("fast_test", "balanced", "high_quality", "lower_performance_hardware"):
        project = make_project(preset, "movie.mkv", "movie.mivf")
        argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")
        joined = " ".join(argv)
        assert "diamond" not in joined and "hierarchical" not in joined and "hybrid" not in joined
        assert "--warm-start-chunks" not in argv


def test_command_preview_is_the_literal_argv_not_a_reconstruction():
    project = make_project("balanced", "a movie.mkv", "out.mivf")
    argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")
    preview = format_command_preview(argv)
    assert '"a movie.mkv"' in preview  # quoted because it contains a space
    assert "out.mivf" in preview


class _Clock:
    def __init__(self):
        self.now = 0.0
    def __call__(self):
        return self.now


def test_progress_tracker_parses_live_video_line_and_eta():
    clock = _Clock()
    tracker = EncodeProgressTracker(total_frames=2400, clock=clock)
    stage = tracker.update("1. Streaming and Compressing Video")
    assert stage.stage == "video" and stage.percent == 0
    progress = tracker.update("encoded segment 2: start=480 frames=240 total=720 speed=36.0 fps")
    assert progress.stage == "video"
    assert progress.current_frames == 720
    assert progress.percent == 30
    assert progress.speed_fps == 36.0
    assert round(progress.eta_seconds) == 47


def test_progress_tracker_is_indeterminate_without_trustworthy_total():
    tracker = EncodeProgressTracker(total_frames=None)
    progress = tracker.update("encoded segment 0: start=0 frames=240 total=240 speed=25.0 fps")
    assert progress.percent is None
    assert progress.eta_seconds is None


def test_progress_tracker_recognizes_non_frame_counted_stages_without_fake_eta():
    tracker = EncodeProgressTracker(total_frames=1000)
    m2y2 = tracker.update("3. Range-coding video to M2Y2 (lossless, smaller file)")
    seek = tracker.update("4. Generating Seek Index Metadata")
    finalizing = tracker.update("STAGE TIMING")
    assert m2y2.stage == "m2y2" and m2y2.eta_seconds is None
    assert seek.stage == "seek_index" and seek.percent is None
    assert finalizing.stage == "finalizing"


def test_progress_tracker_smooths_audio_speed_and_keeps_eta_conservative():
    clock = _Clock()
    tracker = EncodeProgressTracker(total_frames=1200, clock=clock)
    tracker.update("2. Multiplexing Audio (IA4M)")
    clock.now = 10.0
    first = tracker.update("muxed 300 frames")
    clock.now = 20.0
    second = tracker.update("muxed 600 frames")
    assert round(first.speed_fps, 1) == 30.0
    assert round(second.speed_fps, 1) == 30.0
    assert round(second.eta_seconds) == 20


def test_unknown_encoder_lines_do_not_fabricate_progress():
    tracker = EncodeProgressTracker(total_frames=1000)
    assert tracker.update("an unrelated diagnostic line") is None


def test_explicit_stream_selection_emits_authoritative_cli_flags():
    project = make_project("balanced", "movie.mkv", "movie.mivf")
    project.video_stream_index = 2
    project.audio_stream_index = 1
    argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")
    assert argv[-4:] == ["--video-stream", "2", "--audio-stream", "1"]

def test_legacy_project_without_selection_emits_no_stream_flags():
    project = make_project("balanced", "movie.mkv", "movie.mivf")
    argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")
    assert "--video-stream" not in argv
    assert "--audio-stream" not in argv

def test_stream_selection_project_round_trip(tmp_path):
    project = make_project("balanced", "movie.mkv", "movie.mivf")
    project.video_stream_index = 1
    project.audio_stream_index = 3
    project.stream_source_identity = {"canonical_path": "X:/movie.mkv", "size_bytes": 123, "mtime_ns": 456}
    path = tmp_path / "streams.mivfproj"
    project.save(path)
    loaded = MivfProject.load(path)
    assert loaded.video_stream_index == 1
    assert loaded.audio_stream_index == 3
    assert loaded.stream_source_identity == project.stream_source_identity


def run_all():
    tests = [v for k, v in globals().items() if k.startswith("test_") and callable(v)]
    failed = 0
    for t in tests:
        try:
            t()
            print(f"PASS: {t.__name__}")
        except AssertionError as e:
            print(f"FAIL: {t.__name__}: {e}")
            failed += 1
    print(f"\n{'SOME TESTS FAILED' if failed else 'ALL TESTS PASSED'} ({failed} failure{'s' if failed != 1 else ''})")
    return failed


if __name__ == "__main__":
    sys.exit(1 if run_all() else 0)
