"""Headless offscreen smoke test: constructs the real MainWindow and drives
real widgets/state, without needing an attached display. Run with
QT_QPA_PLATFORM=offscreen. Not a pytest file (kept as a standalone script so
it's trivial to run in one line for Phase C/C.1 evidence capture).

Phase C.1: extended to exercise the real preview-repair and Back-control
work -- setting a real source image on an AssetStatusRow and confirming the
PremierePreviewWidget/AssetInspector actually receive real, non-null
pixmaps (not just a boolean flag flipping), and that project save/reload
round-trips the new Back/fit-mode fields.

E.3.2b: the new subtitle-selector checks are the first part of this script
that needs a REAL, live ffprobe invocation (via MediaProbe against the
temp/e32b_multisub_test.mkv fixture) rather than PIL-only image fixtures.
media_probe.py's tool discovery includes an implicit current-working-
directory search (a real Windows shutil.which() behavior, not a bug) --
run this script with CWD at the REPO ROOT (where ffprobe.exe/ffmpeg.exe
live), e.g.:
    QT_QPA_PLATFORM=offscreen python mivf-gui/tests/smoke_offscreen.py
Running it from inside mivf-gui/ will make the subtitle checks silently
find zero streams (ffprobe undiscoverable) rather than fail loudly, since
an empty source is itself a valid state elsewhere in this script; watch for
"Subtitle selector checks SKIPPED" or a stream count of 1 ("No subtitle"
only) as the tell if this ever regresses.
"""
import dataclasses
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))

from PySide6.QtWidgets import QApplication  # noqa: E402
from mivf_gui.main_window import MainWindow  # noqa: E402
from mivf_gui.asset_pipeline import FitMode  # noqa: E402

app = QApplication(sys.argv)
win = MainWindow()
win.show()
print("window constructed and shown", flush=True)

REPO_ROOT = Path(__file__).resolve().parents[2]
FF_SRC = REPO_ROOT / "mivf_customization_gui_20260716" / "phase_c" / "assets" / "src_ff_underlay.png"
BG_SRC = REPO_ROOT / "mivf_customization_gui_20260716" / "phase_c" / "assets" / "src_dashboard_bg.png"
assert FF_SRC.exists(), f"fixture missing: {FF_SRC}"
assert BG_SRC.exists(), f"fixture missing: {BG_SRC}"

# --- Preview root-cause fix: selecting a real image must produce a real,
# non-null pixmap in the preview, not just flip a boolean ---
assert win.preview.ff_pixmap is None, "preview should start with no FF pixmap"
win.ff_row.path_edit.setText(str(FF_SRC))
win.ff_row._reprocess()  # editingFinished doesn't fire programmatically; call directly, same as a real edit would trigger
assert win.preview.ff_pixmap is not None and not win.preview.ff_pixmap.isNull(), \
    "selecting a real FF image must produce a real pixmap in the composited preview"
print("FF real pixmap present in composited preview: PASS", flush=True)

win.bg_row.path_edit.setText(str(BG_SRC))
win.bg_row._reprocess()
assert win.preview.bg_pixmap is not None and not win.preview.bg_pixmap.isNull(), \
    "selecting a real dashboard background must produce a real pixmap"
print("Dashboard background real pixmap present: PASS", flush=True)

# --- Status feedback: missing/invalid file must produce a clear message ---
win.ff_row.path_edit.setText(str(REPO_ROOT / "does_not_exist_anywhere.png"))
win.ff_row._reprocess()
status_text = win.ff_row.status_label.text()
assert "not found" in status_text.lower() or "missing" in status_text.lower(), \
    f"missing-file status should explain the problem, got: {status_text!r}"
print("Missing-file status message present: PASS", flush=True)

# restore a valid FF path before continuing
win.ff_row.path_edit.setText(str(FF_SRC))
win.ff_row._reprocess()

# --- Asset inspector shows real per-mode images for the selected element ---
win.inspector.element_combo.setCurrentText("Fast Forward")
win.inspector.mode_combo.setCurrentText("Source Artwork")
win.inspector._refresh()
assert not win.inspector.image_label.pixmap().isNull(), "inspector should show a real source-artwork pixmap"
win.inspector.mode_combo.setCurrentText("Mask")
win.inspector._refresh()
assert not win.inspector.image_label.pixmap().isNull(), "inspector should show a real mask pixmap for a masked control asset"
print("Asset inspector real per-mode pixmaps: PASS", flush=True)

# --- Cover Artwork card (Phase C.2): real thumbnail + exact headerless spec ---
COVER_SRC = REPO_ROOT / "mivf_customization_gui_20260716" / "phase_c" / "assets" / "src_cover.png"
if COVER_SRC.exists():
    win.cover_card.path_edit.setText(str(COVER_SRC))
    win.cover_card._reprocess()
    cover_result = win.cover_card.result()
    assert cover_result.runtime_bytes == 8800, f"cover must be exactly 8800 bytes (headerless), got {cover_result.runtime_bytes}"
    assert not win.cover_card.thumb_label.pixmap().isNull(), "cover card must show a real thumbnail"
    print("Cover Artwork card real thumbnail + exact 8800-byte headerless spec: PASS", flush=True)
else:
    print("Cover Artwork card check SKIPPED (fixture not present)", flush=True)

# --- Back control: real underlay + fill selection reflected in its own preview ---
BACK_SRC = REPO_ROOT / "mivf_customization_gui_20260716" / "phase_c" / "test_harness" / "fixtures" / "back_src.png"
if BACK_SRC.exists():
    win.back_row.path_edit.setText(str(BACK_SRC))
    win.back_row._reprocess()
    assert win.back_preview.underlay is not None and not win.back_preview.underlay.isNull(), \
        "Back underlay selection must produce a real pixmap in the Back preview"
    print("Back control real pixmap present: PASS", flush=True)
else:
    print("Back control pixmap check SKIPPED (fixture not present)", flush=True)

# --- Fit modes are real enum values wired to the combo ---
assert win.ff_row.fit_mode() == FitMode.CONTAIN, "default fit mode should be Contain"
idx = win.ff_row.fit_combo.findData(FitMode.COVER)
win.ff_row.fit_combo.setCurrentIndex(idx)
assert win.ff_row.fit_mode() == FitMode.COVER, "fit mode combo should reflect a real FitMode enum value"
win.ff_row.fit_combo.setCurrentIndex(win.ff_row.fit_combo.findData(FitMode.CONTAIN))  # restore

# --- Command generation still works end to end (regression check) ---
tmpdir = Path(tempfile.mkdtemp())
fake_source = tmpdir / "movie.mkv"
fake_source.write_bytes(b"not a real video, just needs to exist")
win.source_edit.setText(str(fake_source))
win.output_edit.setText(str(tmpdir / "movie.mivf"))
win.preset_combo.setCurrentIndex(win.preset_combo.findData("high_quality"))
win._generate_command()
preview_text = win.command_preview.toPlainText()
assert "--m2y2" in preview_text and "--keep" in preview_text, "command generation regressed"
print("Command generation regression check: PASS", flush=True)

# --- Project save/reload round-trips the new Back + fit-mode fields ---
win._sync_project_from_fields()
proj_path = tmpdir / "test.mivfproj"
win.project.artwork.movie_menu_back = str(BACK_SRC) if BACK_SRC.exists() else str(FF_SRC)
win.project.save(proj_path)

from mivf_gui.project import MivfProject  # noqa: E402
reloaded = MivfProject.load(proj_path)
assert reloaded.artwork.movie_menu_back == win.project.artwork.movie_menu_back, \
    "movie_menu_back must round-trip through save/load"
assert reloaded.artwork.fit_modes.get("fast_forward_underlay") == "contain", \
    "fit_modes must round-trip through save/load"
print("Project save/reload round-trips Back + fit-mode fields: PASS", flush=True)

# --- Phase C.3.1: main Preview tab must consume applied Control Artwork
# Studio recipes, not just the legacy AssetStatusRow path (this is the
# real bug found and fixed this session -- Apply wrote control_edits but
# _refresh_preview() never read it) ---
from PIL import Image  # noqa: E402

recipe_src = tmpdir / "rewind_recipe_src.png"
Image.new("RGBA", (100, 100), (255, 0, 0, 255)).save(recipe_src)

win.project.artwork.control_edits = {}
win._refresh_preview()
before_recipe = win.preview.rewind_pixmap

win.project.artwork.control_edits = {"rewind": {"idle": {"source": str(recipe_src), "saturation": 1.5}}}
win._refresh_preview()
after_recipe = win.preview.rewind_pixmap
assert after_recipe is not None and not after_recipe.isNull(), \
    "applying a Studio recipe must produce a real pixmap in the main Preview tab"
assert after_recipe.size().width() == 64 and after_recipe.size().height() == 60, \
    "recipe-rendered Rewind pixmap must be the real 64x60 runtime size"
print("Applied Studio recipe changes the main Preview pixmap: PASS", flush=True)

# --- Real bug report: the Asset Inspector kept showing the legacy
# AssetStatusRow's image (e.g. whatever the user last put in the plain
# "Rewind button face image" field, or nothing at all) even after a
# different image was assigned through the Control Artwork Studio -- the
# C.3.1 fix only ever touched the main Preview tab, never the Inspector.
# Confirm the Inspector's Rewind slot now shows the recipe's own source,
# not the legacy row's, whenever a recipe is applied. ---
win.rewind_row.path_edit.setText(str(FF_SRC))
win.rewind_row._reprocess()
win.project.artwork.control_edits = {"rewind": {"idle": {"source": str(recipe_src), "saturation": 1.5}}}
win._refresh_preview()
win.inspector.element_combo.setCurrentText("Rewind")
win.inspector.mode_combo.setCurrentText("Source Artwork")
win.inspector._refresh()
inspector_result = win.inspector._slots["Rewind"]()
assert inspector_result.source_w == 100 and inspector_result.source_h == 100, \
    "Inspector must reflect the Studio recipe's own source (100x100 fixture), not the legacy row's (FF_SRC)"
assert not win.inspector.image_label.pixmap().isNull()
print("Asset Inspector reflects an applied Studio recipe, not the stale legacy row: PASS", flush=True)

# Focus-dependent state resolution: an unset Focused recipe must inherit
# Idle (via control_recipe.state()'s own inherit_idle default), not crash
# or blank the control.
win._set_preview_focus_index(0)  # 0 = Rewind
assert win.preview.focused_index == 0
focused_pixmap = win.preview.rewind_pixmap
assert focused_pixmap is not None and not focused_pixmap.isNull(), \
    "focusing a control with only an Idle recipe must still render (Focused inherits Idle)"
print("Focus-dependent recipe state resolution (Focused inherits Idle): PASS", flush=True)

# A recipe referencing a missing source must fall back safely with a
# visible warning, never raise out of _refresh_preview().
win.project.artwork.control_edits = {"fast_forward": {"idle": {"source": str(tmpdir / "does_not_exist.png")}}}
win._refresh_preview()
assert "fast_forward" in win.preview_recipe_warning.text(), \
    "a missing recipe source must produce a localized warning naming the control"
print("Missing recipe source falls back safely with a localized warning: PASS", flush=True)

win.project.artwork.control_edits = {}
win._refresh_preview()

# --- Phase C.4A: Control Artwork Studio -- tint, grouped inheritance,
# copy-recipe tools ---
from mivf_gui.control_editor import ControlArtworkDialog  # noqa: E402
from PySide6.QtGui import QColor  # noqa: E402
from PySide6.QtWidgets import QInputDialog, QMessageBox  # noqa: E402

win.project.artwork.control_edits = {}
dlg = ControlArtworkDialog(win.project)

# Idle: set a real source + tint, confirm both land in the working recipe.
dlg.ctrl.setCurrentIndex(dlg.ctrl.findData("rewind"))
dlg.st.setCurrentIndex(0)  # Idle
dlg.src.setText(str(recipe_src))
dlg.tint = QColor(255, 200, 200)
dlg._set_tint_swatch()
dlg.tint_strength.setValue(0.5)
dlg.change()
idle_recipe = dlg.work["rewind"]["idle"]
assert idle_recipe["tint"] == [255, 200, 200] and idle_recipe["tint_strength"] == 0.5, \
    "tint color/strength must be written to the working recipe"
print("Studio tint UI writes tint color and strength: PASS", flush=True)

# Focused: grouped inheritance checkboxes -- unchecking only "Image
# adjustments" must leave geometry/mask inherited but adjustments independent.
dlg.st.setCurrentIndex(1)  # Focused
dlg.inh_geom.setChecked(True)
dlg.inh_mask.setChecked(True)
dlg.inh_adjust.setChecked(False)
dlg.bright.setValue(0.4)
dlg.change()
resolved = dlg.recipe()
assert resolved["source"] == str(recipe_src), "geometry group still inherited from Idle -> same source"
assert resolved["brightness"] == 0.4, "adjustments group independent -> Focused brightness applies"
print("Grouped Focused-state inheritance (per-group checkboxes): PASS", flush=True)

# Copy Idle -> Focused: Focused should become an independent snapshot of
# Idle's current values, with all three inherit flags now False.
dlg._copy_within("idle_to_focused")
foc_raw = dlg.work["rewind"]["focused"]
assert foc_raw["inherit_source_geometry"] is False and foc_raw["inherit_mask"] is False and foc_raw["inherit_adjustments"] is False
assert foc_raw["source"] == str(recipe_src) and foc_raw["tint"] == [255, 200, 200]
print("Copy Idle -> Focused recipe tool: PASS", flush=True)

# Mirror to a different control with matching geometry (Rewind -> Fast
# Forward, both 64x60): must not prompt, and must land on the target.
QInputDialog.getItem = staticmethod(lambda *a, **k: ("Fast Forward", True))
dlg.ctrl.setCurrentIndex(dlg.ctrl.findData("rewind"))
dlg.st.setCurrentIndex(1)
dlg._mirror_to()
assert dlg.work["fast_forward"]["focused"]["source"] == str(recipe_src)
print("Mirror-to a same-geometry control: PASS", flush=True)

# Mirror to a control with different geometry (Rewind 64x60 -> Movie-menu
# Back 222x20): must show the geometry-mismatch warning before proceeding.
warn_calls = []
QMessageBox.warning = staticmethod(lambda *a, **k: (warn_calls.append(1), QMessageBox.Yes)[1])
QInputDialog.getItem = staticmethod(lambda *a, **k: ("Movie-menu Back", True))
dlg._mirror_to()
assert warn_calls, "mirroring between controls with different geometry must warn before copying"
assert dlg.work["movie_menu_back"]["focused"]["source"] == str(recipe_src)
print("Mirror-to a different-geometry control warns before copying: PASS", flush=True)

# Cancel must never touch the project's real control_edits.
dlg.reject()
assert win.project.artwork.control_edits == {}, "Cancel must discard all Studio edits made during this session"
print("Studio Cancel discards edits (project untouched): PASS", flush=True)

win.project.artwork.control_edits = {}
win._refresh_preview()

# --- Phase C.4B: undo/redo, drag-and-drop, rounded-rect + chroma-key masks ---
from PySide6.QtCore import QUrl  # noqa: E402

win.project.artwork.control_edits = {}
dlg2 = ControlArtworkDialog(win.project)
dlg2.show()  # isVisible()-based field show/hide assertions below need a shown top-level window
dlg2.ctrl.setCurrentIndex(dlg2.ctrl.findData("rewind"))
dlg2.st.setCurrentIndex(0)  # Idle

assert dlg2.undo_stack == [], "a freshly opened Studio session must start with empty undo history"

# Rapid continuous edits (simulating one slider drag) must coalesce into a
# single undo entry -- without a running Qt event loop the coalescing timer
# never fires, which deterministically proves these three edits stayed
# inside one open coalescing window rather than each pushing its own entry.
dlg2.bright.setValue(0.1)
dlg2.change()
dlg2.bright.setValue(0.2)
dlg2.change()
dlg2.bright.setValue(0.3)
dlg2.change()
assert len(dlg2.undo_stack) == 1, "rapid continuous spinbox edits must coalesce into one undo entry"
print("Undo coalesces rapid continuous edits into one entry: PASS", flush=True)

# A discrete, named operation (inherit-group toggle) must always open a
# fresh boundary, even while a coalescing window from the drag above is
# still open.
dlg2.st.setCurrentIndex(1)  # Focused
before = len(dlg2.undo_stack)
dlg2.inh_adjust.setChecked(False)
assert len(dlg2.undo_stack) == before + 1, "an inheritance-group toggle must always create its own undo boundary"
print("Discrete inheritance toggle forces a fresh undo boundary: PASS", flush=True)

# Mask-mode switch: field visibility follows the selected mode, and the
# switch itself is a forced (isolated) undo boundary.
before = len(dlg2.undo_stack)
dlg2.mask.setCurrentIndex(dlg2.mask.findData("rounded_rect"))
assert len(dlg2.undo_stack) == before + 1
assert dlg2.corner_radius_row.isVisible() and dlg2.mask_inset_row.isVisible()
assert not dlg2.chroma_color_row.isVisible()
dlg2.mask.setCurrentIndex(dlg2.mask.findData("chroma_key"))
assert dlg2.chroma_color_row.isVisible() and dlg2.chroma_tolerance_row.isVisible() and dlg2.chroma_mode_row.isVisible()
assert not dlg2.corner_radius_row.isVisible()
print("Mask-mode switch shows/hides the right fields and is its own undo boundary: PASS", flush=True)

# Undo/redo round-trip on a real value, and redo is cleared by a new edit.
# Switch back to Idle first: Focused's "Mask settings" group is still
# inheriting from Idle here (only inherit_adjustments was unchecked above),
# so editing chroma_tolerance while on Focused would be masked by that live
# inheritance -- Idle has no such ambiguity.
dlg2.st.setCurrentIndex(0)
pre_tolerance = dlg2.recipe()["chroma_tolerance"]
dlg2.chroma_tolerance.setValue(pre_tolerance + 37)
dlg2.change()
assert dlg2.recipe()["chroma_tolerance"] == pre_tolerance + 37
dlg2.undo()
assert dlg2.recipe()["chroma_tolerance"] == pre_tolerance, "undo must revert the last edit"
dlg2.redo()
assert dlg2.recipe()["chroma_tolerance"] == pre_tolerance + 37, "redo must reapply the undone edit"
dlg2.undo()
assert dlg2.redo_stack, "redo stack should be populated immediately after an undo"
dlg2.chroma_softness.setValue(dlg2.chroma_softness.value() + 5)
dlg2.change()
assert not dlg2.redo_stack, "making a new edit after undo must clear the redo stack"
print("Undo/redo round-trip and redo-cleared-by-new-edit: PASS", flush=True)

# Drag-and-drop: reject multi-file, wrong-extension, and undecodable drops
# without touching the source or undo history; accept exactly one valid
# image as exactly one undoable operation, and never modify the original.
dlg2.ctrl.setCurrentIndex(dlg2.ctrl.findData("fast_forward"))
dlg2.st.setCurrentIndex(0)
before_src = dlg2.src.text()
before_undo_len = len(dlg2.undo_stack)

dlg2._handle_drop([QUrl.fromLocalFile(str(recipe_src)), QUrl.fromLocalFile(str(FF_SRC))])
assert dlg2.src.text() == before_src and len(dlg2.undo_stack) == before_undo_len, "a multi-file drop must be rejected"

bad_ext = tmpdir / "not_an_image.txt"
bad_ext.write_text("nope")
dlg2._handle_drop([QUrl.fromLocalFile(str(bad_ext))])
assert dlg2.src.text() == before_src and len(dlg2.undo_stack) == before_undo_len, "an unsupported extension must be rejected"

fake_png = tmpdir / "fake.png"
fake_png.write_bytes(b"not really a png")
dlg2._handle_drop([QUrl.fromLocalFile(str(fake_png))])
assert dlg2.src.text() == before_src and len(dlg2.undo_stack) == before_undo_len, "an undecodable file must be rejected even with a valid extension"

orig_bytes = recipe_src.read_bytes()
dlg2._handle_drop([QUrl.fromLocalFile(str(recipe_src))])
# QUrl.toLocalFile() normalizes to forward slashes on Windows, so compare
# via Path rather than raw string equality.
assert Path(dlg2.src.text()) == recipe_src
assert len(dlg2.undo_stack) == before_undo_len + 1, "a valid drop must create exactly one undo entry"
assert recipe_src.read_bytes() == orig_bytes, "dropping an image must never modify the original file"
print("Drag-and-drop validation, rejection cases, and single undo entry: PASS", flush=True)

# Eyedropper: must sample the exact rendered pixel color under the click
# point and count as one undoable operation.
dlg2.ctrl.setCurrentIndex(dlg2.ctrl.findData("rewind"))
dlg2.st.setCurrentIndex(0)
solid_src = tmpdir / "solid_blue.png"
Image.new("RGBA", (64, 60), (30, 60, 200, 255)).save(solid_src)
dlg2.src.setText(str(solid_src))
dlg2.mask.setCurrentIndex(dlg2.mask.findData("chroma_key"))
dlg2.change()
assert dlg2._last_rendered is not None
dlg2.start_eyedrop()
assert dlg2._eyedrop_active is True

class _FakePos:
    def __init__(self, x, y):
        self._x, self._y = x, y

    def x(self):
        return self._x

    def y(self):
        return self._y


class _FakeClickEvent:
    def position(self):
        return _FakePos(dlg2.view.width() / 2, dlg2.view.height() / 2)


before_undo_len = len(dlg2.undo_stack)
dlg2._on_view_click(_FakeClickEvent())
assert dlg2._eyedrop_active is False, "eyedropper must deactivate after a successful sample"
assert (dlg2.chroma.red(), dlg2.chroma.green(), dlg2.chroma.blue()) == (30, 60, 200), \
    "eyedropper must sample the exact rendered source color"
assert len(dlg2.undo_stack) == before_undo_len + 1, "the eyedropper sample must be one undoable operation"
print("Eyedropper samples the exact rendered pixel color: PASS", flush=True)

# Both mask preview thumbnails (authoring + final binary) must be real,
# non-null pixmaps once a mask is rendered.
assert not dlg2.mask_authoring_view.pixmap().isNull() and not dlg2.mask_final_view.pixmap().isNull(), \
    "both the authoring and final 1-bit mask previews must show real pixmaps"
print("Dual authoring/final-binary mask preview thumbnails: PASS", flush=True)

dlg2.reject()
win.project.artwork.control_edits = {}
win._refresh_preview()

# --- Phase C.5a: Check Project dialog populates from a real PackagePlan,
# and never writes into the destination folder ---
from mivf_gui.preflight_dialog import PreflightDialog  # noqa: E402
from mivf_gui import theme_plan  # noqa: E402

assert win.project.project_path is not None, "the earlier save/reload block must have set project_path"
preflight_dest = tmpdir / "preflight_out"
plan = theme_plan.build_plan(win.project.project_path, preflight_dest, basename="smoke")
dlg = PreflightDialog(win, str(win.project.project_path))
dlg.destination.setText(str(preflight_dest))
dlg._done_ok(plan)
report_text = dlg.report_view.toPlainText()
assert "PROJECT READY" in report_text or "ERROR(S) FOUND" in report_text
assert dlg.table.rowCount() == len(plan.files)
assert not preflight_dest.exists(), "Check Project must never create or write into the destination folder"
if plan.files:
    dlg.table.selectRow(0)
    dlg._update_asset_detail()
    assert dlg.asset_detail.text() != "Select a row to see its effective recipe."
print("Check Project dialog reflects a real PackagePlan and writes nothing: PASS", flush=True)

# --- E.1.2: Movie Information dialog -- working-copy discipline (Apply
# commits, Cancel discards), manual field editing, and a graceful failure
# path when no ffprobe is available (real for this dev environment -- see
# E.1.1's own audit finding that ffmpeg/ffprobe aren't installed here). ---
from mivf_gui.movie_info_dialog import MovieInfoDialog  # noqa: E402

win.project.movie_info.title = None
dlg2_movie = MovieInfoDialog(win.project)
dlg2_movie.widgets["title"].setText("Les Misérables")
dlg2_movie.widgets["synopsis"].setPlainText("An escaped convict pursued across decades.")
assert dlg2_movie.work.title == "Les Misérables"
assert win.project.movie_info.title is None, "editing the dialog's working copy must not touch the real project yet"
dlg2_movie.reject()
assert win.project.movie_info.title is None, "Cancel must leave the project's movie_info untouched"
print("Movie Information dialog Cancel discards edits (project untouched): PASS", flush=True)

dlg3_movie = MovieInfoDialog(win.project)
dlg3_movie.widgets["title"].setText("Les Misérables")
dlg3_movie.widgets["director"].setText("Tom Hooper")
dlg3_movie.apply()
assert win.project.movie_info.title == "Les Misérables"
assert win.project.movie_info.director == "Tom Hooper"
assert win.project.movie_info.field_provenance["title"] == "manual"
print("Movie Information dialog Apply commits the working copy: PASS", flush=True)

# No real ffprobe binary in this environment (confirmed during E.1.1's own
# audit) -- "Import from Source" must fail gracefully with a message, never
# raise out of the dialog.
win.project.source_media = str(recipe_src)
dlg4_movie = MovieInfoDialog(win.project)
dlg4_movie._import_from_source()  # must not raise even though ffprobe is unavailable
print("Movie Information dialog import handles a missing ffprobe tool gracefully: PASS", flush=True)

win.project.movie_info = win.project.movie_info.__class__()
win.project.source_media = None

# --- E.3.1: Storage Planner dialog -- real StoragePlan, never deletes,
# never crashes even with an unsaved/incomplete project. Bypass the QThread
# worker the same way the Check Project smoke check does (call the real
# build function directly and feed it to _done_ok), since this script
# never runs a Qt event loop. ---
from mivf_gui.storage_plan_dialog import StoragePlanDialog, format_report  # noqa: E402
from mivf_gui import storage_plan as sp  # noqa: E402

dlg_storage = StoragePlanDialog(win.project)
plan = sp.build_storage_plan(win.project)
dlg_storage._done_ok(plan)
report_text = dlg_storage.report_view.toPlainText()
assert "SOURCE & OUTPUT" in report_text and "VOLUMES" in report_text
assert "UNKNOWN" in report_text or "KNOWN EXACT" in report_text  # every field is classified, never silently blank
print("Storage Planner dialog reflects a real StoragePlan: PASS", flush=True)

# StoragePlanDialog starts its normal background refresh worker in __init__.
# This smoke test also feeds a synchronous plan into _done_ok(), but must not
# let the constructor-started QThread outlive the dialog/process. Waiting here
# makes teardown deterministic and prevents Qt's fatal
# "QThread: Destroyed while thread is still running" diagnostic.
if dlg_storage.worker is not None and dlg_storage.worker.isRunning():
    assert dlg_storage.worker.wait(30000), "Storage Planner worker did not finish within 30 seconds"

# --- E.3.2b: GUI-level subtitle-selector regression (closes the test-coverage
# gap identified during the E.3.2a reconciliation for this same widget layer:
# CLI/persistence logic was unit-tested, but the live QComboBox widgets and
# their reconciliation logic were only manually validated). Real fixture,
# real MediaProbe/ffprobe call, real widget state -- not mocked. ---
from mivf_gui import media_probe as _media_probe  # noqa: E402
import mivf_gui.main_window as _main_window_mod  # noqa: E402

SUBTITLE_FIXTURE = REPO_ROOT / "temp" / "e32b_multisub_test.mkv"
if SUBTITLE_FIXTURE.exists():
    win.source_edit.setText(str(SUBTITLE_FIXTURE))
    win._probe_source_streams(show_errors=True)

    # 4/14: multiple subtitle streams, unusual absolute ordering (video, sub,
    # audio, sub, sub) -- widget construction and type-relative labeling.
    assert win.subtitle_stream_combo.count() == 4, \
        f"expected 'No subtitle' + 3 real subtitle streams, got {win.subtitle_stream_combo.count()}"
    labels = [win.subtitle_stream_combo.itemText(i) for i in range(win.subtitle_stream_combo.count())]
    assert any("Spanish" in l for l in labels), labels
    assert any("English" in l for l in labels) and any("default" in l for l in labels)
    assert any("Signs" in l for l in labels) and any("forced" in l for l in labels)
    print("Subtitle selector: real fixture population, unusual ordering, default/forced labels: PASS", flush=True)

    # 3: explicit supported text-subtitle selection.
    eng_index = next(i for i, l in enumerate(labels) if "English" in l)
    win.subtitle_stream_combo.setCurrentIndex(eng_index)
    win._update_subtitle_edition_enabled()
    assert win.subtitle_edition_combo.isEnabled()
    win.subtitle_edition_combo.setCurrentIndex(win.subtitle_edition_combo.findData(1))
    win._sync_project_from_fields()
    assert win.project.subtitle_stream_index == win.subtitle_stream_combo.itemData(eng_index)
    assert win.project.subtitle_edition == 1
    print("Subtitle selector: explicit selection + edition slot captured on sync: PASS", flush=True)

    # 10: saved selection survives project save/reopen.
    subtitle_tmpdir = Path(tempfile.mkdtemp())
    subtitle_proj_path = subtitle_tmpdir / "subtitle_test.mivfproj"
    win.project.save(subtitle_proj_path)
    from mivf_gui.project import MivfProject as _MivfProject
    reloaded = _MivfProject.load(subtitle_proj_path)
    assert reloaded.subtitle_stream_index == win.project.subtitle_stream_index
    assert reloaded.subtitle_edition == 1
    print("Subtitle selector: save/reopen restores explicit selection: PASS", flush=True)

    # 11/12: changed-source identity invalidates the saved selection safely,
    # and never silently retains it -- reuses the exact real reconciliation
    # code path, against a genuinely different real file (not a relabeled copy).
    other_fixture = REPO_ROOT / "temp" / "pi_scroll_multiaudio_test.mkv"
    if other_fixture.exists():
        win.project = reloaded
        win.source_edit.setText(str(other_fixture))
        win._probe_source_streams(show_errors=False)
        assert win.subtitle_stream_combo.currentData() is None, \
            "changed source identity must reset a saved subtitle selection to 'No subtitle', never silently keep it"
        assert not win.subtitle_edition_combo.isEnabled()
        print("Subtitle selector: changed-source identity resets saved selection safely: PASS", flush=True)
    else:
        print("Subtitle selector changed-source check SKIPPED (second fixture not present)", flush=True)

    # 8/9: bitmap/unknown subtitle streams are visible but disabled, not
    # silently omitted -- exercised against a real Qt combo box model,
    # not just the data classification (already unit-tested separately).
    _orig_probe = _main_window_mod.probe_media_cached
    try:
        def _fake_probe_with_bitmap(path, **kwargs):
            real = _orig_probe(SUBTITLE_FIXTURE, compute_hash=False)
            fake_sub = _media_probe.SubtitleStreamInfo(
                absolute_index=99, stream_index=0, codec="dvd_subtitle", kind="bitmap",
                language="fra", title="Forced Bitmap", is_default=False, is_forced=False,
                is_hearing_impaired=False,
            )
            return dataclasses.replace(real, subtitle_streams=(fake_sub,) + real.subtitle_streams)
        _main_window_mod.probe_media_cached = _fake_probe_with_bitmap
        win.source_edit.setText(str(SUBTITLE_FIXTURE))
        win._probe_source_streams(show_errors=False)
        bitmap_item = win.subtitle_stream_combo.model().item(1)  # index 0 is "No subtitle"
        assert bitmap_item is not None and not bitmap_item.isEnabled(), \
            "a bitmap subtitle stream must be visible in the selector but not selectable"
        assert "unsupported" in win.subtitle_stream_combo.itemText(1).lower()
        print("Subtitle selector: bitmap stream shown but disabled, not silently omitted: PASS", flush=True)
    finally:
        _main_window_mod.probe_media_cached = _orig_probe
else:
    print("Subtitle selector checks SKIPPED (fixture not present)", flush=True)

# --- C.6: Dashboard Canvas dialog -- real QWidget drag/undo/Cancel discipline ---
from mivf_gui.dashboard_canvas import DashboardCanvasDialog  # noqa: E402

canvas_project = win.project
canvas_project.dashboard_layout = {}
dlg = DashboardCanvasDialog(canvas_project)

# Programmatic drag + undo/redo (mouse events aren't synthesizable
# meaningfully offscreen; set_offset/commit_drag are the exact same code
# path a real drag calls into).
dlg.set_offset("REWIND", 20, 10, push_undo=True)
assert dlg.layout_offsets["REWIND"] == [20, 10]
dlg.undo()
assert "REWIND" not in dlg.layout_offsets
dlg.redo()
assert dlg.layout_offsets["REWIND"] == [20, 10]
print("Dashboard Canvas: drag-equivalent set_offset + undo/redo: PASS", flush=True)

# Reset per-control and reset-all.
dlg.set_offset("FAST_FORWARD", -15, 5, push_undo=True)
dlg.selected_control = "REWIND"
dlg._reset_selected()
assert "REWIND" not in dlg.layout_offsets and dlg.layout_offsets.get("FAST_FORWARD") == [-15, 5]
dlg._reset_all()
assert dlg.layout_offsets == {}
print("Dashboard Canvas: reset-control and reset-all: PASS", flush=True)

# Cancel discipline: reject() must never touch the project.
dlg.set_offset("PLAY_PAUSE", 30, -20, push_undo=True)
dlg.reject()
assert canvas_project.dashboard_layout == {}, "Cancel must discard edits, project must remain untouched"
print("Dashboard Canvas: Cancel discards edits (project untouched): PASS", flush=True)

# Accept commits a valid layout.
dlg2 = DashboardCanvasDialog(canvas_project)
dlg2.set_offset("REWIND", 15, 8, push_undo=True)
dlg2.accept()
assert canvas_project.dashboard_layout == {"REWIND": [15, 8]}
print("Dashboard Canvas: Accept commits a valid layout to the project: PASS", flush=True)

# Accept's error-blocking precondition, WITHOUT calling accept() itself:
# accept() shows a real QMessageBox.warning() on error, which is exactly
# the kind of modal that hung an earlier offscreen smoke run (see the
# E.3.2a reconciliation) -- verify the same current_messages() the real
# accept() gates on, rather than triggering the modal in this script.
dlg3 = DashboardCanvasDialog(canvas_project)
dlg3.set_offset("FAST_FORWARD", 900, 0, push_undo=True)  # out-of-range -> ERROR
errors = [m for m in dlg3.current_messages() if m.severity == "ERROR"]
assert errors, "an out-of-range offset must produce a blocking ERROR before Accept would commit"
print("Dashboard Canvas: invalid layout produces a blocking ERROR (Accept-gate verified without triggering the modal): PASS", flush=True)
canvas_project.dashboard_layout = {}

# --- Progress terminal-state regression (audit_correction2_20260718_102147):
# after completion the "Remaining" label must show an accurate terminal
# value, never a stale "Calculating..." carried over from the last
# in-progress update (which, for a finalizing/non-frame-counted stage, never
# had an ETA to begin with). Exercises the real method with synthetic state
# rather than running a full subprocess encode. ---
import time as _time  # noqa: E402
from mivf_gui.backend import EncodeProgress  # noqa: E402

win.encode_started_monotonic = _time.monotonic() - 5.0
win._last_progress = EncodeProgress("finalizing", "Finalizing", detail="Verifying output")
win._refresh_elapsed_label(finished=True)
timing_text = win.progress_timing_label.text()
assert "Calculating" not in timing_text, f"terminal state must not show 'Calculating...', got: {timing_text!r}"
assert "Remaining 0:00" in timing_text, f"terminal state should show an accurate zero remaining time, got: {timing_text!r}"
print("Progress terminal-state shows accurate Remaining, not stale Calculating: PASS", flush=True)

# --- UX.1 Project Home + guided creation --------------------------------
# ProjectHomeDialog/NewProjectWizard are never .exec()'d here -- same
# reasoning as the Dashboard Canvas checks above: exec() pumps a real
# nested event loop with no user present to click a button, which would
# hang. Widgets are driven directly and the dialogs' own accept-triggering
# methods are called directly instead (accept()/reject() alone don't block).
from PySide6.QtCore import Qt as _Qt  # noqa: E402
from PySide6.QtWidgets import QDialog as _QDialog  # noqa: E402
from mivf_gui.project_home import (  # noqa: E402
    ProjectHomeDialog, NewProjectWizard,
    load_recent_projects, add_recent_project, _settings, _RECENT_KEY,
)

# Isolate from any real recent-projects state a prior local run may have
# left in the OS-level QSettings store for this org/app pair.
_settings().remove(_RECENT_KEY)

home_empty = ProjectHomeDialog(win)
assert home_empty.recent_list.count() == 1  # the "(none yet)" placeholder
home_empty._choose_skip()
assert home_empty.result_action == ProjectHomeDialog.SKIP
print("Project Home: empty recent list shows placeholder, Skip sets SKIP: PASS", flush=True)

fake_project_path = tmpdir / "recent_test.mivfproj"
# source_media points at a real file (fake_source, created above) so
# loading this project never trips the "Project needs relink" warning --
# same modal-hang avoidance already established for Dashboard Canvas
# above (a real user-facing QMessageBox in offscreen mode has no user to
# click it).
MivfProject(source_media=str(fake_source), output_path=str(tmpdir / "recent_test.mivf")).save(fake_project_path)
add_recent_project(str(fake_project_path))
home_with_recent = ProjectHomeDialog(win)
assert home_with_recent.recent_list.count() == 1
first_item = home_with_recent.recent_list.item(0)
assert str(fake_project_path) in first_item.text()
home_with_recent._choose_recent(first_item)
assert home_with_recent.result_action == ProjectHomeDialog.RECENT
assert home_with_recent.recent_path == str(fake_project_path)
print("Project Home: recent entry round-trips through double-click selection: PASS", flush=True)

missing_path = str(tmpdir / "does_not_exist.mivfproj")
add_recent_project(missing_path)
home_missing = ProjectHomeDialog(win)
missing_item = next(
    home_missing.recent_list.item(i) for i in range(home_missing.recent_list.count())
    if home_missing.recent_list.item(i).data(_Qt.UserRole) == missing_path
)
assert "(missing)" in missing_item.text()
home_missing._choose_recent(missing_item)
assert missing_path not in load_recent_projects()
print("Project Home: selecting a missing recent entry prunes it instead of crashing: PASS", flush=True)

wizard = NewProjectWizard(win)
assert not wizard.next_btn.isEnabled()  # no source chosen yet
wizard.source_edit.setText(str(fake_source))
assert wizard.next_btn.isEnabled()
wizard._go_next()
wizard.output_edit.setText(str(tmpdir / "wizard_out.mivf"))
wizard.preset_combo.setCurrentIndex(wizard.preset_combo.findData("balanced"))
wizard._finish()
assert wizard.result() == _QDialog.Accepted
assert wizard.source_path == str(fake_source)
assert wizard.preset_name == "balanced"
print("New Project wizard: Next gated on source, Finish captures source/output/preset: PASS", flush=True)

win.project.source_media = "stale_marker.mkv"
win._apply_new_project_wizard_result(wizard)
assert win.source_edit.text() == str(fake_source)
assert win.output_edit.text() == str(tmpdir / "wizard_out.mivf")
assert win.preset_combo.currentData() == "balanced"
assert win.centralWidget().currentIndex() == 0
print("MainWindow applies a finished wizard's result and lands on the Project tab: PASS", flush=True)

_settings().remove(_RECENT_KEY)
win._handle_project_home_result(ProjectHomeDialog.RECENT, str(fake_project_path))
assert win.project.source_media == str(fake_source)
print("MainWindow._handle_project_home_result(RECENT, ...) loads the chosen project: PASS", flush=True)

win._handle_project_home_result(ProjectHomeDialog.SKIP, None)
assert win.project.source_media == str(fake_source)  # unchanged -- Skip is a true no-op
print("MainWindow._handle_project_home_result(SKIP, ...) leaves the editor untouched: PASS", flush=True)

# --- Phase 3: unified Create MIVF preflight -------------------------------
# _generate_command() now returns a bool (True = safe to launch) and is
# always re-run fresh by _launch_encode before every real launch -- the
# real bug this closes is _launch_encode() previously reusing a stale
# self._argv from an earlier generate call if a field was edited afterward
# without regenerating. Tested here at the level of "regenerating always
# reflects current field state," not by driving the real async probe +
# subprocess launch (too heavy/non-deterministic for an offscreen check).
win.source_edit.setText(str(fake_source))
win.output_edit.setText(str(tmpdir / "preflight_a.mivf"))
win.preset_combo.setCurrentIndex(win.preset_combo.findData("balanced"))
assert win._generate_command() is True
argv_a = list(win._argv)
assert str(tmpdir / "preflight_a.mivf") in " ".join(argv_a)

win.output_edit.setText(str(tmpdir / "preflight_b.mivf"))
assert win._generate_command() is True
argv_b = list(win._argv)
assert str(tmpdir / "preflight_b.mivf") in " ".join(argv_b)
assert argv_a != argv_b, "regenerating must reflect the field change, not reuse a stale argv"
print("Preflight: _generate_command() always reflects current field state (stale-argv bug fixed): PASS", flush=True)

summary_text = win.review_summary.toPlainText()
# fake_source is deliberately unprobeable garbage bytes (not a real video),
# so the honest, correct result here is an UNKNOWN disk-space estimate --
# always shown, never silently dropped, never a false-confident number.
assert "Disk space:" in summary_text, \
    f"preflight should always surface a disk-space line, even when unknown, got: {summary_text!r}"
assert "BLOCKING" not in summary_text  # unknown space must never itself be treated as a blocking failure
print("Preflight: disk-space check always reports into the review summary, honest about unknown state: PASS", flush=True)

not_writable_dir = tmpdir / "readonly_dest"
not_writable_dir.mkdir()
import stat as _stat  # noqa: E402
os.chmod(str(not_writable_dir), _stat.S_IREAD)
try:
    win.output_edit.setText(str(not_writable_dir / "blocked.mivf"))
    result = win._generate_command()
    if not os.access(str(not_writable_dir), os.W_OK):
        # os.chmod's read-only effect on directories is platform-dependent
        # (notably weak on some Windows configurations) -- only assert the
        # blocking behavior where the OS actually reports it as unwritable,
        # so this check is honest about what it can prove on this host
        # rather than assuming POSIX semantics everywhere.
        assert result is False
        assert "not writable" in win.review_summary.toPlainText()
        print("Preflight: unwritable destination directory blocks Create MIVF: PASS", flush=True)
    else:
        print("Preflight: unwritable-destination check SKIPPED (this OS/filesystem did not honor chmod read-only on a directory)", flush=True)
finally:
    os.chmod(str(not_writable_dir), _stat.S_IWRITE | _stat.S_IREAD)

# --- D: Persistent multi-project encode queue -----------------------------
# _start_queue()/_start_next_queue_job() are deliberately never called here
# -- they launch a real EncodeRun subprocess, which is too heavy/slow/
# non-deterministic for an offscreen smoke check (same reasoning already
# applied to the single-project Run tab, which has no launch test either).
# This exercises the queue's own GUI-level wiring: add/reorder/remove and
# the persistence round-trip, including the crash/restart safety rule.
from mivf_gui import encode_queue as _eq  # noqa: E402
_eq._settings().remove(_eq._QUEUE_KEY)
win.queue_jobs = []
win.queue_jobs_root = tmpdir / "queue_jobs"  # keep queue job dirs inside the test tmpdir, not the real home dir
win._refresh_queue_list()

win.source_edit.setText(str(fake_source))
win.output_edit.setText(str(tmpdir / "queue_a.mivf"))
win._sync_project_from_fields()
queue_proj_a = tmpdir / "queue_a.mivfproj"
win.project.save(queue_proj_a)
win._add_current_project_to_queue()
assert len(win.queue_jobs) == 1
assert win.queue_jobs[0].project_path == str(queue_proj_a)
assert win.queue_jobs[0].status == _eq.STATUS_PENDING
assert win.queue_list.count() == 1
print("Queue: Add Current Project to Queue appends a real, saved-project-backed job: PASS", flush=True)

win.output_edit.setText(str(tmpdir / "queue_b.mivf"))
win._sync_project_from_fields()
queue_proj_b = tmpdir / "queue_b.mivfproj"
win.project.save(queue_proj_b)
win._add_current_project_to_queue()
assert len(win.queue_jobs) == 2
job_a_id, job_b_id = win.queue_jobs[0].job_id, win.queue_jobs[1].job_id

win.queue_list.setCurrentRow(1)
win._move_selected_queue_job(-1)
assert [j.job_id for j in win.queue_jobs] == [job_b_id, job_a_id]
print("Queue: Move Up reorders jobs and persists the new order: PASS", flush=True)

win.queue_list.setCurrentRow(0)
win._remove_selected_queue_job()
assert [j.job_id for j in win.queue_jobs] == [job_a_id]
assert win.queue_list.count() == 1
print("Queue: Remove Selected deletes the right job: PASS", flush=True)

reloaded_jobs = _eq.load_queue()
assert [j.job_id for j in reloaded_jobs] == [job_a_id]
print("Queue: state persists through QSettings (survives a fresh load_queue() call): PASS", flush=True)

# Crash/restart safety rule, verified end-to-end through save/load (not
# just the pure-logic unit test): a job persisted as RUNNING must never
# reload as stuck or silently lost.
win.queue_jobs[0].status = _eq.STATUS_RUNNING
_eq.save_queue(win.queue_jobs)
reloaded_after_crash = _eq.load_queue()
assert reloaded_after_crash[0].status == _eq.STATUS_PENDING
print("Queue: a RUNNING job persisted at crash time reloads as PENDING, not lost or stuck: PASS", flush=True)

_eq._settings().remove(_eq._QUEUE_KEY)
win.queue_jobs = []
win._refresh_queue_list()

# --- Make My Theme wizard --------------------------------------------------
# Never .exec()'d, same reasoning as NewProjectWizard above -- driven
# directly and its own accept-triggering method (Finish -> self.accept())
# is called without the real modal loop.
from mivf_gui.theme_wizard import ThemeWizard, QUICK_PRESETS  # noqa: E402

wiz = ThemeWizard(win)
assert wiz.stack.currentIndex() == 0
wiz._apply_preset(QUICK_PRESETS[2][1])  # "Violet"
assert (wiz.accent_color.red(), wiz.accent_color.green(), wiz.accent_color.blue()) == QUICK_PRESETS[2][1]
wiz._go_next()
assert wiz.stack.currentIndex() == 1
wiz.back_fill_checkbox.setChecked(True)
assert wiz.back_fill_enabled is True
wiz.background_edit.setText(str(fake_source))  # any existing file stands in for a real image here
wiz._go_next()
assert wiz.stack.currentIndex() == 2
assert "rgb(150,80,220)" in wiz.summary_label.text()
wiz.accept()
assert wiz.result() == _QDialog.Accepted
print("Theme wizard: preset selection, optional back-fill, and summary page render correctly: PASS", flush=True)

win.accent_swatch = QColor(1, 1, 1)  # stale marker, must be overwritten below
win.back_fill_enabled = False
win._apply_theme_wizard_result(wiz)
assert (win.accent_swatch.red(), win.accent_swatch.green(), win.accent_swatch.blue()) == QUICK_PRESETS[2][1]
assert win.back_fill_enabled is True
assert win.bg_row.path() == str(fake_source)
assert win.bg_mode_combo.currentData() == "custom"
assert win.centralWidget().currentIndex() == 1
print("MainWindow applies a finished theme wizard's result and lands on the Artwork && Theme tab: PASS", flush=True)

# --- Chapter Authoring Studio ----------------------------------------------
# Never .exec()'d, same reasoning as the other dialogs above. Also never
# triggers _import_from_source (it calls probe_media_cached on a real
# source; fine to exercise, but the "no source set" branch pops a real
# QMessageBox.information -- avoided the same way the other modal-hang
# risks above were).
from mivf_gui.chapter_authoring import ChapterAuthoringDialog, ChapterMark  # noqa: E402

win.project.chapters = []
chap_dlg = ChapterAuthoringDialog(win.project, win)
assert chap_dlg.list_widget.count() == 0

chap_dlg.time_edit.setText("1:00")  # parsed as M:S -> 60 seconds -> displays "0:01:00"
chap_dlg.label_edit.setText("Intro")
chap_dlg._add_or_update()
chap_dlg.time_edit.setText("0:10")  # parsed as M:S -> 10 seconds -> displays "0:00:10"
chap_dlg.label_edit.setText("Cold Open")
chap_dlg._add_or_update()
assert chap_dlg.list_widget.count() == 2
# Always time-sorted for display, regardless of authoring order.
assert chap_dlg.list_widget.item(0).text().startswith("0:00:10")
assert chap_dlg.list_widget.item(1).text().startswith("0:01:00")
print("Chapter Authoring Studio: Add / Update inserts chapters in time-sorted order: PASS", flush=True)

chap_dlg.list_widget.setCurrentRow(0)
chap_dlg._remove_selected()
assert chap_dlg.list_widget.count() == 1
assert chap_dlg.list_widget.item(0).text().startswith("0:01:00")
print("Chapter Authoring Studio: Remove Selected deletes the right chapter: PASS", flush=True)

chap_dlg._accept()
assert chap_dlg.result() == _QDialog.Accepted
assert win.project.chapters == [{"seconds": 60.0, "label": "Intro"}]
print("Chapter Authoring Studio: OK commits edits to project.chapters: PASS", flush=True)

# Cancel discipline: a second dialog's edits must never leak back to the
# project if the user cancels instead of clicking OK.
chap_dlg2 = ChapterAuthoringDialog(win.project, win)
chap_dlg2.time_edit.setText("5:00")
chap_dlg2.label_edit.setText("Should not persist")
chap_dlg2._add_or_update()
chap_dlg2.reject()
assert win.project.chapters == [{"seconds": 60.0, "label": "Intro"}]
print("Chapter Authoring Studio: Cancel discards edits (project untouched): PASS", flush=True)

from mivf_gui.chapter_authoring import write_chapters_sidecar  # noqa: E402
exported_path = write_chapters_sidecar(tmpdir / "chapter_export_test.mivf", [ChapterMark.from_dict(c) for c in win.project.chapters])
assert exported_path.exists()
assert exported_path.read_text(encoding="utf-8") == "60.000|Intro\n"
print("Chapter Authoring Studio: exported sidecar matches the player's expected format: PASS", flush=True)

win.project.chapters = []

# --- Reusable Player/Theme Preview Tour ------------------------------------
# run_theme_preview_tour() itself calls dialog.exec() (a real modal loop),
# so it's never called directly here -- PreviewTourDialog is constructed
# and driven the same way every other dialog above is, and its own
# accept()/done() path (not .exec()) is used to verify the restore-on-
# close behavior run_theme_preview_tour relies on.
from mivf_gui.preview_tour import PreviewTourDialog, TourStep, build_theme_preview_tour  # noqa: E402

calls = []
steps = [
    TourStep("First", "First caption", lambda: calls.append("first")),
    TourStep("Second", "Second caption", lambda: calls.append("second")),
]
tour_dlg = PreviewTourDialog(steps, win)
assert calls == ["first"]  # constructing the dialog applies step 0 immediately
assert tour_dlg.title_label.text() == "First"
assert not tour_dlg.back_btn.isEnabled()
assert tour_dlg.next_btn.isEnabled()

tour_dlg._go_next()
assert calls == ["first", "second"]
assert tour_dlg.title_label.text() == "Second"
assert tour_dlg.back_btn.isEnabled()
assert not tour_dlg.next_btn.isEnabled()

tour_dlg._go_back()
assert calls == ["first", "second", "first"]
assert tour_dlg.title_label.text() == "First"
print("Preview Tour: generic step sequencer applies each step and gates Back/Next at the ends: PASS", flush=True)

theme_steps = build_theme_preview_tour(win)
assert len(theme_steps) == 5
original_focus = win.preview.focused_index
original_back = win.back_preview.focused
win._set_preview_focus_index(1)
win._refresh_preview()

restored = []
theme_dlg = PreviewTourDialog(theme_steps, win, on_close=lambda: restored.append(True))
assert win.preview.focused_index == 0  # step 0 ("Rewind -- focused") applied on open
theme_dlg._go_next()
theme_dlg._go_next()
theme_dlg._go_next()
assert win.back_preview.focused is True  # step 3 ("Back -- focused")
theme_dlg.accept()
assert restored == [True]
print("Preview Tour: theme tour drives the real Preview tab's focus states through all 5 stops: PASS", flush=True)

win._set_preview_focus_index(original_focus)
win._set_back_focus(original_back)
win._refresh_preview()

# --- Local Theme Browser ----------------------------------------------------
# Never .exec()'d, same reasoning as the other dialogs above.
from mivf_gui.theme_browser import ThemeBrowserDialog  # noqa: E402

theme_pkg_dir = tmpdir / "exported_themes"
theme_pkg_dir.mkdir()
(theme_pkg_dir / "mytheme.mivftheme").write_text("PALETTE_ACCENT=11,22,33\nPALETTE_OUTLINE=44,55,66\n")

browser_dlg = ThemeBrowserDialog(win.project, win)
browser_dlg.folder_edit.setText(str(theme_pkg_dir))
browser_dlg._scan()
assert len(browser_dlg.packages) == 1
assert browser_dlg.list_widget.count() == 1

browser_dlg.list_widget.setCurrentRow(0)
assert "11, 22, 33" in browser_dlg.detail_label.text()
print("Theme Browser: scanning a folder finds the package and previews its colors: PASS", flush=True)

stale_accent = QColor(1, 1, 1)
win.accent_swatch = stale_accent
browser_dlg._apply_selected()
assert browser_dlg.applied is True
assert browser_dlg.result() == _QDialog.Accepted
assert win.project.theme.accent_rgb == (11, 22, 33)
win._apply_project_theme_to_swatches()
assert (win.accent_swatch.red(), win.accent_swatch.green(), win.accent_swatch.blue()) == (11, 22, 33)
print("Theme Browser: Apply writes colors to the project and MainWindow's swatches pick them up: PASS", flush=True)

empty_dlg = ThemeBrowserDialog(win.project, win)
empty_dlg.folder_edit.setText(str(tmpdir))  # no .mivftheme files here
empty_dlg._scan()
assert empty_dlg.packages == []
assert empty_dlg.list_widget.count() == 1  # the "(no packages found)" placeholder row
print("Theme Browser: an empty scan shows a placeholder instead of a blank list: PASS", flush=True)

print("OFFSCREEN SMOKE TEST: ALL ASSERTIONS PASSED", flush=True)
