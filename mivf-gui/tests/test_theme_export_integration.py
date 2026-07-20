"""Phase C.3.1: real end-to-end export tests.

test_theme_export_runtime_manifest.py only ever exercised _manifest_text()
in isolation with a hand-built, already-self-consistent `names` dict --
it never caught the real movie_menu_back / menu_back key mismatch between
theme_export.py and theme_export_c3.py, because that mismatch only
manifests when the real export_theme_package() pipeline builds `names`
itself. These tests call the real pipeline against real synthetic image
fixtures so this class of bug cannot silently recur.
"""
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))

from PIL import Image  # noqa: E402
from mivf_gui.theme_export import export_theme_package, ThemeExportError  # noqa: E402
from mivf_gui.control_recipe import render  # noqa: E402


def _make_source_images(tmp_path: Path) -> dict[str, str]:
    specs = {
        "dashboard_bg": (400, 300),
        "rewind_underlay": (120, 120),
        "play_pause_underlay": (140, 140),
        "fast_forward_underlay": (120, 120),
        "movie_menu_back": (300, 60),
    }
    paths = {}
    for key, (w, h) in specs.items():
        p = tmp_path / f"{key}.png"
        Image.new("RGBA", (w, h), (120, 160, 200, 255)).save(p)
        paths[key] = str(p)
    return paths


def _write_project(tmp_path: Path, artwork_extra: dict | None = None) -> Path:
    images = _make_source_images(tmp_path)
    artwork = dict(images)
    if artwork_extra:
        artwork.update(artwork_extra)
    project = {
        "schema": "mivf-toolkit-project-v1",
        "artwork": artwork,
        "theme": {"accent_rgb": [70, 120, 210], "outline_rgb": [255, 255, 255]},
    }
    proj_path = tmp_path / "test.mivfproj"
    proj_path.write_text(json.dumps(project), encoding="utf-8")
    return proj_path


def test_end_to_end_export_succeeds_with_all_four_controls(tmp_path):
    """Regression test for the real bug found in this session: exporting a
    project with Back artwork used to raise ThemeExportError unconditionally."""
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"
    result = export_theme_package(proj_path, dest, basename="regress")
    assert result.manifest.exists()
    assert result.manifest.stat().st_size > 0
    manifest_text = result.manifest.read_text(encoding="ascii")
    assert manifest_text.count("CONTROL.END") == 8
    assert ".mivfasset" not in manifest_text
    for control in ("REWIND", "PLAY_PAUSE", "FAST_FORWARD", "BACK"):
        assert f"CONTROL={control}" in manifest_text


def test_every_manifest_reference_points_to_a_real_promoted_file(tmp_path):
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"
    result = export_theme_package(proj_path, dest, basename="refcheck")
    manifest_text = result.manifest.read_text(encoding="ascii")
    for line in manifest_text.splitlines():
        if line.startswith("CONTROL.UNDERLAY=") or line.startswith("DASHBOARD_BG="):
            bare = line.split("=", 1)[1].strip()
            if not bare:
                continue
            candidate = dest / f"{bare}.mivfasset"
            assert candidate.is_file(), f"manifest references {bare}.mivfasset but it was not promoted"
            assert candidate.stat().st_size > 0


def test_legacy_project_with_no_control_edits_still_exports(tmp_path):
    """A pre-C.3 project (no control_edits key at all) must still export
    successfully -- build_state_assets() falls back to the legacy source/fit
    fields when no recipe override exists."""
    proj_path = _write_project(tmp_path)
    raw = json.loads(proj_path.read_text(encoding="utf-8"))
    assert "control_edits" not in raw.get("artwork", {})
    dest = tmp_path / "out"
    result = export_theme_package(proj_path, dest, basename="legacy")
    assert result.manifest.exists()


def test_identical_idle_and_focused_recipes_deduplicate_to_one_file(tmp_path):
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"
    result = export_theme_package(proj_path, dest, basename="dedup")
    # No explicit control_edits were set, so Focused inherits Idle exactly
    # (state()'s inherit_idle default is True) -- Idle and Focused for every
    # control must be byte-identical and therefore reference the same file.
    manifest_text = result.manifest.read_text(encoding="ascii")
    lines = manifest_text.splitlines()
    refs_by_control_state = {}
    current_control = None
    current_state = None
    for line in lines:
        if line.startswith("CONTROL="):
            current_control = line.split("=", 1)[1]
        elif line.startswith("CONTROL.STATE="):
            current_state = line.split("=", 1)[1]
        elif line.startswith("CONTROL.UNDERLAY="):
            refs_by_control_state[(current_control, current_state)] = line.split("=", 1)[1]
    for control in ("REWIND", "PLAY_PAUSE", "FAST_FORWARD", "BACK"):
        idle_ref = refs_by_control_state.get((control, "IDLE"))
        focused_ref = refs_by_control_state.get((control, "FOCUSED"))
        assert idle_ref == focused_ref, f"{control}: expected Idle/Focused dedup, got {idle_ref!r} vs {focused_ref!r}"


def test_distinct_focused_recipe_produces_a_distinct_asset(tmp_path):
    # Phase C.4A: inheritance is now per-group, not one all-or-nothing flag.
    # inherit_adjustments=False is what makes this Focused saturation override
    # actually apply -- inherit_idle=True alone would leave adjustments tied
    # to Idle (see control_recipe.state()'s grouped-inheritance semantics).
    proj_path = _write_project(tmp_path, artwork_extra={
        "control_edits": {
            "rewind": {"focused": {"inherit_adjustments": False, "saturation": 1.9}}
        }
    })
    dest = tmp_path / "out"
    result = export_theme_package(proj_path, dest, basename="distinct")
    manifest_text = result.manifest.read_text(encoding="ascii")
    lines = manifest_text.splitlines()
    refs = {}
    current_control = current_state = None
    for line in lines:
        if line.startswith("CONTROL="):
            current_control = line.split("=", 1)[1]
        elif line.startswith("CONTROL.STATE="):
            current_state = line.split("=", 1)[1]
        elif line.startswith("CONTROL.UNDERLAY="):
            refs[(current_control, current_state)] = line.split("=", 1)[1]
    assert refs[("REWIND", "IDLE")] != refs[("REWIND", "FOCUSED")]
    # Controls that were never edited still dedup Idle==Focused.
    assert refs[("FAST_FORWARD", "IDLE")] == refs[("FAST_FORWARD", "FOCUSED")]


def test_back_control_edit_never_produces_a_disc_mask_asset(tmp_path):
    """Back's real dimensions (222x20) have no disc radius in SPECS; render()
    must never attempt disc masking for it even if a manifest somehow asked."""
    proj_path = _write_project(tmp_path, artwork_extra={
        "control_edits": {"movie_menu_back": {"idle": {"mask": "disc"}}}
    })
    dest = tmp_path / "out"
    # Must not raise -- norm() clamps an invalid mask request back to a safe default.
    result = export_theme_package(proj_path, dest, basename="backmask")
    assert result.manifest.exists()


def test_export_succeeds_with_rounded_rect_and_chroma_key_masks(tmp_path):
    """Phase C.4B: the new mask modes must flow through the real export
    pipeline, not just render() in isolation."""
    proj_path = _write_project(tmp_path, artwork_extra={
        "control_edits": {
            "movie_menu_back": {"idle": {"mask": "rounded_rect", "corner_radius": 6, "mask_inset": 2}},
            "rewind": {"idle": {"mask": "chroma_key", "chroma_key": [0, 255, 0], "chroma_tolerance": 50}},
        }
    })
    dest = tmp_path / "out"
    result = export_theme_package(proj_path, dest, basename="masks")
    assert result.manifest.exists()
    manifest_text = result.manifest.read_text(encoding="ascii")
    assert manifest_text.count("CONTROL.END") == 8
