"""PySide6 vertical-slice main window.

Implements mivf_customization_gui_20260716/ENCODER_GUI_VERTICAL_SLICE_PLAN.md's
flow, extended in Phase C.1 with a real image-conversion preview (see
asset_pipeline.py's docstring for the root-cause this replaces) and Back
(MIVF_CTRL_MOVIE_MENU_BACK) support. Not a complete authoring suite -- see
that document's explicit non-goals (no batch encoding, no video trimming
beyond existing CLI flags, no asset marketplace, no full IDE-style
tree/inspector layout -- an enhanced tabbed layout was chosen instead to
keep this bounded, per the task's own "do not build a Photoshop clone").
"""
from __future__ import annotations
from .theme_export_dialog import ThemeExportDialog
from .preflight_dialog import PreflightDialog
from .movie_info_dialog import MovieInfoDialog
from .storage_plan_dialog import StoragePlanDialog
from .storage_plan import build_storage_plan
from .control_editor import ControlArtworkDialog, LEG as CONTROL_RECIPE_LEG
from . import control_recipe, background_recipe
from .background_editor import BackgroundRecipeDialog
from .project_home import (
    ProjectHomeDialog, NewProjectWizard, add_recent_project, should_show_at_startup,
)
from . import encode_queue
from .theme_wizard import ThemeWizard
from .chapter_authoring import ChapterAuthoringDialog
from .preview_tour import run_theme_preview_tour
from .theme_browser import ThemeBrowserDialog

import sys
import os
import copy
import time
from pathlib import Path

from PySide6.QtCore import Qt, QTimer, QUrl, QThread, Signal
from PySide6.QtGui import QColor, QPainter, QPixmap, QPainterPath, QDesktopServices
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QFormLayout,
    QLabel, QLineEdit, QPushButton, QComboBox, QFileDialog, QTextEdit,
    QGroupBox, QColorDialog, QMessageBox, QTabWidget, QPlainTextEdit,
    QScrollArea, QSizePolicy, QDialog, QProgressBar, QListWidget, QListWidgetItem,
)

from PIL import Image

from .project import MivfProject, ProjectArtwork, ProjectTheme
from .presets import PRESET_NAMES, PRESET_LABELS, PRESET_DESCRIPTIONS
from .backend import build_argv, format_command_preview, EncodeRun, EncodeProgress, verify_output
from .asset_pipeline import process_asset, pil_to_qpixmap, FitMode, AssetState, AssetResult, clear_cache
from .media_probe import MediaProbeError, probe_media_cached

REPO_ROOT = Path(__file__).resolve().parents[3]
ENCODER_SCRIPT = REPO_ROOT / "encode_mivf.py"

# Real dimensions, cited from source: g_mivf_touch_layouts[CINEMATIC]
# (main.c:2332) for FF/PP, mivf_menu_draw_button_top's ROW_W/ROW_H
# (main.c:13416) for Back.
DASHBOARD_BG_DIMS = (320, 240)
FF_DIMS = (64, 60)
PP_DIMS = (74, 78)
BACK_DIMS = (222, 20)
REWIND_DIMS = (64, 60)  # main.c:2332, g_mivf_touch_layouts[CINEMATIC].main[0], same size as Fast Forward
COVER_DIMS = (88, 50)   # real .cover format: headerless RGB565LE, exactly 8,800 bytes (mivf_make_cover.py)

STATE_ICON = {
    AssetState.EMPTY: "○",     # ○ -- built-in fallback, not an error
    AssetState.READY: "✓",     # check
    AssetState.WARNING: "⚠",   # warning triangle
    AssetState.ERROR: "✖",     # cross
    AssetState.MISSING: "✖",
}
STATE_COLOR = {
    AssetState.EMPTY: "#888",
    AssetState.READY: "#2a8a3f",
    AssetState.WARNING: "#b8860b",
    AssetState.ERROR: "#b02020",
    AssetState.MISSING: "#b02020",
}


class AssetStatusRow(QWidget):
    """One editable asset slot: path field + Browse + fit-mode selector +
    a live status line, all reacting automatically to any change -- no
    manual "refresh" step required for normal use."""

    def __init__(self, w: int, h: int, has_mask: bool, on_change, parent=None):
        super().__init__(parent)
        self._w, self._h, self._has_mask = w, h, has_mask
        self._on_change = on_change

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        row = QHBoxLayout()
        self.path_edit = QLineEdit()
        self.path_edit.editingFinished.connect(self._reprocess)
        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse)
        clear_btn = QPushButton("Clear")
        clear_btn.clicked.connect(self._clear)
        row.addWidget(self.path_edit)
        row.addWidget(browse_btn)
        row.addWidget(clear_btn)
        layout.addLayout(row)

        opts_row = QHBoxLayout()
        self.fit_combo = QComboBox()
        for mode in FitMode:
            self.fit_combo.addItem(mode.value.replace("_", " ").title(), mode)
        self.fit_combo.currentIndexChanged.connect(self._reprocess)
        opts_row.addWidget(QLabel("Fit:"))
        opts_row.addWidget(self.fit_combo)
        opts_row.addStretch(1)
        layout.addLayout(opts_row)

        self.status_label = QLabel()
        self.status_label.setWordWrap(True)
        layout.addWidget(self.status_label)

        self._debounce = QTimer(self)
        self._debounce.setSingleShot(True)
        self._debounce.setInterval(250)
        self._debounce.timeout.connect(self._reprocess)
        self.path_edit.textChanged.connect(lambda _: self._debounce.start())

        self._result = None
        self._reprocess()

    def _browse(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select image", "", "Images (*.png *.jpg *.jpeg)")
        if path:
            self.path_edit.setText(path)
            self._reprocess()

    def _clear(self):
        self.path_edit.clear()
        self._reprocess()

    def fit_mode(self) -> FitMode:
        # QComboBox's userData round-trip collapses a str-subclassed Enum
        # (FitMode) back to a plain str on some PySide6 builds -- always
        # re-wrap explicitly rather than trust the retrieved type.
        return FitMode(self.fit_combo.currentData())

    def path(self) -> str:
        return self.path_edit.text().strip()

    def result(self):
        return self._result

    def _reprocess(self):
        self._result = process_asset(self.path() or None, self._w, self._h, self._has_mask, self.fit_mode())
        icon = STATE_ICON[self._result.state]
        color = STATE_COLOR[self._result.state]
        self.status_label.setText(f'<span style="color:{color}">{icon}</span> {self._result.message}')
        if self._on_change:
            self._on_change()


class CoverArtworkCard(QWidget):
    """Full parity treatment for the Library cover, matching the button-face
    assets' UX: real thumbnail, source metadata, expected runtime spec,
    live conversion status, fit mode, Replace/Clear/Reset/Reveal-in-folder.

    Real runtime format (mivf_make_cover.py): 88x50, headerless RGB565LE,
    exactly 8,800 bytes -- NOT an MVCA asset (no 12-byte header, unlike the
    button-face controls), and its real default fit uses a 2px safe margin.
    Both facts were confirmed by a direct SHA-256 byte comparison against
    the real tool's own output, not assumed from a matching byte count."""

    def __init__(self, on_change, parent=None):
        super().__init__(parent)
        self._on_change = on_change
        self._result = None
        w, h = COVER_DIMS

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        top_row = QHBoxLayout()
        self.thumb_label = QLabel()
        self.thumb_label.setFixedSize(w, h)
        self.thumb_label.setStyleSheet("background:#111; border:1px solid #333;")
        self.thumb_label.setAlignment(Qt.AlignCenter)
        top_row.addWidget(self.thumb_label)

        info_col = QVBoxLayout()
        self.path_edit = QLineEdit()
        self.path_edit.editingFinished.connect(self._reprocess)
        browse_btn = QPushButton("Replace...")
        browse_btn.clicked.connect(self._browse)
        clear_btn = QPushButton("Clear")
        clear_btn.clicked.connect(self._clear)
        reset_btn = QPushButton("Reset")
        reset_btn.setToolTip("No separate built-in cover exists to reset to -- clears to the Library's default no-cover appearance, same as Clear.")
        reset_btn.clicked.connect(self._clear)
        reveal_btn = QPushButton("Reveal in folder")
        reveal_btn.clicked.connect(self._reveal)

        path_row = QHBoxLayout()
        path_row.addWidget(self.path_edit)
        info_col.addLayout(path_row)

        btn_row = QHBoxLayout()
        btn_row.addWidget(browse_btn)
        btn_row.addWidget(clear_btn)
        btn_row.addWidget(reset_btn)
        btn_row.addWidget(reveal_btn)
        info_col.addLayout(btn_row)

        fit_row = QHBoxLayout()
        self.fit_combo = QComboBox()
        for mode in FitMode:
            self.fit_combo.addItem(mode.value.replace("_", " ").title(), mode)
        self.fit_combo.currentIndexChanged.connect(self._reprocess)
        fit_row.addWidget(QLabel("Fit:"))
        fit_row.addWidget(self.fit_combo)
        fit_row.addStretch(1)
        info_col.addLayout(fit_row)

        top_row.addLayout(info_col, 1)
        layout.addLayout(top_row)

        self.status_label = QLabel()
        self.status_label.setWordWrap(True)
        layout.addWidget(self.status_label)

        self._debounce = QTimer(self)
        self._debounce.setSingleShot(True)
        self._debounce.setInterval(250)
        self._debounce.timeout.connect(self._reprocess)
        self.path_edit.textChanged.connect(lambda _: self._debounce.start())

        self._reprocess()

    def path(self) -> str:
        return self.path_edit.text().strip()

    def fit_mode(self) -> FitMode:
        return FitMode(self.fit_combo.currentData())

    def result(self):
        return self._result

    def _browse(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select cover image", "", "Images (*.png *.jpg *.jpeg)")
        if path:
            self.path_edit.setText(path)
            self._reprocess()

    def _clear(self):
        self.path_edit.clear()
        self._reprocess()

    def _reveal(self):
        p = self.path()
        if not p:
            return
        folder = Path(p).resolve().parent
        if folder.exists():
            QDesktopServices.openUrl(QUrl.fromLocalFile(str(folder)))

    def _reprocess(self):
        w, h = COVER_DIMS
        self._result = process_asset(
            self.path() or None, w, h, has_mask=False,
            fit_mode=self.fit_mode(), header_bytes=0, format_label=".cover", margin=2,
        )
        r = self._result
        if r.prepared_image is not None:
            self.thumb_label.setPixmap(pil_to_qpixmap(r.prepared_image))
        else:
            self.thumb_label.setPixmap(QPixmap())
            self.thumb_label.setText("No cover")

        icon = STATE_ICON[r.state]
        color = STATE_COLOR[r.state]
        details = ""
        if r.state not in (AssetState.EMPTY, AssetState.MISSING, AssetState.ERROR):
            details = (
                f"<br>Source: {r.source_w}x{r.source_h} {r.source_format}, {r.source_bytes} bytes<br>"
                f"Expected runtime: 88x50, headerless RGB565LE, exactly 8,800 bytes"
            )
        self.status_label.setText(f'<span style="color:{color}">{icon}</span> {r.message}{details}')
        if self._on_change:
            self._on_change()


class AssetInspector(QWidget):
    """Source / Prepared / Converted-RGB565 / Mask viewer for whichever
    asset slot is currently selected -- real decoded pixels in every mode,
    never a schematic approximation."""

    MODES = ["Source Artwork", "Prepared Canvas", "Converted 3DS Asset", "Mask"]

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)

        top_row = QHBoxLayout()
        self.element_combo = QComboBox()
        self.mode_combo = QComboBox()
        self.mode_combo.addItems(self.MODES)
        self.mode_combo.currentIndexChanged.connect(self._refresh)
        top_row.addWidget(QLabel("Element:"))
        top_row.addWidget(self.element_combo)
        top_row.addWidget(QLabel("View:"))
        top_row.addWidget(self.mode_combo)
        layout.addLayout(top_row)

        self.image_label = QLabel("No image selected")
        self.image_label.setAlignment(Qt.AlignCenter)
        self.image_label.setMinimumHeight(140)
        self.image_label.setStyleSheet("background:#111; color:#888; border:1px solid #333;")
        layout.addWidget(self.image_label)

        self.info_label = QLabel()
        self.info_label.setWordWrap(True)
        layout.addWidget(self.info_label)

        self._slots: dict[str, callable] = {}  # name -> () -> AssetResult

    def register_slot(self, name: str, result_getter):
        self._slots[name] = result_getter
        self.element_combo.addItem(name)
        self.element_combo.currentIndexChanged.connect(self._refresh)

    def _refresh(self):
        name = self.element_combo.currentText()
        getter = self._slots.get(name)
        result = getter() if getter else None
        if result is None:
            self.image_label.setText("No image selected")
            self.image_label.setPixmap(QPixmap())
            self.info_label.setText("")
            return

        mode = self.mode_combo.currentText()
        image = None
        if mode == "Source Artwork":
            image = result.source_image
        elif mode == "Prepared Canvas":
            image = result.prepared_image
        elif mode == "Converted 3DS Asset":
            image = result.runtime_image
        elif mode == "Mask":
            image = result.mask_image

        if image is None:
            self.image_label.setText(
                "(no mask -- this asset is drawn fully opaque)" if mode == "Mask" and result.state != AssetState.EMPTY
                else "No image selected"
            )
            self.image_label.setPixmap(QPixmap())
        else:
            pixmap = pil_to_qpixmap(image)
            scaled = pixmap.scaled(300, 220, Qt.KeepAspectRatio, Qt.FastTransformation)
            self.image_label.setPixmap(scaled)
            self.image_label.setText("")

        icon = STATE_ICON.get(result.state, "")
        color = STATE_COLOR.get(result.state, "#888")
        info = (
            f'<span style="color:{color}">{icon} {result.state.value.upper()}</span><br>'
            f"Source: {result.source_w}x{result.source_h} {result.source_format}, {result.source_bytes} bytes, "
            f"alpha={'yes' if result.has_alpha else 'no'}<br>"
            f"Runtime: {result.runtime_w}x{result.runtime_h}, {result.runtime_bytes} bytes"
        )
        if result.mask_byte_count:
            info += f", mask {result.mask_byte_count} bytes ({result.visible_px} visible / {result.transparent_px} transparent px)"
        info += f"<br>{result.message}"
        self.info_label.setText(info)


class PremierePreviewWidget(QWidget):
    """Composited "desktop approximation" of the Premiere dashboard --
    per CUSTOMIZATION_VERTICAL_SLICE_PLAN.md this is explicitly NOT claimed
    to be pixel-perfect against the real 3DS renderer until verified
    against an emulator/hardware reference. Matches the real, corrected
    mivf_c25_premiere_controls() layer order: a colored ring first, then
    the image itself (circularly clipped so a rectangular hitbox-sized
    asset reads as a round button face, not a square poking out from
    behind an opaque disc -- the original layering bug), then the built-in
    icon area on top. The real vector icon code stays in main.c; redrawing
    it a third time here was rejected as scope creep, so this mockup
    represents "icon on top" as a small dark disc, not the actual glyph."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(320, 240)
        self.accent = QColor(70, 120, 210)
        self.outline = QColor(255, 255, 255)
        self.focused_index = 1  # 0=rewind 1=play/pause 2=forward
        self.bg_pixmap: QPixmap | None = None
        self.ff_pixmap: QPixmap | None = None
        self.pp_pixmap: QPixmap | None = None
        self.rewind_pixmap: QPixmap | None = None

    def paintEvent(self, event):  # noqa: N802 (Qt override)
        p = QPainter(self)
        if self.bg_pixmap and not self.bg_pixmap.isNull():
            p.drawPixmap(0, 0, 320, 240, self.bg_pixmap)
        else:
            p.fillRect(self.rect(), QColor(2, 4, 10))
        p.setPen(QColor(200, 180, 125))
        p.drawText(18, 24, "NOW PLAYING")
        p.setPen(QColor(245, 248, 252))
        p.drawText(18, 44, "PREMIERE (desktop approximation)")

        xs = [66, 160, 254]
        y = 132
        radii = [27, 37, 27]
        underlays = [self.rewind_pixmap, self.pp_pixmap, self.ff_pixmap]
        for i, (x, r, underlay) in enumerate(zip(xs, radii, underlays)):
            on = i == self.focused_index
            fill = self.accent if on else QColor(24, 31, 43)
            has_image = underlay and not underlay.isNull()
            if has_image:
                # Ring first (the fill color the disc would have used, or the
                # explicit outline color), then the image on top, clipped to
                # a circle so it reads as a round button face -- not the
                # original bug where an opaque disc was drawn ON TOP of the
                # image, hiding everything but its square corners.
                p.setBrush(self.outline if self.outline != QColor(255, 255, 255) else fill)
                p.setPen(Qt.NoPen)
                p.drawEllipse(x - r - 3, y - r - 3, (r + 3) * 2, (r + 3) * 2)

                path = QPainterPath()
                path.addEllipse(x - r, y - r, r * 2, r * 2)
                p.save()
                p.setClipPath(path)
                p.drawPixmap(x - underlay.width() // 2, y - underlay.height() // 2, underlay)
                p.restore()
            else:
                p.setBrush(fill)
                p.setPen(Qt.NoPen)
                p.drawEllipse(x - r, y - r, r * 2, r * 2)
            # icon-area mockup, always on top (real vector icon stays in main.c)
            p.setBrush(QColor(0, 0, 0, 90) if has_image else Qt.NoBrush)
            p.setPen(Qt.NoPen)
            if has_image:
                p.drawEllipse(x - r // 3, y - r // 3, (r * 2) // 3, (r * 2) // 3)
        p.end()

    def set_state(self, accent: QColor, outline: QColor, bg_pixmap, ff_pixmap, pp_pixmap, rewind_pixmap=None):
        self.accent = accent
        self.outline = outline
        self.bg_pixmap = bg_pixmap
        self.ff_pixmap = ff_pixmap
        self.pp_pixmap = pp_pixmap
        self.rewind_pixmap = rewind_pixmap
        self.update()


class MovieMenuBackPreviewWidget(QWidget):
    """Desktop approximation of the root DVD-menu's Back row -- the real
    rendered rectangle (222x20, mivf_menu_draw_button_top()'s ROW_W/ROW_H,
    main.c:13416), shown against a plain dark top-screen backdrop since
    the real menu's own background isn't part of this customization slice."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(300, 80)
        self.underlay: QPixmap | None = None
        self.fill = QColor(200, 60, 60)
        self.has_fill = False
        self.focused = False

    def paintEvent(self, event):  # noqa: N802
        p = QPainter(self)
        p.fillRect(self.rect(), QColor(5, 8, 14))
        cx, cy = 150, 40
        w, h = 222, 20
        x, y = cx - w // 2, cy - h // 2
        if self.underlay and not self.underlay.isNull():
            p.drawPixmap(cx - self.underlay.width() // 2, cy - self.underlay.height() // 2, self.underlay)
        if self.has_fill and not self.focused:
            fill = QColor(self.fill)
            fill.setAlpha(150)
            p.fillRect(x, y, w, h, fill)
        elif self.focused:
            p.fillRect(x, y, w, h, QColor(70, 120, 210, 160))
        p.setPen(QColor(255, 255, 255) if self.focused else QColor(194, 208, 225))
        p.drawText(x + 10, cy + 4, "BACK")
        p.end()

    def set_state(self, underlay, fill: QColor, has_fill: bool, focused: bool):
        self.underlay = underlay
        self.fill = fill
        self.has_fill = has_fill
        self.focused = focused
        self.update()




class _EncodeSourceProbeWorker(QThread):
    """Obtains a frame total without freezing the Qt event loop.

    A reported ``nb_frames`` value is preferred.  Duration multiplied by the
    exact rational average frame rate is an explicitly approximate fallback.
    Failure never blocks encoding; it merely leaves progress indeterminate.
    """

    succeeded = Signal(object, str)
    failed = Signal(str)

    def __init__(self, source: Path, parent=None):
        super().__init__(parent)
        self.source = source

    def run(self):
        try:
            probe = probe_media_cached(self.source, compute_hash=False)
            video = next((v for v in probe.video_streams if not v.is_attached_pic), None)
            if video is None:
                self.succeeded.emit(None, "No ordinary video stream was reported; progress will be indeterminate.")
                return
            if video.frame_count and video.frame_count > 0:
                self.succeeded.emit(video.frame_count, "Frame total reported by the source container.")
                return
            if (probe.duration_seconds and video.avg_frame_rate_num and video.avg_frame_rate_den):
                estimated = round(probe.duration_seconds * video.avg_frame_rate_num / video.avg_frame_rate_den)
                self.succeeded.emit(estimated if estimated > 0 else None, "Frame total estimated from duration and rational frame rate.")
                return
            self.succeeded.emit(None, "Source frame total is unavailable; progress will be indeterminate.")
        except (MediaProbeError, OSError) as exc:
            self.failed.emit(str(exc))


def _stream_identity(probe) -> dict:
    i = probe.identity
    return {"canonical_path": i.canonical_path, "size_bytes": i.size_bytes, "mtime_ns": i.mtime_ns}

def _stream_label(kind: str, stream) -> str:
    language = stream.language or "language unknown"
    title = stream.title or "untitled"
    default = " · default" if stream.is_default else ""
    if kind == "video":
        dimensions = f"{stream.width}x{stream.height}" if stream.width and stream.height else "dimensions unknown"
        detail = f"{stream.codec} · {dimensions}"
    elif kind == "audio":
        channels = f"{stream.channels}ch" if stream.channels else "channels unknown"
        rate = f"{stream.sample_rate}Hz" if stream.sample_rate else "rate unknown"
        detail = f"{stream.codec} · {channels} · {rate}"
    else:  # subtitle
        kind_note = {"text": "text", "bitmap": "bitmap — unsupported", "unknown": "unsupported codec"}.get(stream.kind, stream.kind)
        detail = f"{stream.codec} ({kind_note})"
        if stream.is_forced:
            default += " · forced"
        if stream.is_hearing_impaired:
            default += " · hearing-impaired"
    return f"#{stream.stream_index} — {language} — {title} — {detail}{default}"


class MainWindow(QMainWindow):

    def _open_dashboard_canvas(self):
        self._sync_project_from_fields()
        from .dashboard_canvas import DashboardCanvasDialog
        DashboardCanvasDialog(self.project, self).exec()

    def _open_control_studio(self):
        self._sync_project_from_fields()
        if ControlArtworkDialog(self.project,self).exec()==QDialog.Accepted:self._refresh_preview()

    def _open_theme_export_dialog(self):
        """Open the transactional runtime-theme package exporter."""
        project_path = ""
        for name in ("project_path", "current_project_path", "_project_path"):
            value = getattr(self, name, "")
            if value:
                project_path = str(value)
                break
        dialog = ThemeExportDialog(self, project_path)
        dialog.exec()

    def _open_preflight_dialog(self):
        """Phase C.5a: Check Project / Export Dry Run / Change Summary --
        never writes anything; safe to run at any time, saved project or not
        (an unsaved project simply has no file path to check yet)."""
        self._sync_project_from_fields()
        if not self.project.project_path:
            QMessageBox.information(self, "Save project first",
                                     "Save this project as a .mivfproj file before running Check Project.")
            return
        # Check Project reads the project FILE (same as the real exporter),
        # so re-save the current in-editor state to that same path first --
        # otherwise a check right after an unsaved edit would silently
        # inspect stale, previously-saved content.
        self.project.save(self.project.project_path)
        dialog = PreflightDialog(self, str(self.project.project_path))
        dialog.exec()

    def _open_movie_info_dialog(self):
        """E.1.2: Movie Information Authoring. Desktop-only working-copy
        editor (see MovieInfoDialog docstring); safe to open regardless of
        whether the project is saved -- unlike Check Project, this never
        needs to read the project back off disk."""
        self._sync_project_from_fields()
        MovieInfoDialog(self.project, self).exec()

    def _open_storage_plan_dialog(self):
        """E.3.1: Device Storage Planner. Read-only report; safe to open
        regardless of whether the project is saved (an unsaved project
        just has fewer known-exact figures -- see StoragePlan's UNKNOWN
        degradation, never a crash)."""
        self._sync_project_from_fields()
        StoragePlanDialog(self.project, self).exec()

    def __init__(self):
        super().__init__()
        project_home_action = self.menuBar().addAction("Project Home...")
        project_home_action.triggered.connect(self._show_project_home)
        theme_wizard_action = self.menuBar().addAction("Make My Theme...")
        theme_wizard_action.triggered.connect(self._run_theme_wizard)
        chapters_action = self.menuBar().addAction("Chapter Authoring Studio...")
        chapters_action.triggered.connect(self._open_chapter_authoring_dialog)
        preview_tour_action = self.menuBar().addAction("Preview Tour...")
        preview_tour_action.triggered.connect(self._run_preview_tour)
        theme_browser_action = self.menuBar().addAction("Local Theme Browser...")
        theme_browser_action.triggered.connect(self._open_theme_browser)
        export_theme_action = self.menuBar().addAction("Export Runtime Theme Package...")
        export_theme_action.triggered.connect(self._open_theme_export_dialog)
        preflight_action = self.menuBar().addAction("Check Project...")
        preflight_action.triggered.connect(self._open_preflight_dialog)
        movie_info_action = self.menuBar().addAction("Movie Information...")
        movie_info_action.triggered.connect(self._open_movie_info_dialog)
        storage_plan_action = self.menuBar().addAction("Storage Planner...")
        storage_plan_action.triggered.connect(self._open_storage_plan_dialog)
        self.setWindowTitle("MIVF Toolkit (Phase C.1)")
        self.project = MivfProject()
        self.encode_run: EncodeRun | None = None
        self.encode_probe_worker: _EncodeSourceProbeWorker | None = None
        self.encode_elapsed_timer = QTimer(self)
        self.encode_elapsed_timer.timeout.connect(self._refresh_elapsed_label)
        self.encode_started_monotonic: float | None = None
        self._last_progress: EncodeProgress | None = None
        self._encode_cancel_requested = False
        self.poll_timer = QTimer(self)
        self.poll_timer.timeout.connect(self._poll_encode)

        # D: Persistent multi-project encode queue -- entirely separate
        # state from the single-project Run tab above (own EncodeRun, own
        # timer, own probe worker) so queue processing can never cross-wire
        # with an ad-hoc single-project run or vice versa.
        self.queue_jobs_root = Path.home() / ".mivf_toolkit" / "queue_jobs"
        self.queue_jobs: list[encode_queue.QueueJob] = encode_queue.load_queue()
        self.queue_running = False
        self.queue_stop_requested = False
        self.queue_active_job: encode_queue.QueueJob | None = None
        self.queue_encode_run: EncodeRun | None = None
        self.queue_probe_worker: _EncodeSourceProbeWorker | None = None
        self.queue_started_monotonic: float | None = None
        self.queue_last_progress: EncodeProgress | None = None
        self.queue_poll_timer = QTimer(self)
        self.queue_poll_timer.timeout.connect(self._queue_poll_encode)
        self.queue_elapsed_timer = QTimer(self)
        self.queue_elapsed_timer.timeout.connect(self._refresh_queue_elapsed_label)

        self._build_ui()
        self._refresh_preview()

    # ---- UI construction -------------------------------------------------

    def _build_ui(self):
        tabs = QTabWidget()
        tabs.addTab(self._build_project_tab(), "1. Project")
        tabs.addTab(self._build_artwork_tab(), "2. Artwork && Theme")
        tabs.addTab(self._build_preview_tab(), "3. Preview")
        tabs.addTab(self._build_run_tab(), "4. Review && Run")
        tabs.addTab(self._build_queue_tab(), "5. Queue")
        self.setCentralWidget(tabs)
        self.resize(900, 620)

    def _build_project_tab(self) -> QWidget:
        w = QWidget()
        form = QFormLayout(w)

        self.source_edit = QLineEdit()
        source_btn = QPushButton("Browse...")
        source_btn.clicked.connect(self._pick_source)
        source_row = QHBoxLayout()
        source_row.addWidget(self.source_edit)
        source_row.addWidget(source_btn)
        form.addRow("Source media:", self._wrap(source_row))
        self.video_stream_combo = QComboBox()
        self.video_stream_combo.addItem("Auto — first video stream (legacy default)", None)
        form.addRow("Video stream:", self.video_stream_combo)
        self.audio_stream_combo = QComboBox()
        self.audio_stream_combo.addItem("Auto — first audio stream (legacy default)", None)
        form.addRow("Audio stream:", self.audio_stream_combo)
        self.subtitle_stream_combo = QComboBox()
        self.subtitle_stream_combo.addItem("No subtitle", None)
        self.subtitle_stream_combo.currentIndexChanged.connect(self._update_subtitle_edition_enabled)
        form.addRow("Subtitle stream:", self.subtitle_stream_combo)
        self.subtitle_edition_combo = QComboBox()
        for slot, label in enumerate((
            "Slot 0 — default sidecar (movie.srt)",
            "Slot 1 (movie.1.srt)",
            "Slot 2 (movie.2.srt)",
            "Slot 3 (movie.3.srt)",
        )):
            self.subtitle_edition_combo.addItem(label, slot)
        self.subtitle_edition_combo.setEnabled(False)
        form.addRow("Subtitle edition slot:", self.subtitle_edition_combo)
        subtitle_note = QLabel(
            "The selected subtitle is authored as a separate player sidecar file, not embedded in "
            "the MIVF and not runtime-switchable. Which slot actually displays on the 3DS is controlled "
            "by a global player setting (Settings → Subtitle Track), not a per-movie choice."
        )
        subtitle_note.setWordWrap(True)
        form.addRow("", subtitle_note)
        self.stream_status_label = QLabel("Choose a source to inspect its streams.")
        self.stream_status_label.setWordWrap(True)
        form.addRow("", self.stream_status_label)
        self.output_edit = QLineEdit()
        output_btn = QPushButton("Browse...")
        output_btn.clicked.connect(self._pick_output)
        output_row = QHBoxLayout()
        output_row.addWidget(self.output_edit)
        output_row.addWidget(output_btn)
        form.addRow("Output .mivf:", self._wrap(output_row))

        self.preset_combo = QComboBox()
        for name in PRESET_NAMES:
            self.preset_combo.addItem(PRESET_LABELS[name], name)
        self.preset_combo.currentIndexChanged.connect(self._on_preset_changed)
        form.addRow("Basic preset:", self.preset_combo)

        self.preset_desc_label = QLabel(PRESET_DESCRIPTIONS[PRESET_NAMES[0]])
        self.preset_desc_label.setWordWrap(True)
        form.addRow("", self.preset_desc_label)

        save_row = QHBoxLayout()
        new_btn = QPushButton("New Project")
        open_btn = QPushButton("Open Project...")
        save_btn = QPushButton("Save Project...")
        new_btn.clicked.connect(self._new_project)
        open_btn.clicked.connect(self._open_project)
        save_btn.clicked.connect(self._save_project)
        save_row.addWidget(new_btn)
        save_row.addWidget(open_btn)
        save_row.addWidget(save_btn)
        form.addRow("Project:", self._wrap(save_row))

        return w

    def _build_artwork_tab(self) -> QWidget:
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        w = QWidget()
        layout = QVBoxLayout(w)

        cover_box = QGroupBox("Cover artwork (Library poster)")
        cover_layout = QVBoxLayout(cover_box)
        self.cover_card = CoverArtworkCard(on_change=self._refresh_preview)
        cover_layout.addWidget(self.cover_card)
        layout.addWidget(cover_box)

        # C.6/Screensaver: per-title custom bounce image, .screensaver.cover
        # sidecar (source/main.c's mivf_menu_load_screensaver_image). Reuses
        # process_asset()'s existing headerless-RGB565 path (already
        # anticipates this exact format, see asset_pipeline.py's own header
        # comment) rather than a new preview-card class -- kept intentionally
        # simple (status text, not a live pixmap preview) given this is a
        # small, secondary asset next to the two Toolkit's larger surfaces.
        saver_box = QGroupBox("Screensaver custom image (per-title, optional)")
        saver_form = QFormLayout(saver_box)
        self.screensaver_edit = QLineEdit()
        saver_btn = QPushButton("Browse...")
        saver_btn.clicked.connect(self._pick_screensaver_image)
        saver_row = QHBoxLayout()
        saver_row.addWidget(self.screensaver_edit)
        saver_row.addWidget(saver_btn)
        saver_form.addRow("Source image:", self._wrap(saver_row))
        self.screensaver_status_label = QLabel("No custom screensaver image selected — the built-in mark is used.")
        self.screensaver_status_label.setWordWrap(True)
        saver_form.addRow("", self.screensaver_status_label)
        layout.addWidget(saver_box)

        dash_box = QGroupBox("Premiere dashboard (playback screen)")
        dash_form = QFormLayout(dash_box)
        self.bg_mode_combo = QComboBox()
        self.bg_mode_combo.addItem("Player built-in", "builtin")
        self.bg_mode_combo.addItem("Custom source image", "custom")
        self.bg_mode_combo.addItem("Generated foundation gradient", "generated")
        self.bg_mode_combo.currentIndexChanged.connect(self._background_mode_changed)
        dash_form.addRow("Background mode:", self.bg_mode_combo)
        self.bg_row = AssetStatusRow(*DASHBOARD_BG_DIMS, has_mask=False, on_change=self._background_source_changed)
        dash_form.addRow("Dashboard background source:", self.bg_row)
        self.bg_readability_btn = QPushButton("Background readability...")
        self.bg_readability_btn.clicked.connect(self._open_background_editor)
        dash_form.addRow("Nondestructive adjustments:", self.bg_readability_btn)
        self.rewind_row = AssetStatusRow(*REWIND_DIMS, has_mask=True, on_change=self._refresh_preview)
        dash_form.addRow("Rewind button face image:", self.rewind_row)
        self.ff_row = AssetStatusRow(*FF_DIMS, has_mask=True, on_change=self._refresh_preview)
        dash_form.addRow("Fast Forward button face image:", self.ff_row)
        self.pp_row = AssetStatusRow(*PP_DIMS, has_mask=True, on_change=self._refresh_preview)
        dash_form.addRow("Play/Pause button face image:", self.pp_row)

        self.accent_btn = QPushButton("Choose accent color...")
        self.accent_swatch = QColor(70, 120, 210)
        self.accent_btn.clicked.connect(self._pick_accent)
        dash_form.addRow("Semantic accent:", self.accent_btn)

        self.outline_btn = QPushButton("Choose outline color...")
        self.outline_swatch = QColor(255, 255, 255)
        self.outline_btn.clicked.connect(self._pick_outline)
        dash_form.addRow("Semantic outline:", self.outline_btn)
        studio=QPushButton("Open Control Artwork Studio...");studio.clicked.connect(self._open_control_studio);dash_form.addRow("Idle / Focused editor:",studio)
        canvas_btn=QPushButton("Open Dashboard Canvas...");canvas_btn.clicked.connect(self._open_dashboard_canvas);dash_form.addRow("Control positions (Premiere):",canvas_btn)
        layout.addWidget(dash_box)

        back_box = QGroupBox("Movie Menu — Back row (MIVF_CTRL_MOVIE_MENU_BACK, real & functional)")
        back_form = QFormLayout(back_box)
        note = QLabel(
            "Back is a real, distinct action on the root DVD-style menu (MIVF_MENU_ACTION_BACK) — "
            "not a playback-dashboard button. It always works even with no artwork."
        )
        note.setWordWrap(True)
        back_form.addRow(note)
        self.back_row = AssetStatusRow(*BACK_DIMS, has_mask=True, on_change=self._refresh_preview)
        back_form.addRow("Back button face image:", self.back_row)
        self.back_fill_btn = QPushButton("Choose Back fill color...")
        self.back_fill_swatch = QColor(200, 60, 60)
        self.back_fill_enabled = False
        self.back_fill_btn.clicked.connect(self._pick_back_fill)
        back_form.addRow("Back fill (Idle):", self.back_fill_btn)
        layout.addWidget(back_box)

        layout.addStretch(1)
        scroll.setWidget(w)
        return scroll

    def _build_preview_tab(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)
        label = QLabel("Desktop approximation — not yet verified against emulator/hardware.")
        label.setStyleSheet("color: #b08; font-weight: bold;")
        layout.addWidget(label)

        composite_row = QHBoxLayout()

        dash_col = QVBoxLayout()
        dash_col.addWidget(QLabel("Premiere dashboard"))
        self.preview = PremierePreviewWidget()
        dash_col.addWidget(self.preview)
        dash_col.addWidget(QLabel("Focused control (only one is focused at a time, matching real D-pad navigation):"))
        state_row = QHBoxLayout()
        # Phase C.2: previously a generic "Idle"/"Focused" pair only ever
        # toggled between Play/Pause and Fast Forward -- Rewind's own
        # Focused state was never reachable in this preview at all. Real
        # semantics: exactly one of the three controls is focused at a
        # time (focused_index), so a per-control selector is the correct
        # fix, not a second Idle/Focused pair.
        rewind_focus_btn = QPushButton("Rewind")
        pp_focus_btn = QPushButton("Play/Pause")
        ff_focus_btn = QPushButton("Fast Forward")
        rewind_focus_btn.clicked.connect(lambda: self._set_preview_focus_index(0))
        pp_focus_btn.clicked.connect(lambda: self._set_preview_focus_index(1))
        ff_focus_btn.clicked.connect(lambda: self._set_preview_focus_index(2))
        state_row.addWidget(rewind_focus_btn)
        state_row.addWidget(pp_focus_btn)
        state_row.addWidget(ff_focus_btn)
        dash_col.addLayout(state_row)
        composite_row.addLayout(dash_col)

        back_col = QVBoxLayout()
        back_col.addWidget(QLabel("Movie Menu — Back row"))
        self.back_preview = MovieMenuBackPreviewWidget()
        back_col.addWidget(self.back_preview)
        back_state_row = QHBoxLayout()
        back_idle_btn = QPushButton("Idle")
        back_focused_btn = QPushButton("Focused")
        back_idle_btn.clicked.connect(lambda: self._set_back_focus(False))
        back_focused_btn.clicked.connect(lambda: self._set_back_focus(True))
        back_state_row.addWidget(back_idle_btn)
        back_state_row.addWidget(back_focused_btn)
        back_col.addLayout(back_state_row)
        back_col.addStretch(1)
        composite_row.addLayout(back_col)

        layout.addLayout(composite_row)

        # Phase C.3.1: localized warning for a recipe that failed to render
        # (missing/invalid source, etc.) -- the affected control still falls
        # back to its legacy/built-in appearance; this just says why.
        self.preview_recipe_warning = QLabel("")
        self.preview_recipe_warning.setStyleSheet("color: #c77;")
        self.preview_recipe_warning.setWordWrap(True)
        layout.addWidget(self.preview_recipe_warning)

        inspector_box = QGroupBox("Asset Inspector — real converted pixels, every mode")
        inspector_layout = QVBoxLayout(inspector_box)
        self.inspector = AssetInspector()
        inspector_layout.addWidget(self.inspector)
        layout.addWidget(inspector_box)

        rebuild_btn = QPushButton("Rebuild Preview (troubleshooting only — updates are automatic)")
        rebuild_btn.clicked.connect(self._refresh_preview)
        layout.addWidget(rebuild_btn)

        layout.addStretch(1)
        return w

    def _build_run_tab(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)

        review_btn = QPushButton("Validate && Generate Command")
        review_btn.clicked.connect(self._generate_command)
        layout.addWidget(review_btn)

        self.review_summary = QTextEdit()
        self.review_summary.setReadOnly(True)
        self.review_summary.setMaximumHeight(140)
        layout.addWidget(self.review_summary)

        self.command_preview = QTextEdit()
        self.command_preview.setReadOnly(True)
        self.command_preview.setMaximumHeight(80)
        layout.addWidget(self.command_preview)

        run_row = QHBoxLayout()
        self.run_btn = QPushButton("Create MIVF")
        self.cancel_btn = QPushButton("Cancel")
        self.cancel_btn.setEnabled(False)
        self.run_btn.clicked.connect(self._launch_encode)
        self.cancel_btn.clicked.connect(self._cancel_encode)
        run_row.addWidget(self.run_btn)
        run_row.addWidget(self.cancel_btn)
        layout.addLayout(run_row)

        # Encode-progress widgets. The progress controller methods below
        # update these fields; constructing them here keeps the live run path
        # and the Review & Run tab in one coherent contract.
        self.progress_stage_label = QLabel("Ready")
        self.progress_stage_label.setWordWrap(True)
        layout.addWidget(self.progress_stage_label)

        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)
        layout.addWidget(self.progress_bar)

        self.progress_timing_label = QLabel(
            "Elapsed -- · Speed -- · Remaining --"
        )
        self.progress_timing_label.setWordWrap(True)
        layout.addWidget(self.progress_timing_label)

        self.log_view = QPlainTextEdit()
        self.log_view.setReadOnly(True)
        layout.addWidget(self.log_view)

        self.result_label = QLabel("")
        layout.addWidget(self.result_label)

        return w

    def _build_queue_tab(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)

        intro = QLabel(
            "Queue multiple saved projects to encode one after another. Each "
            "queued job gets its own persistent job directory, so an "
            "interrupted job resumes from where it left off (the same "
            "resumable-jobs mechanism used by a single project's Job "
            "Directory field) rather than restarting from scratch."
        )
        intro.setWordWrap(True)
        layout.addWidget(intro)

        add_row = QHBoxLayout()
        add_btn = QPushButton("Add Current Project to Queue")
        add_btn.clicked.connect(self._add_current_project_to_queue)
        add_row.addWidget(add_btn)
        layout.addLayout(add_row)

        self.queue_list = QListWidget()
        self.queue_list.itemSelectionChanged.connect(self._refresh_queue_button_states)
        layout.addWidget(self.queue_list)

        manage_row = QHBoxLayout()
        self.queue_up_btn = QPushButton("Move Up")
        self.queue_down_btn = QPushButton("Move Down")
        self.queue_remove_btn = QPushButton("Remove Selected")
        self.queue_up_btn.clicked.connect(lambda: self._move_selected_queue_job(-1))
        self.queue_down_btn.clicked.connect(lambda: self._move_selected_queue_job(1))
        self.queue_remove_btn.clicked.connect(self._remove_selected_queue_job)
        manage_row.addWidget(self.queue_up_btn)
        manage_row.addWidget(self.queue_down_btn)
        manage_row.addWidget(self.queue_remove_btn)
        layout.addLayout(manage_row)

        run_row = QHBoxLayout()
        self.queue_start_btn = QPushButton("Start Queue")
        self.queue_stop_btn = QPushButton("Stop After Current")
        self.queue_start_btn.clicked.connect(self._start_queue)
        self.queue_stop_btn.clicked.connect(self._stop_queue_after_current)
        self.queue_stop_btn.setEnabled(False)
        run_row.addWidget(self.queue_start_btn)
        run_row.addWidget(self.queue_stop_btn)
        layout.addLayout(run_row)

        self.queue_status_label = QLabel("Idle")
        self.queue_status_label.setWordWrap(True)
        layout.addWidget(self.queue_status_label)

        self.queue_progress_bar = QProgressBar()
        self.queue_progress_bar.setRange(0, 100)
        self.queue_progress_bar.setValue(0)
        layout.addWidget(self.queue_progress_bar)

        self.queue_timing_label = QLabel("Elapsed -- · Speed -- · Remaining --")
        layout.addWidget(self.queue_timing_label)

        self.queue_log_view = QPlainTextEdit()
        self.queue_log_view.setReadOnly(True)
        self.queue_log_view.setMaximumHeight(140)
        layout.addWidget(self.queue_log_view)

        self._refresh_queue_list()
        return w

    @staticmethod
    def _wrap(layout) -> QWidget:
        w = QWidget()
        w.setLayout(layout)
        return w

    # ---- project actions ---------------------------------------------------

    def _new_project(self):
        self.project = MivfProject()
        self.source_edit.clear()
        self.output_edit.clear()
        self._reset_stream_controls()
        self.cover_card.path_edit.clear()
        for row in (self.bg_row, self.ff_row, self.pp_row, self.back_row, self.rewind_row):
            row.path_edit.clear()
        self.bg_mode_combo.setCurrentIndex(self.bg_mode_combo.findData("builtin"))
        clear_cache()
        self._refresh_preview()

    def _open_project(self):
        path, _ = QFileDialog.getOpenFileName(self, "Open project", "", "MIVF Project (*.mivfproj)")
        if not path:
            return
        self._load_project_file(Path(path))

    def _show_project_home(self):
        dialog = ProjectHomeDialog(self)
        if dialog.exec() != QDialog.Accepted:
            return
        self._handle_project_home_result(dialog.result_action, dialog.recent_path)

    def _handle_project_home_result(self, action: str | None, recent_path: str | None):
        """Split from _show_project_home so tests can exercise the dispatch
        logic directly, without invoking the real modal exec() loop."""
        if action == ProjectHomeDialog.NEW:
            self._run_new_project_wizard()
        elif action == ProjectHomeDialog.OPEN:
            self._open_project()
        elif action == ProjectHomeDialog.RECENT and recent_path:
            self._load_project_file(Path(recent_path))
        # SKIP (or no choice made): leave the editor exactly as it already
        # was -- identical to pre-Phase-2 behavior.

    def _run_new_project_wizard(self):
        wizard = NewProjectWizard(self)
        if wizard.exec() != QDialog.Accepted:
            return
        self._apply_new_project_wizard_result(wizard)

    def _apply_new_project_wizard_result(self, wizard: NewProjectWizard):
        """Split from _run_new_project_wizard so tests can drive a wizard's
        fields and call this directly, without invoking the real modal
        exec() loop (which has no user present to click Finish)."""
        self._new_project()
        self.source_edit.setText(wizard.source_path)
        self.output_edit.setText(wizard.output_path)
        idx = self.preset_combo.findData(wizard.preset_name)
        if idx >= 0:
            self.preset_combo.setCurrentIndex(idx)
        # show_errors=False, matching _open_project's own choice: this is
        # an indirect/automatic population (not a direct Browse click), so
        # a probe failure should surface in the status label the user will
        # see on the Project tab, not as a blocking dialog mid-wizard.
        self._probe_source_streams(show_errors=False)
        self._refresh_preview()
        central = self.centralWidget()
        if isinstance(central, QTabWidget):
            central.setCurrentIndex(0)

    def _run_preview_tour(self):
        run_theme_preview_tour(self)

    def _open_theme_browser(self):
        self._sync_project_from_fields()
        dialog = ThemeBrowserDialog(self.project, self)
        if dialog.exec() == QDialog.Accepted and dialog.applied:
            self._apply_project_theme_to_swatches()
            self._refresh_preview()
            central = self.centralWidget()
            if isinstance(central, QTabWidget):
                central.setCurrentIndex(1)  # "2. Artwork && Theme"

    def _apply_project_theme_to_swatches(self):
        """Re-read self.project.theme into the swatch fields the manual
        controls use -- same mirror _load_project_file already does after
        loading a saved project, reused here so Theme Browser's Apply
        (which mutates project.theme directly) is visible immediately."""
        if self.project.theme.accent_rgb:
            self.accent_swatch = QColor(*self.project.theme.accent_rgb)
        if self.project.theme.outline_rgb:
            self.outline_swatch = QColor(*self.project.theme.outline_rgb)

    def _open_chapter_authoring_dialog(self):
        self._sync_project_from_fields()
        ChapterAuthoringDialog(self.project, self).exec()

    def _run_theme_wizard(self):
        wizard = ThemeWizard(self)
        if wizard.exec() != QDialog.Accepted:
            return
        self._apply_theme_wizard_result(wizard)

    def _apply_theme_wizard_result(self, wizard: ThemeWizard):
        """Split from _run_theme_wizard so tests can drive a wizard's
        fields and call this directly, without invoking the real modal
        exec() loop. Applies through the exact same state the manual
        Artwork && Theme controls already write -- no parallel theme
        representation."""
        self.accent_swatch = wizard.accent_color
        self.outline_swatch = wizard.outline_color
        if wizard.back_fill_enabled:
            self.back_fill_swatch = wizard.back_fill_color
            self.back_fill_enabled = True
        background_path = wizard.background_edit.text().strip()
        if background_path:
            self.bg_row.path_edit.setText(background_path)
            self.bg_row._reprocess()
            idx = self.bg_mode_combo.findData("custom")
            if idx >= 0:
                self.bg_mode_combo.setCurrentIndex(idx)
        self._refresh_preview()
        central = self.centralWidget()
        if isinstance(central, QTabWidget):
            central.setCurrentIndex(1)  # "2. Artwork && Theme"

    def _load_project_file(self, path: Path):
        try:
            self.project = MivfProject.load(path)
        except Exception as e:  # noqa: BLE001 -- surfaced to the user, not swallowed
            QMessageBox.critical(self, "Open project failed", str(e))
            return
        self.source_edit.setText(self.project.source_media or "")
        self._probe_source_streams(show_errors=False)
        self.output_edit.setText(self.project.output_path or "")
        self.cover_card.path_edit.setText(self.project.artwork.cover or "")
        self.screensaver_edit.setText(self.project.artwork.screensaver or "")
        self._refresh_screensaver_status()
        self.cover_card._reprocess()
        fit_modes = self.project.artwork.fit_modes or {}
        for row, artwork_key in (
            (self.bg_row, "dashboard_bg"), (self.ff_row, "fast_forward_underlay"),
            (self.pp_row, "play_pause_underlay"), (self.back_row, "movie_menu_back"),
            (self.rewind_row, "rewind_underlay"),
        ):
            row.path_edit.setText(getattr(self.project.artwork, artwork_key) or "")
            saved_fit = fit_modes.get(artwork_key)
            if saved_fit:
                idx = row.fit_combo.findData(FitMode(saved_fit))
                if idx >= 0:
                    row.fit_combo.setCurrentIndex(idx)
        mode = background_recipe.effective_mode(self.project.artwork)
        self.bg_mode_combo.setCurrentIndex(max(0, self.bg_mode_combo.findData(mode)))
        self._background_mode_changed()
        idx = self.preset_combo.findData(self.project.preset)
        if idx >= 0:
            self.preset_combo.setCurrentIndex(idx)
        if self.project.theme.accent_rgb:
            self.accent_swatch = QColor(*self.project.theme.accent_rgb)
        if self.project.theme.outline_rgb:
            self.outline_swatch = QColor(*self.project.theme.outline_rgb)
        if self.project.theme.back_fill_rgb:
            self.back_fill_swatch = QColor(*self.project.theme.back_fill_rgb)
            self.back_fill_enabled = True
        else:
            self.back_fill_enabled = False
        missing = self.project.missing_files()
        if missing:
            QMessageBox.warning(self, "Project needs relink", "Missing files:\n" + "\n".join(missing))
        self._refresh_preview()
        add_recent_project(str(path))

    def _save_project(self):
        self._sync_project_from_fields()
        path, _ = QFileDialog.getSaveFileName(self, "Save project", "", "MIVF Project (*.mivfproj)")
        if not path:
            return
        self.project.save(Path(path))
        add_recent_project(path)
        QMessageBox.information(self, "Saved", f"Project saved to {path}")

    def _sync_project_from_fields(self):
        self.project.source_media = self.source_edit.text() or None
        self.project.output_path = self.output_edit.text() or None
        self.project.video_stream_index = self.video_stream_combo.currentData()
        self.project.audio_stream_index = self.audio_stream_combo.currentData()
        self.project.subtitle_stream_index = self.subtitle_stream_combo.currentData()
        self.project.subtitle_edition = self.subtitle_edition_combo.currentData() or 0
        self.project.preset = self.preset_combo.currentData()
        self.project.artwork = ProjectArtwork(
            cover=self.cover_card.path() or None,
            screensaver=self.screensaver_edit.text().strip() or None,
            dashboard_bg=self.bg_row.path() or None,
            dashboard_bg_mode=self.bg_mode_combo.currentData(),
            dashboard_bg_recipe=copy.deepcopy(self.project.artwork.dashboard_bg_recipe),
            fast_forward_underlay=self.ff_row.path() or None,
            play_pause_underlay=self.pp_row.path() or None,
            movie_menu_back=self.back_row.path() or None,
            rewind_underlay=self.rewind_row.path() or None,
            control_edits=copy.deepcopy(self.project.artwork.control_edits),
            fit_modes={
                "dashboard_bg": self.bg_row.fit_mode().value,
                "fast_forward_underlay": self.ff_row.fit_mode().value,
                "play_pause_underlay": self.pp_row.fit_mode().value,
                "movie_menu_back": self.back_row.fit_mode().value,
                "rewind_underlay": self.rewind_row.fit_mode().value,
            },
        )
        self.project.theme = ProjectTheme(
            accent_rgb=(self.accent_swatch.red(), self.accent_swatch.green(), self.accent_swatch.blue()),
            outline_rgb=(self.outline_swatch.red(), self.outline_swatch.green(), self.outline_swatch.blue()),
            back_fill_rgb=(self.back_fill_swatch.red(), self.back_fill_swatch.green(), self.back_fill_swatch.blue())
            if self.back_fill_enabled else None,
        )

    def _pick_source(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select source media")
        if path:
            self.source_edit.setText(path)
            self._probe_source_streams(show_errors=True)

    def _pick_screensaver_image(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select screensaver source image")
        if path:
            self.screensaver_edit.setText(path)
        self._refresh_screensaver_status()

    def _refresh_screensaver_status(self):
        from .asset_pipeline import process_asset, AssetState
        path = self.screensaver_edit.text().strip() or None
        result = process_asset(path, 96, 54, has_mask=False, header_bytes=0, format_label=".screensaver.cover")
        if result.state == AssetState.EMPTY:
            self.screensaver_status_label.setText("No custom screensaver image selected — the built-in mark is used.")
        elif result.state in (AssetState.MISSING, AssetState.ERROR):
            self.screensaver_status_label.setText(f"Problem: {result.message}")
        else:
            self.screensaver_status_label.setText(
                f"Ready: will export as a 96x54 headerless RGB565 sidecar ({result.runtime_bytes} bytes)."
            )

    def _reset_stream_controls(self):
        """Restore all selectors to the legacy automatic/no-subtitle behavior."""
        for combo, text in (
            (self.video_stream_combo, "Auto — first video stream (legacy default)"),
            (self.audio_stream_combo, "Auto — first audio stream (legacy default)"),
            (self.subtitle_stream_combo, "No subtitle"),
        ):
            combo.clear()
            combo.addItem(text, None)
        self.subtitle_edition_combo.setCurrentIndex(0)
        self.subtitle_edition_combo.setEnabled(False)
        self.stream_status_label.setText("Choose a source to inspect its streams.")

    def _update_subtitle_edition_enabled(self):
        self.subtitle_edition_combo.setEnabled(self.subtitle_stream_combo.currentData() is not None)

    def _probe_source_streams(self, show_errors: bool):
        """Populate selectors from MediaProbe and reconcile saved choices.

        This method is intentionally synchronous only when called after an
        explicit Browse/Open action. The existing MediaProbe cache prevents a
        second ffprobe invocation for an unchanged source.
        """
        source = self.source_edit.text().strip()
        self._reset_stream_controls()
        if not source:
            return
        resolved = self.project.resolve(source) or Path(source)
        try:
            probe = probe_media_cached(resolved, compute_hash=False)
        except MediaProbeError as exc:
            self.stream_status_label.setText(f"Could not inspect streams: {exc}")
            if show_errors:
                QMessageBox.warning(self, "Stream inspection failed", str(exc))
            return

        ordinary_video = [v for v in probe.video_streams if not v.is_attached_pic]
        for stream in ordinary_video:
            self.video_stream_combo.addItem(
                _stream_label("video", stream), stream.stream_index
            )
        for stream in probe.audio_streams:
            self.audio_stream_combo.addItem(
                _stream_label("audio", stream), stream.stream_index
            )
        for stream in probe.subtitle_streams:
            self.subtitle_stream_combo.addItem(
                _stream_label("subtitle", stream), stream.stream_index
            )
            if stream.kind != "text":
                # Visible so the user can see it exists (and why it's
                # ineligible), never silently omitted -- but not selectable:
                # no proven extraction pipeline exists for bitmap subtitles,
                # and an "unknown" codec has no classification to trust.
                item_index = self.subtitle_stream_combo.count() - 1
                model_item = self.subtitle_stream_combo.model().item(item_index)
                if model_item is not None:
                    model_item.setEnabled(False)

        identity = _stream_identity(probe)
        saved_identity = self.project.stream_source_identity or {}
        identity_matches = saved_identity == identity
        requested_video = self.project.video_stream_index if identity_matches else None
        requested_audio = self.project.audio_stream_index if identity_matches else None
        requested_subtitle = self.project.subtitle_stream_index if identity_matches else None
        notices = []

        for combo, requested, kind in (
            (self.video_stream_combo, requested_video, "video"),
            (self.audio_stream_combo, requested_audio, "audio"),
            (self.subtitle_stream_combo, requested_subtitle, "subtitle"),
        ):
            if requested is None:
                combo.setCurrentIndex(0)
                continue
            index = combo.findData(requested)
            # findData() matches by stream_index alone; also reject a match
            # that landed on a disabled (bitmap/unknown) item -- a saved
            # selection must never silently resolve to a stream we're
            # refusing to extract from.
            eligible = index >= 0 and (
                combo.model().item(index) is None or combo.model().item(index).isEnabled()
            )
            if eligible:
                combo.setCurrentIndex(index)
            else:
                combo.setCurrentIndex(0)
                notices.append(
                    f"saved {kind} stream #{requested} is no longer available"
                )

        if self.subtitle_stream_combo.currentData() is not None and identity_matches:
            edition_index = self.subtitle_edition_combo.findData(self.project.subtitle_edition)
            if edition_index >= 0:
                self.subtitle_edition_combo.setCurrentIndex(edition_index)
        self._update_subtitle_edition_enabled()

        if saved_identity and not identity_matches:
            notices.append(
                "source identity changed; saved stream selections were reset "
                "to legacy defaults"
            )

        self.project.stream_source_identity = identity
        self.project.video_stream_index = self.video_stream_combo.currentData()
        self.project.audio_stream_index = self.audio_stream_combo.currentData()
        self.project.subtitle_stream_index = self.subtitle_stream_combo.currentData()
        self.project.subtitle_edition = self.subtitle_edition_combo.currentData() or 0
        summary = (
            f"Found {len(ordinary_video)} video, "
            f"{len(probe.audio_streams)} audio, and "
            f"{len(probe.subtitle_streams)} subtitle stream(s)."
        )
        if notices:
            summary += " " + "; ".join(notices) + "."
        self.stream_status_label.setText(summary)

    def _pick_output(self):
        path, _ = QFileDialog.getSaveFileName(self, "Select output .mivf", "", "MIVF (*.mivf)")
        if path:
            self.output_edit.setText(path)

    def _pick_accent(self):
        color = QColorDialog.getColor(self.accent_swatch, self, "Choose accent color")
        if color.isValid():
            self.accent_swatch = color
            self._refresh_preview()

    def _pick_outline(self):
        color = QColorDialog.getColor(self.outline_swatch, self, "Choose outline color")
        if color.isValid():
            self.outline_swatch = color
            self._refresh_preview()

    def _pick_back_fill(self):
        color = QColorDialog.getColor(self.back_fill_swatch, self, "Choose Back fill color")
        if color.isValid():
            self.back_fill_swatch = color
            self.back_fill_enabled = True
            self._refresh_preview()

    def _open_background_editor(self):
        self._sync_project_from_fields()
        mode = self.bg_mode_combo.currentData()
        if mode == "builtin":
            QMessageBox.information(self, "Built-in background", "Choose Custom or Generated mode before editing background readability.")
            return
        accent = (self.accent_swatch.red(), self.accent_swatch.green(), self.accent_swatch.blue())
        if BackgroundRecipeDialog(self.project, accent, self).exec() == QDialog.Accepted:
            self._refresh_preview()

    def _background_mode_changed(self):
        if not hasattr(self, "bg_row"):
            return
        mode = self.bg_mode_combo.currentData()
        self.bg_row.setEnabled(mode == "custom")
        if hasattr(self, "bg_readability_btn"):
            self.bg_readability_btn.setEnabled(mode != "builtin")
        self._refresh_preview()

    def _background_source_changed(self):
        # AssetStatusRow invokes on_change while its constructor is still running,
        # before MainWindow has assigned self.bg_row. Ignore that construction-time
        # callback; later source assignments follow the legacy activate-Custom flow.
        if not hasattr(self, "bg_row"):
            return
        if hasattr(self, "bg_mode_combo") and self.bg_row.path():
            idx = self.bg_mode_combo.findData("custom")
            if idx >= 0 and self.bg_mode_combo.currentIndex() != idx:
                self.bg_mode_combo.setCurrentIndex(idx)
                return
        self._refresh_preview()

    def _on_preset_changed(self):
        name = self.preset_combo.currentData()
        self.preset_desc_label.setText(PRESET_DESCRIPTIONS.get(name, ""))

    def _set_preview_focus_index(self, index: int):
        self.preview.focused_index = index
        self.preview.update()

    def _set_back_focus(self, focused: bool):
        self.back_preview.focused = focused
        self.back_preview.update()

    def _current_state_name(self, control: str) -> str:
        """Which state (Idle/Focused) is 'currently showing' for this control
        right now, matching the exact same rule _refresh_preview() already
        uses for the main Preview tab (one dashboard control focused via
        self.preview.focused_index; Back focused via self.back_preview.focused)
        -- shared so the Asset Inspector can agree with the Preview tab about
        which state's recipe is in effect."""
        if control == "movie_menu_back":
            return "focused" if self.back_preview.focused else "idle"
        focused_control = {0: "rewind", 1: "play_pause", 2: "fast_forward"}.get(self.preview.focused_index, "play_pause")
        return "focused" if control == focused_control else "idle"

    def _recipe_asset_result(self, control: str, state_name: str):
        """Studio-recipe-aware AssetResult for the Asset Inspector, mirroring
        _resolve_control_pixmap()'s precedence (recipe when control_edits has
        an entry, else None so the caller falls back to the legacy row).
        Bug fix: before this, the Inspector always read the legacy
        AssetStatusRow regardless of any applied Studio recipe, so a control
        edited only through the Studio showed a stale/unrelated image here
        even though the Preview tab (fixed in C.3.1) showed the real one.
        Returns None on any failure or when no recipe applies -- never
        raises, matching _resolve_control_pixmap()'s own fallback discipline."""
        edits = self.project.artwork.control_edits or {}
        if control not in edits:
            return None
        try:
            leg_key = CONTROL_RECIPE_LEG[control]
            legacy_source = getattr(self.project.artwork, leg_key)
            legacy_fit = self.project.artwork.fit_modes.get(leg_key, "contain")
            recipe = control_recipe.state(edits, control, state_name, legacy_source, legacy_fit)
            base = self.project.project_path.parent if self.project.project_path else None
            r = control_recipe.render(control, recipe, base)
        except Exception:  # noqa: BLE001 -- fall back to the legacy row, never crash the Inspector
            return None

        source_image = source_w = source_h = None
        source_format, source_bytes, has_alpha = "", 0, False
        source_path = recipe.get("source")
        if source_path:
            try:
                p = Path(source_path)
                if base and not p.is_absolute():
                    p = (base / p).resolve()
                with Image.open(p) as im:
                    source_image = im.convert("RGBA")
                    source_w, source_h = im.width, im.height
                    source_format = im.format or p.suffix.lstrip(".").upper()
                    has_alpha = im.mode in ("RGBA", "LA", "PA") or "transparency" in im.info
                source_bytes = p.stat().st_size
            except Exception:
                pass  # source became unreadable between render() succeeding and this re-open; leave fields at defaults

        binary = r["binary"]
        total = binary.width * binary.height
        visible = sum(1 for v in binary.getdata() if v) if total else 0
        return AssetResult(
            state=AssetState.READY,
            message=f"Studio recipe ({state_name}): {len(r['asset'])} bytes (matches real .mivfasset output)",
            source_image=source_image, prepared_image=r["prepared"], runtime_image=r["runtime"],
            mask_image=binary.convert("RGBA"),
            source_w=source_w or 0, source_h=source_h or 0, source_format=source_format, source_bytes=source_bytes,
            has_alpha=has_alpha, runtime_w=binary.width, runtime_h=binary.height, runtime_bytes=len(r["asset"]),
            mask_byte_count=(total + 7) // 8, visible_px=visible, transparent_px=total - visible,
        )

    def _inspector_getter(self, control: str, legacy_row):
        """Wraps a legacy AssetStatusRow's result() so the Inspector prefers
        an applied Studio recipe when one exists for this control."""
        def getter():
            recipe_result = self._recipe_asset_result(control, self._current_state_name(control))
            if recipe_result is not None:
                return recipe_result
            return legacy_row.result()
        return getter

    def _resolve_control_pixmap(self, control: str, state_name: str, legacy_row):
        """Phase C.3.1: recipe-aware pixmap resolution. Prefers the real C.3
        renderer (control_recipe.state()+render() -- the exact same calls
        control_editor.py's own preview() and the exporter use, not a
        reimplementation) whenever project.artwork.control_edits has an
        entry for this control, so the main Preview tab reflects applied
        Studio edits. Falls back to the legacy AssetStatusRow's
        prepared_image on any error (missing/invalid recipe source, bad
        file, etc.) or when no recipe exists at all -- never raises, never
        blanks the tab. Returns (pixmap_or_None, warning_message_or_None).
        Does not mutate project data and does not write any file."""
        edits = self.project.artwork.control_edits or {}
        if control in edits:
            try:
                leg_key = CONTROL_RECIPE_LEG[control]
                legacy_source = getattr(self.project.artwork, leg_key)
                legacy_fit = self.project.artwork.fit_modes.get(leg_key, "contain")
                recipe = control_recipe.state(edits, control, state_name, legacy_source, legacy_fit)
                base = self.project.project_path.parent if self.project.project_path else None
                r = control_recipe.render(control, recipe, base)
                return pil_to_qpixmap(r["runtime"]), None
            except Exception as e:  # noqa: BLE001 -- any recipe failure must fall back, never crash the tab
                fallback = None
                if legacy_row is not None:
                    legacy_result = legacy_row.result()
                    if legacy_result and legacy_result.prepared_image:
                        fallback = pil_to_qpixmap(legacy_result.prepared_image)
                return fallback, f"{control} ({state_name}): Studio recipe failed, showing legacy/built-in appearance — {e}"
        if legacy_row is not None:
            legacy_result = legacy_row.result()
            if legacy_result and legacy_result.prepared_image:
                return pil_to_qpixmap(legacy_result.prepared_image), None
        return None, None

    def _refresh_preview(self):
        """Reactive rebuild: called automatically whenever any asset row,
        color, or field changes (via AssetStatusRow's on_change callback and
        the color pickers above), and whenever Control Artwork Studio Apply
        succeeds -- never requires a manual click for normal use. Registers/
        re-registers the inspector's element slots each call so a newly-typed
        path is picked up immediately.

        Phase C.3.1: dashboard controls now resolve through
        _resolve_control_pixmap() (real C.3 recipe renderer when
        project.artwork.control_edits has an entry, legacy AssetStatusRow
        otherwise) instead of unconditionally reading the legacy row. The
        dashboard background has no C.3 recipe support (control_recipe.SPECS
        has no 'dashboard_bg' entry) and keeps its existing legacy-only path
        unchanged."""
        if not hasattr(self, "preview"):
            return  # Preview tab not built yet (constructor ordering)

        bg_mode = self.bg_mode_combo.currentData() if hasattr(self, "bg_mode_combo") else background_recipe.effective_mode(self.project.artwork)
        bg_pixmap = None
        if bg_mode == "custom":
            try:
                base = self.project.project_path.parent if self.project.project_path else None
                br = background_recipe.render("custom", self.bg_row.path(), self.project.artwork.dashboard_bg_recipe, (self.accent_swatch.red(), self.accent_swatch.green(), self.accent_swatch.blue()), base)
                bg_pixmap = pil_to_qpixmap(br["runtime"])
            except Exception:
                bg_pixmap = None
        elif bg_mode == "generated":
            try:
                br = background_recipe.render("generated", recipe=self.project.artwork.dashboard_bg_recipe, accent=(self.accent_swatch.red(), self.accent_swatch.green(), self.accent_swatch.blue()))
                bg_pixmap = pil_to_qpixmap(br["runtime"])
            except Exception:
                bg_pixmap = None

        focused_control = {0: "rewind", 1: "play_pause", 2: "fast_forward"}.get(self.preview.focused_index, "play_pause")
        warnings = []

        def eff_state(control):
            return "focused" if control == focused_control else "idle"

        rewind_pixmap, w1 = self._resolve_control_pixmap("rewind", eff_state("rewind"), self.rewind_row)
        pp_pixmap, w2 = self._resolve_control_pixmap("play_pause", eff_state("play_pause"), self.pp_row)
        ff_pixmap, w3 = self._resolve_control_pixmap("fast_forward", eff_state("fast_forward"), self.ff_row)
        back_state_name = "focused" if self.back_preview.focused else "idle"
        back_pixmap, w4 = self._resolve_control_pixmap("movie_menu_back", back_state_name, self.back_row)
        warnings = [w for w in (w1, w2, w3, w4) if w]
        self.preview_recipe_warning.setText(warnings[0] if warnings else "")

        self.preview.set_state(self.accent_swatch, self.outline_swatch, bg_pixmap, ff_pixmap, pp_pixmap, rewind_pixmap)
        self.back_preview.set_state(back_pixmap, self.back_fill_swatch, self.back_fill_enabled, self.back_preview.focused)

        self.inspector._slots.clear()
        self.inspector.element_combo.clear()
        # Phase C.5a follow-up fix: the Inspector used to always read the
        # legacy AssetStatusRow, even for a control edited only through the
        # Control Artwork Studio -- so it could show a stale or unrelated
        # image while the Preview tab (fixed in C.3.1) showed the real one.
        # _inspector_getter() applies the same recipe-first precedence here.
        self.inspector.register_slot("Dashboard Background", self.bg_row.result)
        self.inspector.register_slot("Rewind", self._inspector_getter("rewind", self.rewind_row))
        self.inspector.register_slot("Fast Forward", self._inspector_getter("fast_forward", self.ff_row))
        self.inspector.register_slot("Play/Pause", self._inspector_getter("play_pause", self.pp_row))
        self.inspector.register_slot("Movie Menu Back", self._inspector_getter("movie_menu_back", self.back_row))
        self.inspector._refresh()

    def _generate_command(self) -> bool:
        """Unified Create-MIVF preflight: the single source of truth for
        "is this project safe to launch right now." Always re-run
        immediately before a real launch (see _launch_encode) rather than
        reusing a stale self._argv from a previous, possibly-since-edited
        state. Returns True only if _argv was (re)built and every check
        below passed."""
        self._sync_project_from_fields()
        # Preserve the legacy automatic path: projects with no explicit
        # stream choice and no recorded stream identity do not need a probe
        # just to generate the same historical first-stream command. This is
        # also important for offline/project-review workflows where the source
        # may not currently be attached. Once stream selection has actually
        # been used, identity and index validation remain mandatory.
        has_stream_contract = bool(
            self.project.stream_source_identity
            or self.project.video_stream_index is not None
            or self.project.audio_stream_index is not None
            or self.project.subtitle_stream_index is not None
        )
        if self.project.source_media and has_stream_contract:
            resolved_source = self.project.resolve(self.project.source_media)
            try:
                probe = probe_media_cached(resolved_source, compute_hash=False)
                if self.project.stream_source_identity and self.project.stream_source_identity != _stream_identity(probe):
                    QMessageBox.warning(self, "Source changed", "The source identity changed after stream selection. Re-inspect the source and choose streams again.")
                    return False
                valid_video = {v.stream_index for v in probe.video_streams if not v.is_attached_pic}
                valid_audio = {a.stream_index for a in probe.audio_streams}
                subtitle_by_index = {s.stream_index: s for s in probe.subtitle_streams}
                if self.project.video_stream_index is not None and self.project.video_stream_index not in valid_video:
                    QMessageBox.warning(self, "Video stream unavailable", "The selected video stream is no longer present.")
                    return False
                if self.project.audio_stream_index is not None and self.project.audio_stream_index not in valid_audio:
                    QMessageBox.warning(self, "Audio stream unavailable", "The selected audio stream is no longer present.")
                    return False
                if self.project.subtitle_stream_index is not None:
                    subtitle_stream = subtitle_by_index.get(self.project.subtitle_stream_index)
                    if subtitle_stream is None:
                        QMessageBox.warning(self, "Subtitle stream unavailable", "The selected subtitle stream is no longer present.")
                        return False
                    if subtitle_stream.kind != "text":
                        QMessageBox.warning(
                            self, "Subtitle stream unsupported",
                            f"The selected subtitle stream is {subtitle_stream.kind} ({subtitle_stream.codec}), "
                            "which cannot be authored as a sidecar. Choose a text subtitle stream or No subtitle."
                        )
                        return False
            except MediaProbeError as exc:
                QMessageBox.warning(self, "Stream validation failed", str(exc))
                return False
        missing = self.project.missing_files()
        if missing:
            QMessageBox.warning(self, "Missing files", "\n".join(missing))
        try:
            self._argv = build_argv(self.project, ENCODER_SCRIPT)
        except ValueError as e:
            QMessageBox.critical(self, "Cannot generate command", str(e))
            return False
        self.command_preview.setPlainText(format_command_preview(self._argv))

        lines = [
            f"Source: {self.project.source_media or '(not set)'}",
            f"Output: {self.project.output_path or '(not set)'}",
            f"Preset: {PRESET_LABELS.get(self.project.preset, self.project.preset)}",
            "",
            f"Dashboard background: {self.bg_row.result().state.value} — {self.bg_row.result().message}",
            f"Rewind:               {self.rewind_row.result().state.value} — {self.rewind_row.result().message}",
            f"Fast Forward:         {self.ff_row.result().state.value} — {self.ff_row.result().message}",
            f"Play/Pause:           {self.pp_row.result().state.value} — {self.pp_row.result().message}",
            f"Movie Menu Back:      {self.back_row.result().state.value} — {self.back_row.result().message}",
        ]
        out_path = self.project.resolve(self.project.output_path)
        if out_path and out_path.exists():
            lines.append("")
            lines.append(f"WARNING: output file already exists and will be overwritten: {out_path}")

        # Preflight: disk space + output-directory writability. Neither
        # existed anywhere in the pre-encode path before -- the only place
        # a user could see space projections was the separate, manually
        # opened Storage Planner dialog, and writability was never checked
        # at all (a bad destination would only surface after the backend
        # subprocess had already started). Reuses build_storage_plan(),
        # the same pure model the Storage Planner dialog itself calls --
        # no duplicated disk-usage/probing logic.
        blocking_preflight_errors: list[str] = []
        plan = build_storage_plan(self.project)
        free = plan.projected_destination_free
        lines.append("")
        if free.bytes is not None and free.bytes < 0:
            blocking_preflight_errors.append(
                f"Estimated output ({plan.estimated_output_size.label}) would exceed free space on the "
                f"destination volume by about {abs(free.bytes):,} bytes ({free.label})."
            )
            lines.append(f"BLOCKING: insufficient destination disk space -- {free.label}")
        else:
            # Always shown, even when UNKNOWN (e.g. an unprobeable source) --
            # a degraded-but-honest estimate stays visible rather than being
            # silently dropped, matching this codebase's own established
            # convention elsewhere (e.g. bitmap subtitle streams: "shown but
            # disabled, not silently omitted").
            lines.append(f"Disk space: {free.label}")
        if plan.destination_volume is not None:
            if not os.access(plan.destination_volume.path, os.W_OK):
                blocking_preflight_errors.append(
                    f"Destination directory is not writable: {plan.destination_volume.path}"
                )
                lines.append(f"BLOCKING: destination directory is not writable: {plan.destination_volume.path}")

        self.review_summary.setPlainText("\n".join(lines))

        if blocking_preflight_errors:
            QMessageBox.critical(self, "Cannot create MIVF", "\n\n".join(blocking_preflight_errors))
            return False
        return True

    def _launch_encode(self):
        # Always re-run the full preflight immediately before a real launch
        # -- previously this only ran _generate_command() the first time
        # (hasattr check), so editing a field after an earlier "Validate &&
        # Generate Command" click and then clicking Launch directly would
        # silently launch a stale command reflecting the OLD field values.
        if not self._generate_command():
            return
        source = self.project.resolve(self.project.source_media)
        if source is None:
            QMessageBox.critical(self, "Launch failed", "No source media is set.")
            return
        self.log_view.clear()
        self.result_label.setText("")
        self.progress_stage_label.setText("Analyzing source…")
        self.progress_bar.setRange(0, 0)
        self.progress_timing_label.setText("Preparing trustworthy progress information…")
        self.run_btn.setEnabled(False)
        self.cancel_btn.setEnabled(False)
        self.encode_probe_worker = _EncodeSourceProbeWorker(Path(source), self)
        self.encode_probe_worker.succeeded.connect(self._start_encode_after_probe)
        self.encode_probe_worker.failed.connect(self._start_encode_without_total)
        self.encode_probe_worker.start()

    def _start_encode_without_total(self, message: str):
        self._start_encode_after_probe(None, f"Could not determine a frame total: {message}")

    def _start_encode_after_probe(self, total_frames, note: str):
        self.encode_run = EncodeRun(self._argv, total_frames=total_frames)
        self._last_progress = None
        self._encode_cancel_requested = False
        self.encode_started_monotonic = time.monotonic()
        self.progress_stage_label.setText(f"Starting backend — {note}")
        if total_frames:
            self.progress_bar.setRange(0, 100)
            self.progress_bar.setValue(0)
        else:
            self.progress_bar.setRange(0, 0)
        try:
            self.encode_run.start()
        except Exception as e:  # noqa: BLE001
            self.run_btn.setEnabled(True)
            self.progress_bar.setRange(0, 100)
            self.progress_bar.setValue(0)
            self.progress_stage_label.setText("Launch failed")
            QMessageBox.critical(self, "Launch failed", str(e))
            return
        self.cancel_btn.setEnabled(True)
        self.encode_elapsed_timer.start(500)
        self.poll_timer.start(200)

    @staticmethod
    def _format_duration(seconds: float | None) -> str:
        if seconds is None:
            return "--"
        value = max(0, round(seconds))
        hours, rem = divmod(value, 3600)
        minutes, secs = divmod(rem, 60)
        return f"{hours:d}:{minutes:02d}:{secs:02d}" if hours else f"{minutes:d}:{secs:02d}"

    def _refresh_elapsed_label(self, finished: bool = False):
        elapsed = time.monotonic() - self.encode_started_monotonic if self.encode_started_monotonic is not None else None
        speed = self._last_progress.speed_fps if self._last_progress else None
        eta = self._last_progress.eta_seconds if self._last_progress else None
        speed_text = f"{speed:.1f} fps" if speed else "--"
        if finished:
            # The process has already exited -- nothing is being calculated
            # any more, regardless of whether the last in-progress update
            # happened to carry a known ETA. Audited defect: this label
            # previously kept showing "Calculating..." after completion
            # because it recomputed from the last (stage-dependent, often
            # ETA-less) progress event rather than reflecting that the run
            # is over.
            eta_text = self._format_duration(0)
        else:
            eta_text = self._format_duration(eta) if eta is not None else "Calculating…"
        self.progress_timing_label.setText(
            f"Elapsed {self._format_duration(elapsed)} · Speed {speed_text} · Remaining {eta_text}"
        )

    def _update_encode_progress(self, progress: EncodeProgress):
        self._last_progress = progress
        self.progress_stage_label.setText(progress.label + (f" — {progress.detail}" if progress.detail else ""))
        percent = progress.percent
        if percent is None:
            self.progress_bar.setRange(0, 0)
        else:
            self.progress_bar.setRange(0, 100)
            self.progress_bar.setValue(percent)
        self._refresh_elapsed_label()

    def _poll_encode(self):
        if self.encode_run is None:
            self.poll_timer.stop()
            return
        still_running = self.encode_run.poll_output(
            self.log_view.appendPlainText, self._update_encode_progress
        )
        if not still_running:
            self.poll_timer.stop()
            self.encode_elapsed_timer.stop()
            self.run_btn.setEnabled(True)
            self.cancel_btn.setEnabled(False)
            self._finish_encode()

    def _cancel_encode(self):
        if self.encode_run:
            self._encode_cancel_requested = True
            self.progress_stage_label.setText("Cancelling…")
            self.encode_run.cancel()
        self.cancel_btn.setEnabled(False)

    def _finish_encode(self):
        if not self.encode_run:
            return
        out_path = self.project.resolve(self.project.output_path)
        ok, msg = verify_output(out_path) if out_path else (False, "no output path set")
        returncode = self.encode_run.returncode
        self.result_label.setText(f"Exit code {returncode}: {msg}")
        if returncode == 0 and ok:
            self.progress_bar.setRange(0, 100)
            self.progress_bar.setValue(100)
            self.progress_stage_label.setText("Completed")
        elif returncode is not None:
            self.progress_stage_label.setText("Cancelled" if self._encode_cancel_requested else "Failed")
        self._refresh_elapsed_label(finished=True)
        if self.project.job_dir:
            log_path = Path(self.project.resolve(self.project.job_dir) or ".") / "gui_run.log"
            try:
                self.encode_run.save_raw_log(log_path)
            except OSError:
                pass

    # ---- D: Persistent multi-project encode queue -------------------------

    def _refresh_queue_list(self):
        self.queue_list.clear()
        for job in self.queue_jobs:
            name = Path(job.project_path).name
            label = f"[{encode_queue.status_label(job.status)}] {name}"
            if job.error:
                label += f" — {job.error}"
            item = QListWidgetItem(label)
            item.setData(Qt.UserRole, job.job_id)
            self.queue_list.addItem(item)
        self._refresh_queue_button_states()

    def _selected_queue_job(self) -> encode_queue.QueueJob | None:
        item = self.queue_list.currentItem()
        if item is None:
            return None
        job_id = item.data(Qt.UserRole)
        return next((j for j in self.queue_jobs if j.job_id == job_id), None)

    def _refresh_queue_button_states(self):
        job = self._selected_queue_job()
        row = self.queue_list.currentRow()
        self.queue_up_btn.setEnabled(job is not None and row > 0)
        self.queue_down_btn.setEnabled(job is not None and row >= 0 and row < self.queue_list.count() - 1)
        # Never remove the job currently being encoded out from under the
        # running EncodeRun -- its job_dir/process are still live.
        self.queue_remove_btn.setEnabled(
            job is not None and (self.queue_active_job is None or job.job_id != self.queue_active_job.job_id)
        )
        self.queue_start_btn.setEnabled(not self.queue_running)
        self.queue_stop_btn.setEnabled(self.queue_running)

    def _add_current_project_to_queue(self):
        # Same "must be saved first" discipline as Check Project
        # (_open_preflight_dialog) -- a queue entry references a project
        # FILE, never an inline/duplicated project representation, so an
        # unsaved project has nothing to reference yet.
        self._sync_project_from_fields()
        if not self.project.project_path:
            QMessageBox.information(self, "Save project first",
                                     "Save this project as a .mivfproj file before adding it to the queue.")
            return
        self.project.save(self.project.project_path)
        add_recent_project(str(self.project.project_path))
        self.queue_jobs_root.mkdir(parents=True, exist_ok=True)
        job = encode_queue.new_job(str(self.project.project_path), self.queue_jobs_root)
        self.queue_jobs.append(job)
        encode_queue.save_queue(self.queue_jobs)
        self._refresh_queue_list()

    def _remove_selected_queue_job(self):
        job = self._selected_queue_job()
        if job is None:
            return
        self.queue_jobs = encode_queue.remove_job(self.queue_jobs, job.job_id)
        encode_queue.save_queue(self.queue_jobs)
        self._refresh_queue_list()

    def _move_selected_queue_job(self, delta: int):
        job = self._selected_queue_job()
        if job is None:
            return
        self.queue_jobs = encode_queue.move_job(self.queue_jobs, job.job_id, delta)
        encode_queue.save_queue(self.queue_jobs)
        self._refresh_queue_list()
        # Re-select the job that moved, so repeated clicks keep walking it
        # up/down instead of losing the selection each time.
        for i in range(self.queue_list.count()):
            if self.queue_list.item(i).data(Qt.UserRole) == job.job_id:
                self.queue_list.setCurrentRow(i)
                break

    def _start_queue(self):
        if self.queue_running:
            return
        self.queue_running = True
        self.queue_stop_requested = False
        self._refresh_queue_button_states()
        self._start_next_queue_job()

    def _stop_queue_after_current(self):
        self.queue_stop_requested = True
        self.queue_status_label.setText(
            self.queue_status_label.text() + " (stopping after this job)"
        )

    def _start_next_queue_job(self):
        if self.queue_stop_requested:
            self.queue_running = False
            self.queue_status_label.setText("Stopped after current job.")
            self._refresh_queue_button_states()
            return
        job = encode_queue.next_pending(self.queue_jobs)
        if job is None:
            self.queue_running = False
            self.queue_status_label.setText("Queue complete — no pending jobs.")
            self._refresh_queue_button_states()
            return
        try:
            queue_project = MivfProject.load(Path(job.project_path))
        except Exception as e:  # noqa: BLE001 -- recorded on the job, not swallowed
            job.status = encode_queue.STATUS_FAILED
            job.error = f"could not load project: {e}"
            encode_queue.save_queue(self.queue_jobs)
            self._refresh_queue_list()
            self._start_next_queue_job()
            return
        queue_project.job_dir = job.job_dir
        queue_project.resume_job = job.has_resumable_manifest()
        try:
            queue_argv = build_argv(queue_project, ENCODER_SCRIPT)
        except ValueError as e:
            job.status = encode_queue.STATUS_FAILED
            job.error = f"could not build command: {e}"
            encode_queue.save_queue(self.queue_jobs)
            self._refresh_queue_list()
            self._start_next_queue_job()
            return
        source = queue_project.resolve(queue_project.source_media)
        if source is None:
            job.status = encode_queue.STATUS_FAILED
            job.error = "project has no source media set"
            encode_queue.save_queue(self.queue_jobs)
            self._refresh_queue_list()
            self._start_next_queue_job()
            return

        job.status = encode_queue.STATUS_RUNNING
        job.error = None
        encode_queue.save_queue(self.queue_jobs)
        self._refresh_queue_list()

        self.queue_active_job = job
        self._queue_argv = queue_argv
        self.queue_log_view.clear()
        self.queue_status_label.setText(f"Analyzing source — {Path(job.project_path).name}")
        self.queue_progress_bar.setRange(0, 0)
        self.queue_probe_worker = _EncodeSourceProbeWorker(Path(source), self)
        self.queue_probe_worker.succeeded.connect(self._start_queue_job_after_probe)
        self.queue_probe_worker.failed.connect(self._start_queue_job_without_total)
        self.queue_probe_worker.start()

    def _start_queue_job_without_total(self, message: str):
        self._start_queue_job_after_probe(None, f"Could not determine a frame total: {message}")

    def _start_queue_job_after_probe(self, total_frames, note: str):
        self.queue_encode_run = EncodeRun(self._queue_argv, total_frames=total_frames)
        self.queue_last_progress = None
        self.queue_started_monotonic = time.monotonic()
        self.queue_status_label.setText(f"Running — {Path(self.queue_active_job.project_path).name} ({note})")
        self.queue_progress_bar.setRange(0, 100 if total_frames else 0)
        self.queue_progress_bar.setValue(0)
        try:
            self.queue_encode_run.start()
        except Exception as e:  # noqa: BLE001
            self.queue_active_job.status = encode_queue.STATUS_FAILED
            self.queue_active_job.error = str(e)
            encode_queue.save_queue(self.queue_jobs)
            self._refresh_queue_list()
            self.queue_active_job = None
            self._start_next_queue_job()
            return
        self.queue_elapsed_timer.start(500)
        self.queue_poll_timer.start(200)

    def _refresh_queue_elapsed_label(self, finished: bool = False):
        elapsed = time.monotonic() - self.queue_started_monotonic if self.queue_started_monotonic is not None else None
        speed = self.queue_last_progress.speed_fps if self.queue_last_progress else None
        eta = self.queue_last_progress.eta_seconds if self.queue_last_progress else None
        speed_text = f"{speed:.1f} fps" if speed else "--"
        eta_text = self._format_duration(0) if finished else (self._format_duration(eta) if eta is not None else "Calculating…")
        self.queue_timing_label.setText(
            f"Elapsed {self._format_duration(elapsed)} · Speed {speed_text} · Remaining {eta_text}"
        )

    def _update_queue_progress(self, progress: EncodeProgress):
        self.queue_last_progress = progress
        self.queue_status_label.setText(progress.label + (f" — {progress.detail}" if progress.detail else ""))
        percent = progress.percent
        if percent is None:
            self.queue_progress_bar.setRange(0, 0)
        else:
            self.queue_progress_bar.setRange(0, 100)
            self.queue_progress_bar.setValue(percent)
        self._refresh_queue_elapsed_label()

    def _queue_poll_encode(self):
        if self.queue_encode_run is None:
            self.queue_poll_timer.stop()
            return
        still_running = self.queue_encode_run.poll_output(
            self.queue_log_view.appendPlainText, self._update_queue_progress
        )
        if not still_running:
            self.queue_poll_timer.stop()
            self.queue_elapsed_timer.stop()
            self._finish_queue_job()

    def _finish_queue_job(self):
        job = self.queue_active_job
        run = self.queue_encode_run
        if job is None or run is None:
            return
        queue_project = MivfProject.load(Path(job.project_path))
        out_path = queue_project.resolve(queue_project.output_path)
        ok, msg = verify_output(out_path) if out_path else (False, "no output path set")
        returncode = run.returncode
        if returncode == 0 and ok:
            job.status = encode_queue.STATUS_DONE
            job.error = None
        else:
            job.status = encode_queue.STATUS_FAILED
            job.error = f"exit code {returncode}: {msg}"
        self._refresh_queue_elapsed_label(finished=True)
        try:
            run.save_raw_log(Path(job.job_dir) / "gui_run.log")
        except OSError:
            pass
        encode_queue.save_queue(self.queue_jobs)
        self._refresh_queue_list()
        self.queue_active_job = None
        self.queue_encode_run = None
        self._start_next_queue_job()


def main():
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    if should_show_at_startup():
        window._show_project_home()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
