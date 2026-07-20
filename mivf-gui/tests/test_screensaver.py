"""Advanced Screensaver Customization -- Toolkit-side pure logic.

Player-side settings (speed/idle-delay/reduce-motion/fade) are GLOBAL,
in-player Settings-menu concerns (source/mivf_settings.c, no Toolkit/
project role -- see PHASE_STATE.md's explicit scope note); only the
per-title custom bounce image is Toolkit/project-persisted, matching the
existing `cover` artwork's own scope exactly. This file covers what's
actually Toolkit-side: the project field, exact-dimension conversion via
the existing process_asset() primitive, and missing/corrupt fallback.
"""
import sys
from pathlib import Path

import pytest
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "mivf-gui" / "src"))

from mivf_gui.project import MivfProject, ProjectArtwork  # noqa: E402
from mivf_gui.asset_pipeline import process_asset, AssetState, clear_cache  # noqa: E402

SCREENSAVER_W, SCREENSAVER_H = 96, 54
EXPECTED_BYTES = SCREENSAVER_W * SCREENSAVER_H * 2  # headerless RGB565, no envelope


def make_project():
    return MivfProject(source_media="movie.mkv", output_path="movie.mivf")


# --- legacy / project persistence -----------------------------------------

def test_legacy_project_dict_without_screensaver_field_loads_safely():
    raw = {"schema": "mivf-toolkit-project-v1", "tool_version": "0.1.0",
           "source_media": "m.mkv", "output_path": "m.mivf", "preset": "balanced"}
    project = MivfProject.from_dict(raw)
    assert project.artwork.screensaver is None


def test_screensaver_path_project_round_trip(tmp_path):
    project = make_project()
    project.artwork.screensaver = "logo.png"
    path = tmp_path / "saver.mivfproj"
    project.save(path)
    loaded = MivfProject.load(path)
    assert loaded.artwork.screensaver == "logo.png"


# --- exact dimensions / byte size / deterministic conversion --------------

def _make_source_png(path: Path, size=(200, 120), color=(30, 60, 200)):
    Image.new("RGB", size, color).save(path)


def test_conversion_produces_exact_runtime_byte_size(tmp_path):
    clear_cache()
    src = tmp_path / "logo.png"
    _make_source_png(src)
    result = process_asset(str(src), SCREENSAVER_W, SCREENSAVER_H, has_mask=False,
                            header_bytes=0, format_label=".screensaver.cover")
    assert result.state == AssetState.READY
    assert result.runtime_bytes == EXPECTED_BYTES == 10368


def test_conversion_is_headerless_no_envelope_bytes(tmp_path):
    """header_bytes=0 is the real, deliberate contract for this format --
    see asset_pipeline.py's own comment on the 12-byte-overcount bug this
    was written to avoid. Any non-zero header would break the player's
    exact-size fread() in mivf_menu_load_screensaver_image."""
    clear_cache()
    src = tmp_path / "logo.png"
    _make_source_png(src)
    result = process_asset(str(src), SCREENSAVER_W, SCREENSAVER_H, has_mask=False,
                            header_bytes=0, format_label=".screensaver.cover")
    assert result.runtime_bytes == SCREENSAVER_W * SCREENSAVER_H * 2  # no +12 header


def test_conversion_deterministic_across_repeated_calls(tmp_path):
    clear_cache()
    src = tmp_path / "logo.png"
    _make_source_png(src)
    a = process_asset(str(src), SCREENSAVER_W, SCREENSAVER_H, has_mask=False,
                       header_bytes=0, format_label=".screensaver.cover")
    b = process_asset(str(src), SCREENSAVER_W, SCREENSAVER_H, has_mask=False,
                       header_bytes=0, format_label=".screensaver.cover")
    assert a.runtime_bytes == b.runtime_bytes
    assert list(a.runtime_image.getdata()) == list(b.runtime_image.getdata())


# --- missing / corrupt asset fallback --------------------------------------

def test_missing_screensaver_source_reports_missing_not_crash():
    clear_cache()
    result = process_asset("does_not_exist_anywhere.png", SCREENSAVER_W, SCREENSAVER_H,
                            has_mask=False, header_bytes=0, format_label=".screensaver.cover")
    assert result.state == AssetState.MISSING


def test_corrupt_screensaver_source_reports_error_not_crash(tmp_path):
    clear_cache()
    bad = tmp_path / "not_an_image.png"
    bad.write_bytes(b"this is not a real png file")
    result = process_asset(str(bad), SCREENSAVER_W, SCREENSAVER_H, has_mask=False,
                            header_bytes=0, format_label=".screensaver.cover")
    assert result.state == AssetState.ERROR


def test_no_source_selected_is_a_clean_empty_state():
    clear_cache()
    result = process_asset(None, SCREENSAVER_W, SCREENSAVER_H, has_mask=False,
                            header_bytes=0, format_label=".screensaver.cover")
    assert result.state == AssetState.EMPTY


# --- ProjectArtwork field existence (data-model contract) -----------------

def test_project_artwork_screensaver_field_exists_and_defaults_none():
    art = ProjectArtwork()
    assert art.screensaver is None
