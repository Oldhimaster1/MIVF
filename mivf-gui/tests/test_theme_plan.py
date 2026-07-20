"""Phase C.5a: tests for theme_plan.py's PackagePlan -- the shared
read-only model behind Check Project, Export Dry Run, Change Summary, and
the real transactional exporter. Definition-of-done requirements (from the
user's own C.5a scope) are each covered by a named test below."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))

from PIL import Image  # noqa: E402
from mivf_gui import theme_plan  # noqa: E402
from mivf_gui.theme_export import export_theme_package  # noqa: E402


def _make_source_images(tmp_path: Path) -> dict[str, str]:
    # Distinct colors per control -- if two controls' source pixels were
    # identical, build_state_assets()'s cross-control byte-dedup would
    # legitimately (if confusingly) point them at the same physical file,
    # which would make hash-based change classification depend on which
    # control happened to be processed first. Real projects almost never
    # have two controls sharing pixel-identical artwork; keep the fixtures
    # realistic by giving each a different color.
    specs = {
        "dashboard_bg": (400, 300, (30, 40, 60, 255)),
        "rewind_underlay": (120, 120, (120, 160, 200, 255)),
        "play_pause_underlay": (140, 140, (200, 120, 160, 255)),
        "fast_forward_underlay": (120, 120, (160, 200, 120, 255)),
        "movie_menu_back": (300, 60, (90, 90, 140, 255)),
    }
    paths = {}
    for key, (w, h, color) in specs.items():
        p = tmp_path / f"{key}.png"
        Image.new("RGBA", (w, h), color).save(p)
        paths[key] = str(p)
    return paths


def _write_project(tmp_path: Path, artwork_extra: dict | None = None, name="test") -> Path:
    images = _make_source_images(tmp_path)
    artwork = dict(images)
    if artwork_extra:
        artwork.update(artwork_extra)
    project = {
        "schema": "mivf-toolkit-project-v1",
        "artwork": artwork,
        "theme": {"accent_rgb": [70, 120, 210], "outline_rgb": [255, 255, 255]},
    }
    proj_path = tmp_path / f"{name}.mivfproj"
    proj_path.write_text(json.dumps(project), encoding="utf-8")
    return proj_path


# --- 1. dry run writes nothing ---------------------------------------------

def test_dry_run_writes_nothing_to_destination(tmp_path):
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"
    plan = theme_plan.build_plan(proj_path, dest, basename="dry")
    assert plan.ok_to_export
    assert not dest.exists(), "build_plan() must never create or write into the destination folder"


# --- 2. every planned output has an exact hash before promotion -----------

def test_every_planned_file_has_a_real_hash_and_size(tmp_path):
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"
    plan = theme_plan.build_plan(proj_path, dest, basename="hashcheck")
    assert plan.ok_to_export
    for pf in plan.files:
        assert len(pf.sha256) == 64
        assert pf.size > 0


# --- 3. manifest references matching planned outputs, missing-source ERROR -

def test_missing_required_source_is_a_collected_error_not_a_raise(tmp_path):
    proj_path = _write_project(tmp_path)
    raw = json.loads(proj_path.read_text(encoding="utf-8"))
    del raw["artwork"]["rewind_underlay"]
    proj_path.write_text(json.dumps(raw), encoding="utf-8")
    dest = tmp_path / "out"
    plan = theme_plan.build_plan(proj_path, dest, basename="missing")
    assert not plan.ok_to_export
    assert any(m.category == "missing_source" and "rewind" in m.message.lower() for m in plan.errors)
    # Every OTHER role must still have been planned -- Preflight shows every
    # problem at once, not just the first one hit.
    roles = {pf.role for pf in plan.files}
    assert "dashboard_bg" in roles and "play_pause_idle" in roles


def test_manifest_over_budget_is_an_error():
    huge_manifest = "MIVFTHEME_SCHEMA=1\r\n" + ("X" * 5000)
    messages = theme_plan._validate_manifest(huge_manifest, Path("."), {}, set())
    assert any(m.category == "manifest_budget" for m in messages)


def test_manifest_block_count_must_be_exactly_eight():
    short_manifest = "MIVFTHEME_SCHEMA=1\r\nCONTROL.END\r\n"
    messages = theme_plan._validate_manifest(short_manifest, Path("."), {}, set())
    assert any(m.category == "manifest_block_count" for m in messages)


# --- 4. destination comparison uses real hashes (added/changed/unchanged) -

def test_change_classification_uses_real_hashes(tmp_path):
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"

    result = export_theme_package(proj_path, dest, basename="classify")
    plan_after_first_export = theme_plan.build_plan(proj_path, dest, basename="classify")
    assert all(pf.status == "unchanged" for pf in plan_after_first_export.files), \
        "an unmodified project re-planned against its own just-exported output must show everything unchanged"

    # Modify Rewind's Idle recipe; re-plan.
    raw = json.loads(proj_path.read_text(encoding="utf-8"))
    raw["artwork"]["control_edits"] = {"rewind": {"idle": {"saturation": 1.8}}}
    proj_path.write_text(json.dumps(raw), encoding="utf-8")
    plan_after_edit = theme_plan.build_plan(proj_path, dest, basename="classify")
    by_role = {pf.role: pf for pf in plan_after_edit.files if pf.role != "manifest"}
    assert by_role["rewind_idle"].status == "changed"
    assert by_role["fast_forward_idle"].status == "unchanged"
    assert result.manifest.exists()  # sanity: the real export actually happened


def test_added_status_for_a_brand_new_destination(tmp_path):
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "never_exported_here"
    plan = theme_plan.build_plan(proj_path, dest, basename="brandnew")
    assert all(pf.status == "added" for pf in plan.files)
    assert any(m.category == "destination" for m in plan.infos)


def test_removed_file_is_flagged_but_never_deleted(tmp_path):
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"
    export_theme_package(proj_path, dest, basename="stale")
    # Simulate a stale leftover from a prior export shape (e.g. an old role
    # name) that the current plan no longer produces.
    stale = dest / "stale.orphaned_role.mivfasset"
    stale.write_bytes(b"leftover-bytes")
    plan = theme_plan.build_plan(proj_path, dest, basename="stale")
    removed = [pf for pf in plan.files if pf.status == "removed"]
    assert any(pf.filename == stale.name for pf in removed)
    assert stale.is_file(), "Check Project / Dry Run must never delete anything from disk"


# --- 5. errors/warnings/info clearly separated -----------------------------

def test_severities_are_partitioned_correctly(tmp_path):
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"
    plan = theme_plan.build_plan(proj_path, dest, basename="sev")
    assert set(m.severity for m in plan.errors) <= {"ERROR"}
    assert set(m.severity for m in plan.warnings) <= {"WARNING"}
    assert set(m.severity for m in plan.infos) <= {"INFO"}
    assert len(plan.errors) + len(plan.warnings) + len(plan.infos) == len(plan.messages)


# --- 6. advisory checks: state similarity, empty mask, full mask ----------

def test_near_identical_focused_state_produces_similarity_warning(tmp_path):
    # brightness=0.03 is the smallest delta (against this fixture's solid
    # source color) that survives RGB565 quantization as a real byte
    # difference -- confirmed empirically -- while staying under the
    # similarity heuristic's own tolerance, i.e. genuinely "different bytes,
    # practically indistinguishable."
    proj_path = _write_project(tmp_path, artwork_extra={
        "control_edits": {"rewind": {"focused": {"inherit_adjustments": False, "brightness": 0.03}}}
    })
    dest = tmp_path / "out"
    plan = theme_plan.build_plan(proj_path, dest, basename="similar")
    assert any(m.category == "state_similarity" and m.role == "rewind_focused" for m in plan.warnings)


def test_empty_mask_produces_warning(tmp_path):
    # A 'full' mask's authoring value is always 255 -- no threshold can make
    # it empty. Use 'alpha' against a fully-transparent source instead, the
    # realistic way a control ends up with nothing visible.
    proj_path = _write_project(tmp_path)
    raw = json.loads(proj_path.read_text(encoding="utf-8"))
    transparent = tmp_path / "transparent.png"
    Image.new("RGBA", (120, 120), (120, 160, 200, 0)).save(transparent)
    raw["artwork"]["control_edits"] = {"rewind": {"idle": {"source": str(transparent), "mask": "alpha", "threshold": 1}}}
    proj_path.write_text(json.dumps(raw), encoding="utf-8")
    dest = tmp_path / "out"
    plan = theme_plan.build_plan(proj_path, dest, basename="emptymask")
    assert any(m.category == "empty_mask" and m.role == "rewind_idle" for m in plan.warnings)


def test_fully_opaque_mask_produces_info(tmp_path):
    proj_path = _write_project(tmp_path, artwork_extra={
        "control_edits": {"rewind": {"idle": {"mask": "full", "threshold": 1}}}
    })
    dest = tmp_path / "out"
    plan = theme_plan.build_plan(proj_path, dest, basename="fullmask")
    assert any(m.category == "full_mask" and m.role == "rewind_idle" for m in plan.infos)


# --- 7. Preflight and real export share one path; dry run predicts export -

def test_dry_run_hashes_match_the_subsequent_real_export(tmp_path):
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"
    plan = theme_plan.build_plan(proj_path, dest, basename="predict")
    predicted = {pf.filename: pf.sha256 for pf in plan.files if pf.role != "manifest"}

    result = export_theme_package(proj_path, dest, basename="predict")
    actual = {f.path.name: f.sha256 for f in result.files if f.role != "manifest"}

    assert predicted == actual, "a dry run must predict exactly the bytes the real export then writes"


# --- report formatters -----------------------------------------------------

def test_check_project_report_all_clear(tmp_path):
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"
    plan = theme_plan.build_plan(proj_path, dest, basename="report")
    report = theme_plan.format_check_project_report(plan)
    assert report.startswith("PROJECT READY")
    assert "✓ Project schema recognized" in report
    assert "✖" not in report.split("\n\n")[0]  # no failing checks in the checklist block
    assert "Errors:" not in report


def test_check_project_report_shows_errors_and_blocks(tmp_path):
    proj_path = _write_project(tmp_path)
    raw = json.loads(proj_path.read_text(encoding="utf-8"))
    del raw["artwork"]["rewind_underlay"]
    proj_path.write_text(json.dumps(raw), encoding="utf-8")
    dest = tmp_path / "out"
    plan = theme_plan.build_plan(proj_path, dest, basename="report")
    report = theme_plan.format_check_project_report(plan)
    # One missing source cascades into a render failure for both its states
    # plus a resulting incomplete manifest reference -- all real, all worth
    # surfacing in one pass rather than stopping at the first.
    assert "ERROR(S) FOUND -- EXPORT BLOCKED" in report.splitlines()[0]
    assert "✖ All required source artwork found" in report
    assert "Errors:" in report


def test_change_summary_groups_by_status(tmp_path):
    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"
    export_theme_package(proj_path, dest, basename="summary")
    raw = json.loads(proj_path.read_text(encoding="utf-8"))
    raw["artwork"]["control_edits"] = {"rewind": {"idle": {"saturation": 1.8}}}
    proj_path.write_text(json.dumps(raw), encoding="utf-8")
    plan = theme_plan.build_plan(proj_path, dest, basename="summary")
    summary = theme_plan.format_change_summary(plan)
    assert "Changed:" in summary and "• Rewind Idle" in summary
    assert "Unchanged:" in summary and "• Fast Forward Idle" in summary
    assert "Added:" in summary and "• (none)" in summary


def test_rollback_restores_prior_files_on_a_failed_promotion(tmp_path, monkeypatch):
    """The copy-aside/os.replace/restore-on-exception transaction predates
    C.5a; this confirms it survived the refactor to consume PackagePlan's
    staged files instead of a locally-built target list."""
    import os as os_module

    proj_path = _write_project(tmp_path)
    dest = tmp_path / "out"
    first = export_theme_package(proj_path, dest, basename="rollback")
    original_manifest_bytes = first.manifest.read_bytes()
    original_rewind_bytes = {f.path.name: f.path.read_bytes() for f in first.files if f.role != "manifest"}

    raw = json.loads(proj_path.read_text(encoding="utf-8"))
    raw["artwork"]["control_edits"] = {"rewind": {"idle": {"saturation": 1.9}}}
    proj_path.write_text(json.dumps(raw), encoding="utf-8")

    real_replace = os_module.replace
    call_count = {"n": 0}

    def flaky_replace(src, dst):
        call_count["n"] += 1
        if call_count["n"] == 3:
            raise OSError("simulated mid-promotion failure")
        return real_replace(src, dst)

    import mivf_gui.theme_export as theme_export_module
    monkeypatch.setattr(theme_export_module.os, "replace", flaky_replace)
    try:
        export_theme_package(proj_path, dest, basename="rollback")
        assert False, "the simulated failure must propagate, not be swallowed"
    except OSError:
        pass
    finally:
        monkeypatch.undo()

    assert first.manifest.read_bytes() == original_manifest_bytes, \
        "manifest must be restored to its pre-attempt bytes after a failed promotion"
    for name, data in original_rewind_bytes.items():
        assert (dest / name).read_bytes() == data, f"{name} must be restored after a failed promotion"


def test_export_refuses_when_plan_has_errors(tmp_path):
    proj_path = _write_project(tmp_path)
    raw = json.loads(proj_path.read_text(encoding="utf-8"))
    del raw["artwork"]["movie_menu_back"]
    proj_path.write_text(json.dumps(raw), encoding="utf-8")
    dest = tmp_path / "out"
    try:
        export_theme_package(proj_path, dest, basename="refuse")
        assert False, "export must refuse when the shared plan has any ERROR"
    except theme_plan.ThemeExportError as e:
        assert "Movie Menu Back" in str(e) or "movie_menu_back" in str(e)
    assert not dest.exists() or not any(dest.iterdir()), "a refused export must not leave partial output behind"
