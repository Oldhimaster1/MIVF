"""H: Chapter Authoring Studio -- pure-logic layer.

ChapterAuthoringDialog needs a real QWidget and lives in
smoke_offscreen.py instead (same split already used for
dashboard_canvas.py, project_home.py, encode_queue.py).
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "mivf-gui" / "src"))

from mivf_gui.chapter_authoring import (  # noqa: E402
    ChapterMark, format_timestamp, parse_timestamp, import_from_probe,
    validate_chapters, chapters_sidecar_text, write_chapters_sidecar,
    CHAPTER_MAX, LABEL_MAX_CHARS,
)
from mivf_gui.project import MivfProject  # noqa: E402


# --- timestamp formatting / parsing (must match hfix60_chapters_load) -----

def test_format_timestamp_under_an_hour():
    assert format_timestamp(75) == "0:01:15"


def test_format_timestamp_over_an_hour():
    assert format_timestamp(3725) == "1:02:05"


def test_parse_timestamp_hms():
    assert parse_timestamp("1:02:05") == 3725.0


def test_parse_timestamp_ms():
    assert parse_timestamp("01:15") == 75.0


def test_parse_timestamp_bare_seconds():
    assert parse_timestamp("90") == 90.0


def test_parse_timestamp_invalid_returns_none():
    assert parse_timestamp("not a time") is None


def test_parse_timestamp_empty_returns_none():
    assert parse_timestamp("") is None
    assert parse_timestamp("   ") is None


def test_format_then_parse_round_trips_to_the_same_whole_second():
    for seconds in (0, 5, 59, 60, 3599, 3600, 7325):
        assert parse_timestamp(format_timestamp(seconds)) == float(round(seconds))


# --- import_from_probe -----------------------------------------------------

class _FakeChapterInfo:
    def __init__(self, index, start_seconds, title):
        self.index = index
        self.start_seconds = start_seconds
        self.title = title


def test_import_from_probe_uses_real_titles():
    probe_chapters = (_FakeChapterInfo(0, 0.0, "Intro"), _FakeChapterInfo(1, 120.5, "Act One"))
    imported = import_from_probe(probe_chapters)
    assert [c.label for c in imported] == ["Intro", "Act One"]
    assert [c.seconds for c in imported] == [0.0, 120.5]


def test_import_from_probe_falls_back_to_a_generated_title_when_missing():
    probe_chapters = (_FakeChapterInfo(0, 0.0, None), _FakeChapterInfo(1, 30.0, "  "))
    imported = import_from_probe(probe_chapters)
    assert imported[0].label == "Chapter 1"
    assert imported[1].label == "Chapter 2"


def test_import_from_probe_empty_input_is_an_empty_list():
    assert import_from_probe(()) == []


# --- validate_chapters -------------------------------------------------------

def test_validate_chapters_flags_negative_time():
    warnings = validate_chapters([ChapterMark(-5.0, "Bad")])
    assert any("negative" in w for w in warnings)


def test_validate_chapters_flags_empty_label():
    warnings = validate_chapters([ChapterMark(10.0, "")])
    assert any("empty label" in w for w in warnings)


def test_validate_chapters_flags_duplicate_timestamps():
    warnings = validate_chapters([ChapterMark(10.0, "A"), ChapterMark(10.0, "B")])
    assert any("shares its timestamp" in w for w in warnings)


def test_validate_chapters_flags_exceeding_the_player_cap():
    marks = [ChapterMark(float(i), f"C{i}") for i in range(CHAPTER_MAX + 1)]
    warnings = validate_chapters(marks)
    assert any(str(CHAPTER_MAX) in w for w in warnings)


def test_validate_chapters_clean_list_has_no_warnings():
    assert validate_chapters([ChapterMark(0.0, "Intro"), ChapterMark(60.0, "Act One")]) == []


# --- chapters_sidecar_text (the exact format hfix60_chapters_load parses) --

def test_sidecar_text_is_pipe_delimited_and_time_sorted():
    marks = [ChapterMark(60.0, "Second"), ChapterMark(0.0, "First")]
    text = chapters_sidecar_text(marks)
    lines = text.splitlines()
    assert lines[0] == "0.000|First"
    assert lines[1] == "60.000|Second"


def test_sidecar_text_strips_the_delimiter_character_from_labels():
    text = chapters_sidecar_text([ChapterMark(0.0, "Chapter | One")])
    assert "|" not in text.split("|", 1)[1]  # only the field-separator pipe remains
    assert text.startswith("0.000|Chapter / One")


def test_sidecar_text_strips_newlines_from_labels():
    text = chapters_sidecar_text([ChapterMark(0.0, "Line1\nLine2")])
    assert len(text.splitlines()) == 1


def test_sidecar_text_truncates_overlong_labels_to_the_player_buffer_size():
    long_label = "x" * 100
    text = chapters_sidecar_text([ChapterMark(0.0, long_label)])
    written_label = text.split("|", 1)[1].strip()
    assert len(written_label) == LABEL_MAX_CHARS


def test_sidecar_text_caps_at_the_player_chapter_limit():
    marks = [ChapterMark(float(i), f"C{i}") for i in range(CHAPTER_MAX + 10)]
    text = chapters_sidecar_text(marks)
    assert len(text.splitlines()) == CHAPTER_MAX


def test_sidecar_text_empty_list_is_an_empty_string():
    assert chapters_sidecar_text([]) == ""


# --- write_chapters_sidecar (path convention + real file I/O) -------------

def test_write_chapters_sidecar_replaces_the_mivf_extension(tmp_path):
    out = tmp_path / "movie.mivf"
    sidecar = write_chapters_sidecar(out, [ChapterMark(0.0, "Intro")])
    assert sidecar == tmp_path / "movie.chapters"
    assert sidecar.exists()
    assert sidecar.read_text(encoding="utf-8") == "0.000|Intro\n"


# --- project.chapters field (legacy load / round trip) ----------------------

def test_legacy_project_dict_without_chapters_field_loads_safely():
    raw = {"schema": "mivf-toolkit-project-v1", "tool_version": "0.1.0",
           "source_media": "m.mkv", "output_path": "m.mivf", "preset": "balanced"}
    project = MivfProject.from_dict(raw)
    assert project.chapters == []


def test_chapters_project_round_trip(tmp_path):
    project = MivfProject(source_media="m.mkv", output_path="m.mivf")
    project.chapters = [ChapterMark(0.0, "Intro").to_dict(), ChapterMark(60.0, "Act One").to_dict()]
    path = tmp_path / "chapters.mivfproj"
    project.save(path)
    loaded = MivfProject.load(path)
    assert loaded.chapters == project.chapters
