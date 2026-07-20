"""Local Theme Browser + theme-package import -- pure-logic layer.

ThemeBrowserDialog needs a real QWidget and is exercised in
smoke_offscreen.py instead (same split as the other dialogs this batch).
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "mivf-gui" / "src"))

from mivf_gui.theme_browser import (  # noqa: E402
    parse_manifest_colors, scan_theme_packages, apply_theme_package, ThemePackageInfo,
)
from mivf_gui.theme_plan import _manifest_text  # noqa: E402
from mivf_gui.project import MivfProject, ProjectTheme  # noqa: E402


# --- parse_manifest_colors against hand-built text -------------------------

def test_parse_manifest_colors_reads_accent_and_outline():
    text = "MIVFTHEME_SCHEMA=1\nPALETTE_ACCENT=70,120,210\nPALETTE_OUTLINE=255,255,255\n"
    result = parse_manifest_colors(text)
    assert result["accent_rgb"] == (70, 120, 210)
    assert result["outline_rgb"] == (255, 255, 255)


def test_parse_manifest_colors_attributes_position_to_the_right_control():
    text = (
        "CONTROL=REWIND\nCONTROL.STATE=IDLE\nCONTROL.POSITION=-10,5\nCONTROL.END\n"
        "CONTROL=PLAY_PAUSE\nCONTROL.STATE=IDLE\nCONTROL.POSITION=3,-2\nCONTROL.END\n"
    )
    result = parse_manifest_colors(text)
    assert result["dashboard_layout"] == {"REWIND": [-10, 5], "PLAY_PAUSE": [3, -2]}


def test_parse_manifest_colors_ignores_position_outside_a_control_block():
    text = "CONTROL.POSITION=1,2\n"
    result = parse_manifest_colors(text)
    assert result["dashboard_layout"] == {}


def test_parse_manifest_colors_missing_keys_are_none_not_a_crash():
    result = parse_manifest_colors("MIVFTHEME_SCHEMA=1\n")
    assert result["accent_rgb"] is None
    assert result["outline_rgb"] is None
    assert result["dashboard_layout"] == {}


def test_parse_manifest_colors_handles_crlf_line_endings():
    text = "PALETTE_ACCENT=1,2,3\r\nPALETTE_OUTLINE=4,5,6\r\n"
    result = parse_manifest_colors(text)
    assert result["accent_rgb"] == (1, 2, 3)
    assert result["outline_rgb"] == (4, 5, 6)


def test_parse_manifest_colors_malformed_rgb_is_none_not_a_crash():
    result = parse_manifest_colors("PALETTE_ACCENT=not,a,color\n")
    assert result["accent_rgb"] is None


# --- real round trip against the actual manifest writer --------------------

def test_round_trips_against_the_real_manifest_writer():
    """Not a hand-built fixture -- calls theme_plan._manifest_text() itself
    (the same function export_theme_package() uses) so this test fails
    immediately if the real writer's grammar ever drifts from what this
    parser expects, rather than silently passing against a stale fixture."""
    project = {
        "theme": {"accent_rgb": [12, 34, 56], "outline_rgb": [200, 210, 220]},
        "dashboard_layout": {"REWIND": [-8, 4], "FAST_FORWARD": [6, -3]},
    }
    text = _manifest_text(project, names={})
    result = parse_manifest_colors(text)
    assert result["accent_rgb"] == (12, 34, 56)
    assert result["outline_rgb"] == (200, 210, 220)
    assert result["dashboard_layout"]["REWIND"] == [-8, 4]
    assert result["dashboard_layout"]["FAST_FORWARD"] == [6, -3]
    # BACK never gets a position key (see theme_plan._manifest_text's own
    # comment: the DVD-menu Back control isn't part of the C.6 canvas).
    assert "BACK" not in result["dashboard_layout"]


def test_round_trips_default_colors_when_project_has_none():
    text = _manifest_text({}, names={})
    result = parse_manifest_colors(text)
    assert result["accent_rgb"] == (70, 120, 210)  # _manifest_text's own documented default
    assert result["outline_rgb"] == (255, 255, 255)


# --- scan_theme_packages (real filesystem) ----------------------------------

def test_scan_finds_manifests_and_counts_sibling_assets(tmp_path):
    (tmp_path / "mytheme.mivftheme").write_text("PALETTE_ACCENT=1,2,3\nPALETTE_OUTLINE=4,5,6\n")
    (tmp_path / "mytheme.rewind.idle.mivfasset").write_bytes(b"x" * 10)
    (tmp_path / "mytheme.rewind.focused.mivfasset").write_bytes(b"y" * 20)
    (tmp_path / "unrelated.txt").write_text("not a theme file")

    found = scan_theme_packages(tmp_path)
    assert len(found) == 1
    info = found[0]
    assert info.basename == "mytheme"
    assert info.accent_rgb == (1, 2, 3)
    assert info.asset_count == 2
    assert info.total_bytes > 30  # 2 assets + the manifest itself


def test_scan_nonexistent_folder_returns_empty_list_not_a_crash(tmp_path):
    assert scan_theme_packages(tmp_path / "does_not_exist") == []


def test_scan_empty_folder_returns_empty_list(tmp_path):
    assert scan_theme_packages(tmp_path) == []


def test_scan_multiple_packages_in_the_same_folder(tmp_path):
    (tmp_path / "a.mivftheme").write_text("PALETTE_ACCENT=1,1,1\n")
    (tmp_path / "b.mivftheme").write_text("PALETTE_ACCENT=2,2,2\n")
    found = scan_theme_packages(tmp_path)
    assert sorted(info.basename for info in found) == ["a", "b"]


# --- apply_theme_package -----------------------------------------------------

def test_apply_theme_package_sets_accent_and_outline_preserves_back_fill():
    project = MivfProject(source_media="m.mkv", output_path="m.mivf")
    project.theme = ProjectTheme(back_fill_rgb=(9, 9, 9))
    info = ThemePackageInfo(
        manifest_path=Path("x.mivftheme"), basename="x",
        accent_rgb=(1, 2, 3), outline_rgb=(4, 5, 6),
        dashboard_layout={"REWIND": [1, 1]}, asset_count=0, total_bytes=0,
    )
    apply_theme_package(info, project)
    assert project.theme.accent_rgb == (1, 2, 3)
    assert project.theme.outline_rgb == (4, 5, 6)
    assert project.theme.back_fill_rgb == (9, 9, 9)  # never exported/imported -- must survive untouched
    assert project.dashboard_layout == {"REWIND": [1, 1]}


def test_apply_theme_package_with_no_colors_leaves_existing_theme_alone():
    project = MivfProject(source_media="m.mkv", output_path="m.mivf")
    project.theme = ProjectTheme(accent_rgb=(9, 9, 9))
    info = ThemePackageInfo(
        manifest_path=Path("x.mivftheme"), basename="x",
        accent_rgb=None, outline_rgb=None, dashboard_layout={}, asset_count=0, total_bytes=0,
    )
    apply_theme_package(info, project)
    assert project.theme.accent_rgb == (9, 9, 9)
    assert project.dashboard_layout == {}
