"""Local Theme Browser + the missing import half of theme sharing.

theme_export.py's export_theme_package() is a real, transactional,
versioned, SHA-256-hashed export -- but nothing ever reads a package back
in ("half the feature is done"). This module is the counterpart: scan a
folder for previously-exported ".mivftheme" packages and let the user
apply one's colors and dashboard layout to the current project.

Deliberately NOT a full project reconstruction. An exported package's
control artwork is already-rendered, final RGB565 runtime assets, not
the original source images the Toolkit edits (dashboard_bg_recipe,
control_edits, etc.) -- there is no lossless path back from "final
pixels" to "the editable recipe that produced them." What genuinely IS
recoverable, exactly and losslessly, is what the manifest stores as
plain values rather than rendered pixels: PALETTE_ACCENT, PALETTE_OUTLINE,
and CONTROL.POSITION (dashboard layout). This module imports exactly
those and is explicit that artwork import is out of scope, rather than
silently reconstructing something approximate.
"""
from __future__ import annotations

import dataclasses
from pathlib import Path

from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QListWidget,
    QListWidgetItem, QLineEdit, QFileDialog, QMessageBox,
)

from .project import ProjectTheme


@dataclasses.dataclass
class ThemePackageInfo:
    manifest_path: Path
    basename: str
    accent_rgb: tuple[int, int, int] | None
    outline_rgb: tuple[int, int, int] | None
    dashboard_layout: dict[str, list[int]]
    asset_count: int
    total_bytes: int


def parse_manifest_colors(text: str) -> dict:
    """Pure text parsing, no file I/O -- independently testable from
    scan_theme_packages. Tracks the current CONTROL block so a
    CONTROL.POSITION line is attributed to the right control name,
    mirroring the exact grammar theme_plan._manifest_text writes
    (CONTROL=NAME ... CONTROL.POSITION=dx,dy ... CONTROL.END)."""
    accent_rgb = None
    outline_rgb = None
    dashboard_layout: dict[str, list[int]] = {}
    current_control = None

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            continue
        key, _, value = line.partition("=")
        key = key.strip()
        value = value.strip()
        if key == "CONTROL":
            current_control = value
        elif key == "CONTROL.END":
            current_control = None
        elif key == "PALETTE_ACCENT":
            accent_rgb = _parse_rgb(value)
        elif key == "PALETTE_OUTLINE":
            outline_rgb = _parse_rgb(value)
        elif key == "CONTROL.POSITION" and current_control:
            dxdy = _parse_rgb(value, expected=2)
            if dxdy:
                dashboard_layout[current_control] = list(dxdy)

    return {"accent_rgb": accent_rgb, "outline_rgb": outline_rgb, "dashboard_layout": dashboard_layout}


def _parse_rgb(value: str, expected: int = 3) -> tuple[int, ...] | None:
    parts = value.split(",")
    if len(parts) != expected:
        return None
    try:
        return tuple(int(p.strip()) for p in parts)
    except ValueError:
        return None


def scan_theme_packages(folder: Path) -> list[ThemePackageInfo]:
    """Non-recursive by design -- a "themes" folder is expected to be a
    flat drop location (matching export_theme_package()'s own single-
    destination-directory model), not a tree a browser needs to walk."""
    folder = Path(folder)
    if not folder.is_dir():
        return []
    infos = []
    for manifest_path in sorted(folder.glob("*.mivftheme")):
        try:
            text = manifest_path.read_text(encoding="utf-8")
        except OSError:
            continue
        colors = parse_manifest_colors(text)
        basename = manifest_path.stem
        siblings = [p for p in folder.glob(f"{basename}.*.mivfasset")]
        infos.append(ThemePackageInfo(
            manifest_path=manifest_path,
            basename=basename,
            accent_rgb=colors["accent_rgb"],
            outline_rgb=colors["outline_rgb"],
            dashboard_layout=colors["dashboard_layout"],
            asset_count=len(siblings),
            total_bytes=sum(p.stat().st_size for p in siblings) + manifest_path.stat().st_size,
        ))
    return infos


def apply_theme_package(info: ThemePackageInfo, project) -> None:
    """Mutates project in place -- accent/outline/dashboard_layout only,
    per this module's own documented scope (see module docstring)."""
    if info.accent_rgb or info.outline_rgb:
        project.theme = ProjectTheme(
            accent_rgb=info.accent_rgb or project.theme.accent_rgb,
            outline_rgb=info.outline_rgb or project.theme.outline_rgb,
            back_fill_rgb=project.theme.back_fill_rgb,  # never exported; left untouched
        )
    if info.dashboard_layout:
        project.dashboard_layout = dict(info.dashboard_layout)


class ThemeBrowserDialog(QDialog):
    """Browse a folder of previously-exported theme packages and apply
    one's colors/layout to the current project. Mutates project only on
    Apply, never on selection alone -- browsing/previewing is always
    non-destructive."""

    def __init__(self, project, parent=None):
        super().__init__(parent)
        self.project = project
        self.setWindowTitle("Local Theme Browser")
        self.resize(520, 420)
        self.packages: list[ThemePackageInfo] = []
        self.applied = False

        layout = QVBoxLayout(self)
        note = QLabel(
            "Applies a previously-exported theme's accent/outline colors and dashboard "
            "layout to this project. Artwork is not re-imported -- an exported package "
            "contains only final, already-rendered runtime images, not the editable "
            "source files the Toolkit needs (see Artwork && Theme to set artwork directly)."
        )
        note.setWordWrap(True)
        layout.addWidget(note)

        folder_row = QHBoxLayout()
        self.folder_edit = QLineEdit()
        browse_btn = QPushButton("Browse...")
        scan_btn = QPushButton("Scan")
        browse_btn.clicked.connect(self._browse_folder)
        scan_btn.clicked.connect(self._scan)
        folder_row.addWidget(self.folder_edit)
        folder_row.addWidget(browse_btn)
        folder_row.addWidget(scan_btn)
        layout.addLayout(folder_row)

        self.list_widget = QListWidget()
        layout.addWidget(self.list_widget)

        self.detail_label = QLabel("")
        self.detail_label.setWordWrap(True)
        layout.addWidget(self.detail_label)
        self.list_widget.itemSelectionChanged.connect(self._refresh_detail)

        buttons = QHBoxLayout()
        close_btn = QPushButton("Close")
        apply_btn = QPushButton("Apply to Current Project")
        close_btn.clicked.connect(self.reject)
        apply_btn.clicked.connect(self._apply_selected)
        buttons.addStretch(1)
        buttons.addWidget(close_btn)
        buttons.addWidget(apply_btn)
        layout.addLayout(buttons)

    def _browse_folder(self):
        folder = QFileDialog.getExistingDirectory(self, "Select a folder of exported theme packages")
        if folder:
            self.folder_edit.setText(folder)
            self._scan()

    def _scan(self):
        folder_text = self.folder_edit.text().strip()
        if not folder_text:
            return
        self.packages = scan_theme_packages(Path(folder_text))
        self.list_widget.clear()
        for info in self.packages:
            item = QListWidgetItem(f"{info.basename}  ({info.asset_count} asset file(s), {info.total_bytes:,} bytes)")
            self.list_widget.addItem(item)
        if not self.packages:
            placeholder = QListWidgetItem("(no .mivftheme packages found in this folder)")
            self.list_widget.addItem(placeholder)

    def _selected_info(self) -> ThemePackageInfo | None:
        row = self.list_widget.currentRow()
        if row < 0 or row >= len(self.packages):
            return None
        return self.packages[row]

    def _refresh_detail(self):
        info = self._selected_info()
        if info is None:
            self.detail_label.setText("")
            return
        lines = [f"Manifest: {info.manifest_path}"]
        lines.append(f"Accent: {info.accent_rgb or 'not set in this package'}")
        lines.append(f"Outline: {info.outline_rgb or 'not set in this package'}")
        lines.append(f"Dashboard layout: {info.dashboard_layout or 'legacy fixed positions'}")
        self.detail_label.setText("\n".join(lines))

    def _apply_selected(self):
        info = self._selected_info()
        if info is None:
            QMessageBox.information(self, "No package selected", "Scan a folder and select a theme package first.")
            return
        apply_theme_package(info, self.project)
        self.applied = True
        self.accept()
