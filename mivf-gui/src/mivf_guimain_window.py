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
from .control_editor import ControlArtworkDialog

import sys
import copy
from pathlib import Path

from PySide6.QtCore import Qt, QTimer, QUrl
from PySide6.QtGui import QColor, QPainter, QPixmap, QPainterPath, QDesktopServices
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QFormLayout,
    QLabel, QLineEdit, QPushButton, QComboBox, QFileDialog, QTextEdit,
    QGroupBox, QColorDialog, QMessageBox, QTabWidget, QPlainTextEdit,
    QScrollArea, QSizePolicy, QDialog,
)

from .project import MivfProject, ProjectArtwork, ProjectTheme
from .presets import PRESET_NAMES, PRESET_LABELS, PRESET_DESCRIPTIONS
from .backend import build_argv, format_command_preview, EncodeRun, verify_output
from .asset_pipeline import process_asset, pil_to_qpixmap, FitMode, AssetState, clear_cache

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


class MainWindow(QMainWindow):

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
    def __init__(self):
        super().__init__()
        export_theme_action = self.menuBar().addAction("Export Runtime Theme Package...")
        export_theme_action.triggered.connect(self._open_theme_export_dialog)
        self.setWindowTitle("MIVF Toolkit (Phase C.1)")
        self.project = MivfProject()
        self.encode_run: EncodeRun | None = None
        self.poll_timer = QTimer(self)
        self.poll_timer.timeout.connect(self._poll_encode)

        self._build_ui()
        self._refresh_preview()

    # ---- UI construction -------------------------------------------------

    def _build_ui(self):
        tabs = QTabWidget()
        tabs.addTab(self._build_project_tab(), "1. Project")
        tabs.addTab(self._build_artwork_tab(), "2. Artwork && Theme")
        tabs.addTab(self._build_preview_tab(), "3. Preview")
        tabs.addTab(self._build_run_tab(), "4. Review && Run")
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

        dash_box = QGroupBox("Premiere dashboard (playback screen)")
        dash_form = QFormLayout(dash_box)
        self.bg_row = AssetStatusRow(*DASHBOARD_BG_DIMS, has_mask=False, on_change=self._refresh_preview)
        dash_form.addRow("Dashboard background:", self.bg_row)
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
        studio=QPushButton("Open Control Artwork Studio…");studio.clicked.connect(self._open_control_studio);dash_form.addRow("Idle / Focused editor:",studio)
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
        self.run_btn = QPushButton("Launch backend")
        self.cancel_btn = QPushButton("Cancel")
        self.cancel_btn.setEnabled(False)
        self.run_btn.clicked.connect(self._launch_encode)
        self.cancel_btn.clicked.connect(self._cancel_encode)
        run_row.addWidget(self.run_btn)
        run_row.addWidget(self.cancel_btn)
        layout.addLayout(run_row)

        self.log_view = QPlainTextEdit()
        self.log_view.setReadOnly(True)
        layout.addWidget(self.log_view)

        self.result_label = QLabel("")
        layout.addWidget(self.result_label)

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
        self.cover_card.path_edit.clear()
        for row in (self.bg_row, self.ff_row, self.pp_row, self.back_row, self.rewind_row):
            row.path_edit.clear()
        clear_cache()
        self._refresh_preview()

    def _open_project(self):
        path, _ = QFileDialog.getOpenFileName(self, "Open project", "", "MIVF Project (*.mivfproj)")
        if not path:
            return
        try:
            self.project = MivfProject.load(Path(path))
        except Exception as e:  # noqa: BLE001 -- surfaced to the user, not swallowed
            QMessageBox.critical(self, "Open project failed", str(e))
            return
        self.source_edit.setText(self.project.source_media or "")
        self.output_edit.setText(self.project.output_path or "")
        self.cover_card.path_edit.setText(self.project.artwork.cover or "")
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

    def _save_project(self):
        self._sync_project_from_fields()
        path, _ = QFileDialog.getSaveFileName(self, "Save project", "", "MIVF Project (*.mivfproj)")
        if not path:
            return
        self.project.save(Path(path))
        QMessageBox.information(self, "Saved", f"Project saved to {path}")

    def _sync_project_from_fields(self):
        self.project.source_media = self.source_edit.text() or None
        self.project.output_path = self.output_edit.text() or None
        self.project.preset = self.preset_combo.currentData()
        self.project.artwork = ProjectArtwork(
            cover=self.cover_card.path() or None,
            dashboard_bg=self.bg_row.path() or None,
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

    def _on_preset_changed(self):
        name = self.preset_combo.currentData()
        self.preset_desc_label.setText(PRESET_DESCRIPTIONS.get(name, ""))

    def _set_preview_focus_index(self, index: int):
        self.preview.focused_index = index
        self.preview.update()

    def _set_back_focus(self, focused: bool):
        self.back_preview.focused = focused
        self.back_preview.update()

    def _refresh_preview(self):
        """Reactive rebuild: called automatically whenever any asset row,
        color, or field changes (via AssetStatusRow's on_change callback and
        the color pickers above) -- never requires a manual click for normal
        use. Registers/re-registers the inspector's element slots each call
        so a newly-typed path is picked up immediately."""
        if not hasattr(self, "preview"):
            return  # Preview tab not built yet (constructor ordering)

        bg_result = self.bg_row.result()
        ff_result = self.ff_row.result()
        pp_result = self.pp_row.result()
        back_result = self.back_row.result()
        rewind_result = self.rewind_row.result()

        bg_pixmap = pil_to_qpixmap(bg_result.prepared_image) if bg_result and bg_result.prepared_image else None
        ff_pixmap = pil_to_qpixmap(ff_result.prepared_image) if ff_result and ff_result.prepared_image else None
        pp_pixmap = pil_to_qpixmap(pp_result.prepared_image) if pp_result and pp_result.prepared_image else None
        back_pixmap = pil_to_qpixmap(back_result.prepared_image) if back_result and back_result.prepared_image else None
        rewind_pixmap = pil_to_qpixmap(rewind_result.prepared_image) if rewind_result and rewind_result.prepared_image else None

        self.preview.set_state(self.accent_swatch, self.outline_swatch, bg_pixmap, ff_pixmap, pp_pixmap, rewind_pixmap)
        self.back_preview.set_state(back_pixmap, self.back_fill_swatch, self.back_fill_enabled, self.back_preview.focused)

        self.inspector._slots.clear()
        self.inspector.element_combo.clear()
        self.inspector.register_slot("Dashboard Background", self.bg_row.result)
        self.inspector.register_slot("Rewind", self.rewind_row.result)
        self.inspector.register_slot("Fast Forward", self.ff_row.result)
        self.inspector.register_slot("Play/Pause", self.pp_row.result)
        self.inspector.register_slot("Movie Menu Back", self.back_row.result)
        self.inspector._refresh()

    def _generate_command(self):
        self._sync_project_from_fields()
        missing = self.project.missing_files()
        if missing:
            QMessageBox.warning(self, "Missing files", "\n".join(missing))
        try:
            self._argv = build_argv(self.project, ENCODER_SCRIPT)
        except ValueError as e:
            QMessageBox.critical(self, "Cannot generate command", str(e))
            return
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
        self.review_summary.setPlainText("\n".join(lines))

    def _launch_encode(self):
        if not hasattr(self, "_argv"):
            self._generate_command()
            if not hasattr(self, "_argv"):
                return
        self.encode_run = EncodeRun(self._argv)
        self.log_view.clear()
        self.result_label.setText("")
        try:
            self.encode_run.start()
        except Exception as e:  # noqa: BLE001
            QMessageBox.critical(self, "Launch failed", str(e))
            return
        self.run_btn.setEnabled(False)
        self.cancel_btn.setEnabled(True)
        self.poll_timer.start(200)

    def _poll_encode(self):
        if self.encode_run is None:
            self.poll_timer.stop()
            return
        still_running = self.encode_run.poll_output(self.log_view.appendPlainText)
        if not still_running:
            self.poll_timer.stop()
            self.run_btn.setEnabled(True)
            self.cancel_btn.setEnabled(False)
            self._finish_encode()

    def _cancel_encode(self):
        if self.encode_run:
            self.encode_run.cancel()
        self.cancel_btn.setEnabled(False)

    def _finish_encode(self):
        if not self.encode_run:
            return
        out_path = self.project.resolve(self.project.output_path)
        ok, msg = verify_output(out_path) if out_path else (False, "no output path set")
        self.result_label.setText(f"Exit code {self.encode_run.returncode}: {msg}")
        if self.project.job_dir:
            log_path = Path(self.project.resolve(self.project.job_dir) or ".") / "gui_run.log"
            try:
                self.encode_run.save_raw_log(log_path)
            except OSError:
                pass


def main():
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
