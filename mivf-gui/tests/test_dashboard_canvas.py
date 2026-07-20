"""C.6: Advanced Interactive Dashboard Canvas -- pure-logic layer.

GUI drag/undo/snap/Cancel-discipline checks live in smoke_offscreen.py
(needs a real QWidget); this file covers the project model, manifest
export, and validation rules, which don't need a display.
"""
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "mivf-gui" / "src"))

from mivf_gui.project import MivfProject  # noqa: E402
from mivf_gui.theme_plan import (  # noqa: E402
    validate_dashboard_layout, _manifest_text, PREMIERE_CONTROL_GEOMETRY,
    DASHBOARD_CANVAS_W, DASHBOARD_CANVAS_H,
)


def make_project():
    return MivfProject(source_media="movie.mkv", output_path="movie.mivf")


# --- legacy project loading / round trip ---------------------------------

def test_legacy_project_dict_without_dashboard_layout_loads_safely():
    raw = {"schema": "mivf-toolkit-project-v1", "tool_version": "0.1.0",
           "source_media": "m.mkv", "output_path": "m.mivf", "preset": "balanced"}
    project = MivfProject.from_dict(raw)
    assert project.dashboard_layout == {}


def test_dashboard_layout_project_round_trip(tmp_path):
    project = make_project()
    project.dashboard_layout = {"REWIND": [10, -5], "FAST_FORWARD": [-8, 3]}
    path = tmp_path / "layout.mivfproj"
    project.save(path)
    loaded = MivfProject.load(path)
    assert loaded.dashboard_layout == {"REWIND": [10, -5], "FAST_FORWARD": [-8, 3]}


# --- validation: bounds, offscreen, overlap, unsupported control ---------

def test_validate_empty_layout_has_no_messages():
    assert validate_dashboard_layout({}) == []


def test_validate_rejects_unsupported_control():
    msgs = validate_dashboard_layout({"BACK": (0, 0)})
    assert any(m.category == "layout_unsupported_control" for m in msgs)


def test_validate_rejects_out_of_range_offset():
    msgs = validate_dashboard_layout({"REWIND": (999, 0)})
    assert any(m.category == "layout_offset_out_of_range" for m in msgs)


def test_validate_flags_offscreen_position():
    # Rewind base is (66, 128); pushing it hard left drives it off-canvas.
    msgs = validate_dashboard_layout({"REWIND": (-160, 0)})
    assert any(m.category == "layout_offscreen" for m in msgs)


def test_validate_accepts_small_safe_offset():
    msgs = validate_dashboard_layout({"REWIND": (10, 5)})
    assert msgs == []


def test_validate_warns_on_overlap_not_error():
    # Drag Rewind on top of Play/Pause (base (160,128)) -- real overlap.
    rw_base = PREMIERE_CONTROL_GEOMETRY["REWIND"]
    pp_base = PREMIERE_CONTROL_GEOMETRY["PLAY_PAUSE"]
    dx = pp_base[0] - rw_base[0]
    dy = pp_base[1] - rw_base[1]
    msgs = validate_dashboard_layout({"REWIND": (dx, dy)})
    overlap = [m for m in msgs if m.category == "layout_overlap"]
    assert overlap and overlap[0].severity == "WARNING"


def test_validate_no_false_positive_overlap_for_default_layout():
    """The three real base positions (66,160,254 @ y=128) must not warn
    against each other with zero offset -- confirms the geometry constants
    match the real player spacing, not just internal self-consistency."""
    msgs = validate_dashboard_layout({c: (0, 0) for c in PREMIERE_CONTROL_GEOMETRY})
    assert not any(m.category == "layout_overlap" for m in msgs)


# --- deterministic manifest export, visual/hitbox coherence at the data level ---

def test_manifest_omits_position_key_when_no_layout():
    text = _manifest_text({}, {})
    assert "CONTROL.POSITION" not in text


def test_manifest_emits_position_once_per_control_not_per_state():
    text = _manifest_text({"dashboard_layout": {"REWIND": [12, -4]}}, {})
    assert text.count("CONTROL.POSITION=12,-4") == 1


def test_manifest_never_emits_position_for_back_control():
    text = _manifest_text({"dashboard_layout": {"REWIND": [1, 1]}}, {})
    # BACK's own CONTROL=BACK...CONTROL.END block must contain no position key.
    back_block = text.split("CONTROL=BACK", 1)[1].split("CONTROL.END", 1)[0]
    assert "CONTROL.POSITION" not in back_block


def test_manifest_deterministic_across_repeated_calls():
    project = {"dashboard_layout": {"REWIND": [5, 5], "PLAY_PAUSE": [-3, 2], "FAST_FORWARD": [0, -6]}}
    a = _manifest_text(project, {})
    b = _manifest_text(project, {})
    assert a == b


def test_manifest_position_survives_for_each_moved_control():
    project = {"dashboard_layout": {"REWIND": [5, 5], "PLAY_PAUSE": [-3, 2], "FAST_FORWARD": [0, -6]}}
    text = _manifest_text(project, {})
    assert "CONTROL.POSITION=5,5" in text
    assert "CONTROL.POSITION=-3,2" in text
    assert "CONTROL.POSITION=0,-6" in text


# --- geometry sanity (visual/hitbox coherence relies on these constants) --

def test_premiere_geometry_covers_exactly_the_three_functional_controls():
    assert set(PREMIERE_CONTROL_GEOMETRY.keys()) == {"REWIND", "PLAY_PAUSE", "FAST_FORWARD"}


def test_canvas_dimensions_match_real_dashboard_bg_contract():
    """mivf_customization_get_dashboard_bg()'s own doc comment: dimensions
    are always exactly 320x240 -- the layout validator must agree."""
    assert (DASHBOARD_CANVAS_W, DASHBOARD_CANVAS_H) == (320, 240)
