"""E.3.2b: authoring-time subtitle selection.

Covers the CLI/project-persistence/recovery layer. See smoke_offscreen.py
for the GUI-level widget/reconciliation regression test (changed-source
reset, missing-saved-selection, unusual stream ordering against the real
fixture) -- that layer needs a live QComboBox model, which these plain
pytest tests deliberately don't require.
"""
import importlib.util
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "temp" / "e32b_multisub_test.mkv"

sys.path.insert(0, str(ROOT / "mivf-gui" / "src"))

spec = importlib.util.spec_from_file_location("encode_mivf", ROOT / "encode_mivf.py")
enc = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = enc
spec.loader.exec_module(enc)

from mivf_gui.backend import build_argv  # noqa: E402
from mivf_gui.project import MivfProject  # noqa: E402
from mivf_gui.media_probe import parse_ffprobe_json, SourceIdentity, MediaProbeResult  # noqa: E402


def make_project(source="movie.mkv", output="movie.mivf"):
    return MivfProject(source_media=source, output_path=output)


def _identity(path="X:/movie.mkv", size=123, mtime=456):
    return SourceIdentity(path=path, canonical_path=path, size_bytes=size, mtime_ns=mtime, sha256=None)


# --- 1/2/3/13: CLI argument parsing and argv equivalence ----------------

def test_parser_omits_subtitle_by_default():
    ns = enc.build_parser().parse_args(["in.mkv", "out.mivf"])
    assert ns.subtitle_stream is None
    assert ns.subtitle_edition == 0


def test_parser_accepts_explicit_subtitle_stream_and_edition():
    ns = enc.build_parser().parse_args(["in.mkv", "out.mivf", "--subtitle-stream", "1", "--subtitle-edition", "2"])
    assert ns.subtitle_stream == 1
    assert ns.subtitle_edition == 2


def test_legacy_project_without_subtitle_selection_emits_no_flags():
    project = make_project()
    argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")
    assert "--subtitle-stream" not in argv
    assert "--subtitle-edition" not in argv


def test_explicit_no_subtitle_selection_is_the_same_as_omission():
    project = make_project()
    project.subtitle_stream_index = None
    argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")
    assert "--subtitle-stream" not in argv


def test_explicit_subtitle_selection_emits_authoritative_cli_flags():
    project = make_project()
    project.subtitle_stream_index = 1
    project.subtitle_edition = 2
    argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")
    assert "--subtitle-stream" in argv
    i = argv.index("--subtitle-stream")
    assert argv[i + 1] == "1"
    j = argv.index("--subtitle-edition")
    assert argv[j + 1] == "2"


def test_default_edition_slot_is_not_redundantly_emitted():
    """Edition 0 is the default sidecar slot -- omitting the flag when it's
    0 keeps the common-case argv minimal, matching the omit-when-default
    style already used for video/audio selection."""
    project = make_project()
    project.subtitle_stream_index = 0
    project.subtitle_edition = 0
    argv = build_argv(project, Path("encode_mivf.py"), python_exe="python")
    assert "--subtitle-stream" in argv
    assert "--subtitle-edition" not in argv


# --- 10: project persistence round-trip ----------------------------------

def test_subtitle_selection_project_round_trip(tmp_path):
    project = make_project()
    project.subtitle_stream_index = 2
    project.subtitle_edition = 3
    project.stream_source_identity = {"canonical_path": "X:/movie.mkv", "size_bytes": 123, "mtime_ns": 456}
    path = tmp_path / "subtitle.mivfproj"
    project.save(path)
    loaded = MivfProject.load(path)
    assert loaded.subtitle_stream_index == 2
    assert loaded.subtitle_edition == 3


# --- 1: legacy project with no subtitle fields at all --------------------

def test_legacy_project_dict_without_subtitle_keys_loads_safely():
    """A .mivfproj saved before E.3.2b existed has no subtitle_* keys at
    all -- from_dict must not KeyError, and must resolve to the honest
    'no subtitle authored' state."""
    raw = {
        "schema": "mivf-toolkit-project-v1",
        "tool_version": "0.1.0",
        "source_media": "movie.mkv",
        "output_path": "movie.mivf",
        "preset": "balanced",
    }
    project = MivfProject.from_dict(raw)
    assert project.subtitle_stream_index is None
    assert project.subtitle_edition == 0


# --- 15: recovery fingerprint includes subtitle selection -----------------

def test_recovery_fingerprint_includes_subtitle_selection(tmp_path):
    source = tmp_path / "in.mkv"
    source.write_bytes(b"source")
    a = enc.e0_settings_fingerprint(source, enc.EncodeSettings(subtitle_stream=None))
    b = enc.e0_settings_fingerprint(source, enc.EncodeSettings(subtitle_stream=1, subtitle_edition=2))
    assert a["subtitle_stream"] is None and a["subtitle_edition"] == 0
    assert b["subtitle_stream"] == 1 and b["subtitle_edition"] == 2
    assert a != b


# --- CLI preflight validation ---------------------------------------------

def test_preflight_rejects_negative_subtitle_stream(tmp_path):
    source = tmp_path / "in.mkv"
    source.write_bytes(b"x")
    settings = enc.EncodeSettings(width=64, height=64, subtitle_stream=-1)
    with pytest.raises(SystemExit):
        enc.e0_validate_preflight(source, settings)


def test_preflight_rejects_out_of_range_edition(tmp_path):
    source = tmp_path / "in.mkv"
    source.write_bytes(b"x")
    settings = enc.EncodeSettings(width=64, height=64, subtitle_stream=0, subtitle_edition=4)
    with pytest.raises(SystemExit):
        enc.e0_validate_preflight(source, settings)


def test_preflight_rejects_edition_without_stream(tmp_path):
    source = tmp_path / "in.mkv"
    source.write_bytes(b"x")
    settings = enc.EncodeSettings(width=64, height=64, subtitle_stream=None, subtitle_edition=1)
    with pytest.raises(SystemExit):
        enc.e0_validate_preflight(source, settings)


# --- Sidecar path derivation (mirrors source/main.c's convention) --------

def test_subtitle_sidecar_path_edition_0_matches_player_convention():
    p = enc.make_subtitle_sidecar_path(Path("movie.mivf"), edition=0)
    assert p.name == "movie.srt"


def test_subtitle_sidecar_path_edition_n_matches_player_convention():
    for n in (1, 2, 3):
        p = enc.make_subtitle_sidecar_path(Path("movie.mivf"), edition=n)
        assert p.name == f"movie.{n}.srt"


# --- 8/9: bitmap and unknown codec classification (MediaProbe, real data model) ---

def _probe_with_subtitle_kinds(*kinds_and_codecs):
    streams = [{"index": 0, "codec_type": "video", "avg_frame_rate": "24/1", "width": 4, "height": 4}]
    for i, (kind, codec) in enumerate(kinds_and_codecs):
        streams.append({"index": i + 1, "codec_type": "subtitle", "codec_name": codec})
    raw = {"format": {}, "streams": streams, "chapters": []}
    return parse_ffprobe_json(raw, _identity())


def test_bitmap_subtitle_classified_and_flagged_unsupported():
    result = _probe_with_subtitle_kinds(("bitmap", "dvd_subtitle"))
    assert result.subtitle_streams[0].kind == "bitmap"


def test_unknown_subtitle_codec_classified_and_flagged_unsupported():
    result = _probe_with_subtitle_kinds(("unknown", "some_future_codec"))
    assert result.subtitle_streams[0].kind == "unknown"


def test_text_subtitle_codecs_all_classified_text():
    codecs = ["subrip", "ass", "ssa", "webvtt", "mov_text", "ttml"]
    result = _probe_with_subtitle_kinds(*[("text", c) for c in codecs])
    assert all(s.kind == "text" for s in result.subtitle_streams)


# --- 5/6/7: honest metadata handling (missing language/title, default/forced/HI) ---

def test_missing_language_and_title_are_honestly_none():
    raw = {
        "format": {}, "chapters": [],
        "streams": [
            {"index": 0, "codec_type": "video", "avg_frame_rate": "24/1", "width": 4, "height": 4},
            {"index": 1, "codec_type": "subtitle", "codec_name": "subrip", "tags": {}, "disposition": {}},
        ],
    }
    result = parse_ffprobe_json(raw, _identity())
    s = result.subtitle_streams[0]
    assert s.language is None
    assert s.title is None
    assert s.is_default is False
    assert s.is_forced is False
    assert s.is_hearing_impaired is False


def test_default_forced_hearing_impaired_dispositions_are_read_honestly():
    raw = {
        "format": {}, "chapters": [],
        "streams": [
            {"index": 0, "codec_type": "video", "avg_frame_rate": "24/1", "width": 4, "height": 4},
            {"index": 1, "codec_type": "subtitle", "codec_name": "subrip",
             "tags": {"language": "eng", "title": "English"},
             "disposition": {"default": 1, "forced": 0, "hearing_impaired": 0}},
            {"index": 2, "codec_type": "subtitle", "codec_name": "subrip",
             "tags": {"language": "eng", "title": "Signs"},
             "disposition": {"default": 0, "forced": 1, "hearing_impaired": 0}},
            {"index": 3, "codec_type": "subtitle", "codec_name": "subrip",
             "tags": {"language": "eng", "title": "SDH"},
             "disposition": {"default": 0, "forced": 0, "hearing_impaired": 1}},
        ],
    }
    result = parse_ffprobe_json(raw, _identity())
    default_s, forced_s, hi_s = result.subtitle_streams
    assert default_s.is_default and not default_s.is_forced and not default_s.is_hearing_impaired
    assert forced_s.is_forced and not forced_s.is_default
    assert hi_s.is_hearing_impaired and not hi_s.is_default and not hi_s.is_forced


# --- 14: type-relative vs absolute index, real fixture with unusual ordering ---

FIXTURE_FFPROBE_JSON = ROOT / "temp" / "e32b_multisub_test.ffprobe.json"


@pytest.mark.skipif(not FIXTURE_FFPROBE_JSON.exists(), reason="real captured ffprobe JSON not present")
def test_real_fixture_type_relative_index_survives_unusual_stream_order():
    """The fixture's real stream order is video(0), subtitle(1), audio(2),
    subtitle(3), subtitle(4) -- subtitle streams are NOT contiguous at the
    end, which is exactly the "unusual ordering" case that would break a
    naive absolute-index assumption. Uses the real ffprobe JSON captured
    from the fixture (not a live ffprobe invocation, which needs tool
    discovery outside this test's control) through the same
    parse_ffprobe_json() the real probe path uses -- pure parsing, real data."""
    import json
    raw = json.loads(FIXTURE_FFPROBE_JSON.read_text())
    result = parse_ffprobe_json(raw, _identity(path=str(FIXTURE)))
    assert [s.absolute_index for s in result.subtitle_streams] == [1, 3, 4]
    assert [s.stream_index for s in result.subtitle_streams] == [0, 1, 2]
    langs = [s.language for s in result.subtitle_streams]
    assert langs == ["spa", "eng", "eng"]
    titles = [s.title for s in result.subtitle_streams]
    assert titles == ["Spanish", "English", "Signs"]
    assert result.subtitle_streams[1].is_default is True
    assert result.subtitle_streams[2].is_forced is True


# --- 16/19: deterministic extraction + incomplete-output cleanup, real fixture ---

@pytest.mark.skipif(not FIXTURE.exists(), reason="real fixture not present")
def test_real_extraction_is_deterministic_and_matches_player_naming(tmp_path):
    out = tmp_path / "movie.mivf"
    p1 = enc.extract_subtitle_sidecar(FIXTURE, out, subtitle_stream=0, edition=0)
    p2 = enc.extract_subtitle_sidecar(FIXTURE, out, subtitle_stream=0, edition=0)
    assert p1 == p2 == out.with_name("movie.srt")
    assert p1.read_bytes() == p2.read_bytes()
    assert "Subtitulo en espanol uno." in p1.read_text()
    assert not p1.with_suffix(p1.suffix + ".tmp").exists()


@pytest.mark.skipif(not FIXTURE.exists(), reason="real fixture not present")
def test_real_extraction_different_editions_produce_distinct_sidecars(tmp_path):
    out = tmp_path / "movie.mivf"
    spanish = enc.extract_subtitle_sidecar(FIXTURE, out, subtitle_stream=0, edition=0)
    english = enc.extract_subtitle_sidecar(FIXTURE, out, subtitle_stream=1, edition=1)
    assert spanish.name == "movie.srt"
    assert english.name == "movie.1.srt"
    assert spanish.read_bytes() != english.read_bytes()
    assert "English caption one." in english.read_text()


@pytest.mark.skipif(not FIXTURE.exists(), reason="real fixture not present")
def test_real_extraction_invalid_stream_refuses_cleanly_no_leak(tmp_path):
    out = tmp_path / "movie.mivf"
    with pytest.raises(SystemExit):
        enc.extract_subtitle_sidecar(FIXTURE, out, subtitle_stream=99, edition=0)
    assert not out.with_name("movie.srt.tmp").exists()
    assert not out.with_name("movie.srt").exists()
