"""E.1.1: MediaProbe Foundation tests. All parsing tests use synthetic
ffprobe JSON fixtures -- no real ffmpeg/ffprobe binary is needed or used,
per the definition of done ("no need to encode media during MediaProbe
tests"). probe_media()'s subprocess call is exercised via monkeypatching
subprocess.run, so its full contract (argv shape, timeout, error handling)
is still covered without a real tool."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))

from mivf_gui import media_probe as mp  # noqa: E402


def _realistic_ffprobe_json() -> dict:
    return {
        "format": {
            "filename": "movie.mkv", "format_name": "matroska,webm",
            "duration": "5400.500000", "bit_rate": "4500000",
            "tags": {"title": "Les Misérables", "genre": "Drama", "date": "2012-12-25"},
        },
        "streams": [
            {
                "index": 0, "codec_type": "video", "codec_name": "h264", "profile": "High",
                "pix_fmt": "yuv420p", "width": 1920, "height": 1080,
                "display_aspect_ratio": "16:9", "sample_aspect_ratio": "1:1",
                "avg_frame_rate": "24000/1001", "r_frame_rate": "24000/1001",
                "color_space": "bt709", "color_primaries": "bt709", "color_transfer": "bt709",
                "bit_rate": "4000000", "nb_frames": "129600",
                "disposition": {"default": 1, "attached_pic": 0},
                "tags": {"language": "eng"},
            },
            {
                "index": 1, "codec_type": "audio", "codec_name": "aac",
                "sample_rate": "48000", "channels": 6, "channel_layout": "5.1",
                "bit_rate": "384000", "disposition": {"default": 1},
                "tags": {"language": "eng", "title": "Surround"},
            },
            {
                "index": 2, "codec_type": "audio", "codec_name": "ac3",
                "sample_rate": "48000", "channels": 2, "channel_layout": "stereo",
                "bit_rate": "192000", "disposition": {"default": 0},
                "tags": {"language": "fra"},
            },
            {
                "index": 3, "codec_type": "subtitle", "codec_name": "subrip",
                "disposition": {"default": 1, "forced": 0, "hearing_impaired": 0},
                "tags": {"language": "eng", "title": "English"},
            },
            {
                "index": 4, "codec_type": "subtitle", "codec_name": "hdmv_pgs_subtitle",
                "disposition": {"default": 0, "forced": 0, "hearing_impaired": 1},
                "tags": {"language": "eng", "title": "English (SDH)"},
            },
        ],
        "chapters": [
            {"time_base": "1/1000", "start": 0, "start_time": "0.000000",
             "end": 600000, "end_time": "600.000000", "tags": {"title": "Chapter 1"}},
            {"time_base": "1/1000", "start": 600000, "start_time": "600.000000",
             "end": 1200000, "end_time": "1200.000000", "tags": {"title": "Chapter 2"}},
        ],
    }


def _identity(tmp_path: Path) -> mp.SourceIdentity:
    p = tmp_path / "fake.mkv"
    p.write_bytes(b"not real media, just needs to exist")
    return mp.source_identity(p)


# --- parse_ffprobe_json: the deterministic-fixture core --------------------

def test_realistic_fixture_parses_every_field(tmp_path):
    result = mp.parse_ffprobe_json(_realistic_ffprobe_json(), _identity(tmp_path))
    assert result.ok
    assert result.container_format == "matroska,webm"
    assert result.duration_seconds == 5400.5
    assert result.bitrate_bps == 4500000
    assert result.container_tags == {"title": "Les Misérables", "genre": "Drama", "date": "2012-12-25"}

    assert len(result.video_streams) == 1
    v = result.video_streams[0]
    assert v.absolute_index == 0 and v.stream_index == 0
    assert v.codec == "h264" and v.width == 1920 and v.height == 1080
    assert v.avg_frame_rate_num == 24000 and v.avg_frame_rate_den == 1001
    assert v.is_default is True and v.is_attached_pic is False
    assert v.frame_count == 129600 and v.frame_count_source == "reported"
    assert v.language == "eng"

    assert len(result.audio_streams) == 2
    assert result.audio_streams[0].stream_index == 0 and result.audio_streams[0].channels == 6
    assert result.audio_streams[1].stream_index == 1 and result.audio_streams[1].language == "fra"

    assert len(result.subtitle_streams) == 2
    assert result.subtitle_streams[0].kind == "text"
    assert result.subtitle_streams[1].kind == "bitmap" and result.subtitle_streams[1].is_hearing_impaired is True

    assert len(result.chapters) == 2
    assert result.chapters[0].title == "Chapter 1" and result.chapters[0].end_seconds == 600.0
    assert result.chapters[1].start_seconds == 600.0


def test_unavailable_frame_rate_is_a_warning_not_a_crash(tmp_path):
    raw = _realistic_ffprobe_json()
    raw["streams"][0]["avg_frame_rate"] = "0/0"
    result = mp.parse_ffprobe_json(raw, _identity(tmp_path))
    assert result.ok  # a warning, not an error -- this must not block anything
    assert result.video_streams[0].avg_frame_rate_num is None
    assert any("avg_frame_rate" in w for w in result.warnings)


def test_missing_container_tags_is_an_empty_dict_not_a_crash(tmp_path):
    raw = _realistic_ffprobe_json()
    del raw["format"]["tags"]
    result = mp.parse_ffprobe_json(raw, _identity(tmp_path))
    assert result.container_tags == {}


def test_missing_chapters_key_is_empty_not_a_crash(tmp_path):
    raw = _realistic_ffprobe_json()
    del raw["chapters"]
    result = mp.parse_ffprobe_json(raw, _identity(tmp_path))
    assert result.chapters == ()


def test_overlapping_chapters_produce_a_warning(tmp_path):
    raw = _realistic_ffprobe_json()
    raw["chapters"][1]["start"] = 300000
    raw["chapters"][1]["start_time"] = "300.000000"
    result = mp.parse_ffprobe_json(raw, _identity(tmp_path))
    assert any("overlaps" in w for w in result.warnings)


def test_chapter_end_before_start_produces_a_warning(tmp_path):
    raw = _realistic_ffprobe_json()
    raw["chapters"][0]["end"] = -1
    raw["chapters"][0]["end_time"] = "-1"
    result = mp.parse_ffprobe_json(raw, _identity(tmp_path))
    assert any("precedes" in w for w in result.warnings)


def test_no_streams_at_all_warns_but_does_not_error(tmp_path):
    result = mp.parse_ffprobe_json({"format": {}, "streams": [], "chapters": []}, _identity(tmp_path))
    assert result.ok
    assert any("No video or audio streams" in w for w in result.warnings)


def test_attached_picture_video_stream_is_flagged(tmp_path):
    raw = _realistic_ffprobe_json()
    raw["streams"][0]["disposition"]["attached_pic"] = 1
    result = mp.parse_ffprobe_json(raw, _identity(tmp_path))
    assert result.video_streams[0].is_attached_pic is True


def test_to_json_dict_is_plain_serializable(tmp_path):
    result = mp.parse_ffprobe_json(_realistic_ffprobe_json(), _identity(tmp_path))
    d = result.to_json_dict()
    assert "raw" not in d
    json.dumps(d)  # must not raise


# --- source_identity: matches encode_mivf.py's e0_sha256 exactly ----------

def test_source_identity_hash_matches_manual_sha256(tmp_path):
    p = tmp_path / "x.bin"
    data = b"some real bytes" * 1000
    p.write_bytes(data)
    identity = mp.source_identity(p)
    assert identity.sha256 == hashlib.sha256(data).hexdigest()
    assert identity.size_bytes == len(data)


def test_source_identity_skips_hash_when_not_requested(tmp_path):
    p = tmp_path / "x.bin"
    p.write_bytes(b"data")
    identity = mp.source_identity(p, compute_hash=False)
    assert identity.sha256 is None


def test_source_identity_raises_for_missing_file(tmp_path):
    try:
        mp.source_identity(tmp_path / "does_not_exist.mkv")
        assert False, "must raise for a missing source"
    except mp.MediaProbeError:
        pass


def test_source_identity_handles_unicode_and_spaces(tmp_path):
    p = tmp_path / "Les Misérables — Chapitre 4.mkv"
    p.write_bytes(b"data")
    identity = mp.source_identity(p)
    assert identity.size_bytes == 4
    assert "Misérables" in identity.path


# --- probe_media: subprocess contract, via monkeypatched subprocess.run ---

def _fake_run_factory(stdout_json: dict | str, returncode: int = 0, stderr: str = ""):
    def _fake_run(argv, capture_output, text, timeout, shell):
        assert shell is False, "must never invoke ffprobe through a shell"
        assert "-show_streams" in argv and "-show_chapters" in argv and "-show_format" in argv
        stdout = stdout_json if isinstance(stdout_json, str) else json.dumps(stdout_json)
        return subprocess.CompletedProcess(argv, returncode, stdout=stdout, stderr=stderr)
    return _fake_run


def test_probe_media_full_path_with_stubbed_ffprobe(tmp_path, monkeypatch):
    p = tmp_path / "movie.mkv"
    p.write_bytes(b"fake bytes")
    monkeypatch.setattr(mp.subprocess, "run", _fake_run_factory(_realistic_ffprobe_json()))
    result = mp.probe_media(p, ffprobe="ffprobe")
    assert result.ok
    assert result.identity.size_bytes == len(b"fake bytes")
    assert len(result.video_streams) == 1


def test_probe_media_raises_on_nonzero_exit(tmp_path, monkeypatch):
    p = tmp_path / "movie.mkv"
    p.write_bytes(b"x")
    monkeypatch.setattr(mp.subprocess, "run", _fake_run_factory("", returncode=1, stderr="no such filter"))
    try:
        mp.probe_media(p, ffprobe="ffprobe")
        assert False, "must raise when ffprobe exits nonzero"
    except mp.MediaProbeError as e:
        assert "no such filter" in str(e)


def test_probe_media_raises_on_malformed_json(tmp_path, monkeypatch):
    p = tmp_path / "movie.mkv"
    p.write_bytes(b"x")
    monkeypatch.setattr(mp.subprocess, "run", _fake_run_factory("{not valid json"))
    try:
        mp.probe_media(p, ffprobe="ffprobe")
        assert False, "must raise on invalid JSON"
    except mp.MediaProbeError:
        pass


def test_probe_media_raises_on_timeout(tmp_path, monkeypatch):
    p = tmp_path / "movie.mkv"
    p.write_bytes(b"x")

    def _timeout(*a, **k):
        raise subprocess.TimeoutExpired(cmd="ffprobe", timeout=1.0)
    monkeypatch.setattr(mp.subprocess, "run", _timeout)
    try:
        mp.probe_media(p, timeout=1.0, ffprobe="ffprobe")
        assert False, "must raise on a timeout"
    except mp.MediaProbeError as e:
        assert "timed out" in str(e)


def test_probe_media_raises_for_missing_file(tmp_path):
    try:
        mp.probe_media(tmp_path / "does_not_exist.mkv", ffprobe="ffprobe")
        assert False, "must raise for a missing source file"
    except mp.MediaProbeError:
        pass


def test_probe_media_raises_when_tool_not_found(tmp_path, monkeypatch):
    p = tmp_path / "movie.mkv"
    p.write_bytes(b"x")
    monkeypatch.setattr(mp, "_discover_tool", lambda name: (_ for _ in ()).throw(mp.MediaProbeError("not found")))
    try:
        mp.probe_media(p)  # no explicit ffprobe override -> triggers discovery
        assert False, "must raise when ffprobe cannot be discovered"
    except mp.MediaProbeError:
        pass


# --- caching ---------------------------------------------------------------

def test_cache_avoids_reprobing_an_unchanged_source(tmp_path, monkeypatch):
    p = tmp_path / "movie.mkv"
    p.write_bytes(b"x")
    mp.clear_cache()
    call_count = {"n": 0}

    def _counting_run(argv, capture_output, text, timeout, shell):
        # probe_media() also shells out to `ffprobe -version` for the
        # diagnostic version string -- only count the real probe call
        # (identifiable by -show_streams) so this measures "how many times
        # was the source actually probed," not incidental side calls.
        if "-show_streams" in argv:
            call_count["n"] += 1
        return subprocess.CompletedProcess(argv, 0, stdout=json.dumps(_realistic_ffprobe_json()), stderr="")
    monkeypatch.setattr(mp.subprocess, "run", _counting_run)

    r1 = mp.probe_media_cached(p, ffprobe="ffprobe")
    r2 = mp.probe_media_cached(p, ffprobe="ffprobe")
    assert r1 is r2
    assert call_count["n"] == 1, "an unchanged source must not be re-probed"


def test_cache_invalidates_when_source_identity_changes(tmp_path, monkeypatch):
    p = tmp_path / "movie.mkv"
    p.write_bytes(b"original")
    mp.clear_cache()
    call_count = {"n": 0}

    def _counting_run(argv, capture_output, text, timeout, shell):
        # probe_media() also shells out to `ffprobe -version` for the
        # diagnostic version string -- only count the real probe call
        # (identifiable by -show_streams) so this measures "how many times
        # was the source actually probed," not incidental side calls.
        if "-show_streams" in argv:
            call_count["n"] += 1
        return subprocess.CompletedProcess(argv, 0, stdout=json.dumps(_realistic_ffprobe_json()), stderr="")
    monkeypatch.setattr(mp.subprocess, "run", _counting_run)

    mp.probe_media_cached(p, ffprobe="ffprobe")
    p.write_bytes(b"a completely different, longer set of bytes")  # changes size + mtime
    mp.probe_media_cached(p, ffprobe="ffprobe")
    assert call_count["n"] == 2, "a changed source must be re-probed, not served from cache"


def test_cache_key_includes_compute_hash_option(tmp_path, monkeypatch):
    p = tmp_path / "movie.mkv"
    p.write_bytes(b"x")
    mp.clear_cache()

    def _fake_run(argv, capture_output, text, timeout, shell):
        return subprocess.CompletedProcess(argv, 0, stdout=json.dumps(_realistic_ffprobe_json()), stderr="")
    monkeypatch.setattr(mp.subprocess, "run", _fake_run)

    r1 = mp.probe_media_cached(p, compute_hash=True, ffprobe="ffprobe")
    r2 = mp.probe_media_cached(p, compute_hash=False, ffprobe="ffprobe")
    assert r1.identity.sha256 is not None
    assert r2.identity.sha256 is None
