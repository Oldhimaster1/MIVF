"""E.1.2: Movie Information Authoring tests."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))

from mivf_gui import movie_info as mi  # noqa: E402
from mivf_gui.media_probe import parse_ffprobe_json, source_identity  # noqa: E402


def _probe_with_tags(tmp_path, tags: dict, sha_suffix: str = "a"):
    p = tmp_path / f"movie_{sha_suffix}.mkv"
    p.write_bytes(b"x" * 10)
    identity = source_identity(p)
    return parse_ffprobe_json({"format": {"tags": tags}, "streams": [], "chapters": []}, identity)


# --- import_from_probe: only real tags, never invented --------------------

def test_import_populates_only_fields_with_a_real_tag(tmp_path):
    info = mi.MovieInformation()
    probe = _probe_with_tags(tmp_path, {"title": "Les Misérables", "genre": "Drama"})
    populated = mi.import_from_probe(info, probe)
    assert set(populated) == {"title", "genre"}
    assert info.title == "Les Misérables" and info.genre == "Drama"
    assert info.director is None  # no 'director' tag present -- must not be invented
    assert info.field_provenance["title"] == "imported"


def test_import_extracts_year_from_date_tag(tmp_path):
    info = mi.MovieInformation()
    probe = _probe_with_tags(tmp_path, {"date": "2012-12-25"})
    mi.import_from_probe(info, probe)
    assert info.release_year == "2012"


def test_import_skips_unparseable_year(tmp_path):
    info = mi.MovieInformation()
    probe = _probe_with_tags(tmp_path, {"date": "unknown"})
    populated = mi.import_from_probe(info, probe)
    assert "release_year" not in populated
    assert info.release_year is None


def test_import_never_overwrites_a_manually_edited_field_by_default(tmp_path):
    info = mi.MovieInformation(title="My Own Title")
    mi.mark_manual(info, "title")
    probe = _probe_with_tags(tmp_path, {"title": "Container Title"})
    populated = mi.import_from_probe(info, probe)
    assert "title" not in populated
    assert info.title == "My Own Title"


def test_import_with_overwrite_true_replaces_a_manual_field(tmp_path):
    info = mi.MovieInformation(title="My Own Title")
    mi.mark_manual(info, "title")
    probe = _probe_with_tags(tmp_path, {"title": "Container Title"})
    populated = mi.import_from_probe(info, probe, overwrite=True)
    assert "title" in populated
    assert info.title == "Container Title"


def test_import_falls_back_to_show_tag_when_title_absent(tmp_path):
    info = mi.MovieInformation()
    probe = _probe_with_tags(tmp_path, {"show": "Fallback Show Name"})
    mi.import_from_probe(info, probe)
    assert info.title == "Fallback Show Name"


def test_import_with_no_tags_populates_nothing(tmp_path):
    info = mi.MovieInformation()
    probe = _probe_with_tags(tmp_path, {})
    populated = mi.import_from_probe(info, probe)
    assert populated == []
    assert info.last_import_source_sha256 is None


# --- reset_field_to_probed --------------------------------------------------

def test_reset_restores_the_last_imported_value_after_a_manual_edit(tmp_path):
    info = mi.MovieInformation()
    probe = _probe_with_tags(tmp_path, {"title": "Container Title"})
    mi.import_from_probe(info, probe)
    info.title = "Hand-edited title"
    mi.mark_manual(info, "title")
    restored = mi.reset_field_to_probed(info, "title")
    assert restored is True
    assert info.title == "Container Title"
    assert info.field_provenance["title"] == "imported"


def test_reset_returns_false_for_a_field_never_imported():
    info = mi.MovieInformation(director="Hand-typed, never imported")
    assert mi.reset_field_to_probed(info, "director") is False
    assert info.director == "Hand-typed, never imported"  # unchanged


def test_mark_manual_rejects_unknown_field_name():
    info = mi.MovieInformation()
    try:
        mi.mark_manual(info, "not_a_real_field")
        assert False, "must reject an unknown field name"
    except ValueError:
        pass


# --- synopsis / real player text-fit behavior -------------------------------

def test_collapse_whitespace_matches_player_algorithm_exactly():
    assert mi.collapse_whitespace_ascii("  hello   world\n\tagain  ") == "hello world again "
    assert mi.collapse_whitespace_ascii("") == ""
    assert mi.collapse_whitespace_ascii("noSpacesHere") == "noSpacesHere"


def test_synopsis_preview_splits_at_real_19_char_boundary():
    text = "A" * 19 + "B" * 19
    preview = mi.synopsis_preview(text)
    assert preview["line1"] == "A" * 19
    assert preview["line2"] == "B" * 19
    assert preview["truncated"] is False


def test_synopsis_preview_flags_truncation_beyond_38_chars():
    text = "A" * 50
    preview = mi.synopsis_preview(text)
    assert preview["line1"] == "A" * 19
    assert preview["line2"] == "A" * 19
    assert preview["truncated"] is True
    assert preview["collapsed_length"] == 50


def test_synopsis_preview_handles_none():
    preview = mi.synopsis_preview(None)
    assert preview == {"line1": "", "line2": "", "truncated": False, "collapsed_length": 0, "max_length": 38}


# --- summaries: sanity, not exhaustive formatting tests --------------------

def test_concise_summary_includes_title_year_runtime_genre():
    info = mi.MovieInformation(title="Les Misérables", release_year="2012",
                                displayed_runtime="2h 38min", genre="Drama")
    s = mi.concise_summary(info)
    assert "Les Misérables" in s and "2012" in s and "2h 38min" in s and "Drama" in s


def test_concise_summary_handles_untitled():
    assert "(untitled)" in mi.concise_summary(mi.MovieInformation())


def test_detailed_summary_includes_director_and_synopsis():
    info = mi.MovieInformation(title="X", director="Tom Hooper", synopsis="A story.")
    s = mi.detailed_summary(info)
    assert "Tom Hooper" in s and "A story." in s


# --- round-trip -------------------------------------------------------------

def test_to_dict_from_dict_round_trip():
    info = mi.MovieInformation(title="X", genre="Drama")
    mi.mark_manual(info, "title")
    d = info.to_dict()
    restored = mi.MovieInformation.from_dict(d)
    assert restored == info


def test_from_dict_handles_a_completely_empty_dict():
    restored = mi.MovieInformation.from_dict({})
    assert restored == mi.MovieInformation()
