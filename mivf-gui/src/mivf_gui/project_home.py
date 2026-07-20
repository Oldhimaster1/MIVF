"""UX.1: Project Home + guided creation.

A lightweight landing dialog in front of the existing tabbed editor --
not a replacement for it (main_window.py's own module docstring already
rules out a full IDE-style layout; this follows the same discipline).
Offers New Project (a short two-page guided wizard), Open Existing
Project, a small persisted Recent Projects list, and Skip. Skip -- or
simply closing the dialog -- reproduces the exact pre-Phase-2 behavior
(a blank editor): nothing here is required to use the Toolkit.

The wizard deliberately collects only the minimum to create a valid
project (source, output, preset) and then hands off to MainWindow's own
existing new/probe/populate methods -- it never reimplements project
creation or media probing itself.
"""
from __future__ import annotations

import json
from pathlib import Path

from PySide6.QtCore import Qt, QSettings
from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QListWidget,
    QListWidgetItem, QCheckBox, QStackedWidget, QWidget, QLineEdit,
    QFileDialog, QComboBox, QMessageBox, QFormLayout,
)

from .presets import PRESET_NAMES, PRESET_LABELS, PRESET_DESCRIPTIONS

_SETTINGS_ORG = "MIVF"
_SETTINGS_APP = "MIVFToolkit"
_RECENT_KEY = "project_home/recent_projects"
_SHOW_AT_STARTUP_KEY = "project_home/show_at_startup"
RECENT_CAP = 8


def _settings() -> QSettings:
    return QSettings(_SETTINGS_ORG, _SETTINGS_APP)


def dedup_cap_recent(existing: list[str], new_path: str | None, cap: int = RECENT_CAP) -> list[str]:
    """Pure logic, no QSettings I/O: most-recent-first, de-duplicated by
    resolved path, capped at `cap`. Kept separate from persistence so
    it's host-testable without a display or any saved state."""
    result: list[str] = []
    seen: set[str] = set()
    ordered = ([new_path] if new_path else []) + list(existing)
    for p in ordered:
        if not p:
            continue
        try:
            key = str(Path(p).resolve())
        except OSError:
            key = p
        if key in seen:
            continue
        seen.add(key)
        result.append(p)
        if len(result) >= cap:
            break
    return result


def load_recent_projects() -> list[str]:
    raw = _settings().value(_RECENT_KEY, "[]")
    try:
        data = json.loads(raw) if isinstance(raw, str) else list(raw)
    except (TypeError, ValueError):
        data = []
    return [p for p in data if isinstance(p, str)]


def add_recent_project(path: str) -> None:
    updated = dedup_cap_recent(load_recent_projects(), path)
    _settings().setValue(_RECENT_KEY, json.dumps(updated))


def remove_recent_project(path: str) -> None:
    updated = [p for p in load_recent_projects() if p != path]
    _settings().setValue(_RECENT_KEY, json.dumps(updated))


def should_show_at_startup() -> bool:
    value = _settings().value(_SHOW_AT_STARTUP_KEY, True)
    if isinstance(value, str):
        return value.lower() not in ("false", "0", "")
    return bool(value)


def set_show_at_startup(value: bool) -> None:
    _settings().setValue(_SHOW_AT_STARTUP_KEY, bool(value))


class NewProjectWizard(QDialog):
    """Two-page guided flow: pick a source, then confirm output + preset.

    Does not re-implement the full editor as a wizard -- artwork, theme,
    dashboard layout, chapters, etc. all stay exactly where they already
    live, in the existing tabbed editor. This just gets a first-time
    user from "I have a video" to a populated Project tab in two steps.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("New Project")
        self.resize(480, 280)
        self.source_path: str = ""
        self.output_path: str = ""
        self.preset_name: str = PRESET_NAMES[0]

        self.stack = QStackedWidget()
        self.stack.addWidget(self._build_source_page())
        self.stack.addWidget(self._build_output_page())

        self.back_btn = QPushButton("Back")
        self.next_btn = QPushButton("Next")
        self.finish_btn = QPushButton("Finish")
        cancel_btn = QPushButton("Cancel")
        self.back_btn.clicked.connect(self._go_back)
        self.next_btn.clicked.connect(self._go_next)
        self.finish_btn.clicked.connect(self._finish)
        cancel_btn.clicked.connect(self.reject)

        nav = QHBoxLayout()
        nav.addWidget(self.back_btn)
        nav.addStretch(1)
        nav.addWidget(cancel_btn)
        nav.addWidget(self.next_btn)
        nav.addWidget(self.finish_btn)

        layout = QVBoxLayout(self)
        layout.addWidget(self.stack)
        layout.addLayout(nav)
        self._update_nav()

    def _build_source_page(self) -> QWidget:
        w = QWidget()
        form = QFormLayout(w)
        intro = QLabel("Choose the source video you want to turn into a .mivf.")
        intro.setWordWrap(True)
        form.addRow(intro)
        self.source_edit = QLineEdit()
        self.source_edit.textChanged.connect(self._update_nav)
        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_source)
        row = QWidget()
        row_layout = QHBoxLayout(row)
        row_layout.setContentsMargins(0, 0, 0, 0)
        row_layout.addWidget(self.source_edit)
        row_layout.addWidget(browse_btn)
        form.addRow("Source media:", row)
        return w

    def _build_output_page(self) -> QWidget:
        w = QWidget()
        form = QFormLayout(w)
        self.output_edit = QLineEdit()
        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_output)
        row = QWidget()
        row_layout = QHBoxLayout(row)
        row_layout.setContentsMargins(0, 0, 0, 0)
        row_layout.addWidget(self.output_edit)
        row_layout.addWidget(browse_btn)
        form.addRow("Output .mivf:", row)

        self.preset_combo = QComboBox()
        for name in PRESET_NAMES:
            self.preset_combo.addItem(PRESET_LABELS[name], name)
        form.addRow("Basic preset:", self.preset_combo)
        self.preset_desc = QLabel(PRESET_DESCRIPTIONS[PRESET_NAMES[0]])
        self.preset_desc.setWordWrap(True)
        self.preset_combo.currentIndexChanged.connect(self._update_preset_desc)
        form.addRow("", self.preset_desc)
        return w

    def _update_preset_desc(self):
        self.preset_desc.setText(PRESET_DESCRIPTIONS.get(self.preset_combo.currentData(), ""))

    def _browse_source(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select source media")
        if not path:
            return
        self.source_edit.setText(path)
        if not self.output_edit.text().strip():
            self.output_edit.setText(str(Path(path).with_suffix(".mivf")))

    def _browse_output(self):
        path, _ = QFileDialog.getSaveFileName(self, "Select output .mivf", "", "MIVF (*.mivf)")
        if path:
            self.output_edit.setText(path)

    def _go_back(self):
        self.stack.setCurrentIndex(max(0, self.stack.currentIndex() - 1))
        self._update_nav()

    def _go_next(self):
        self.stack.setCurrentIndex(min(1, self.stack.currentIndex() + 1))
        self._update_nav()

    def _update_nav(self):
        idx = self.stack.currentIndex()
        self.back_btn.setEnabled(idx > 0)
        self.next_btn.setVisible(idx == 0)
        self.next_btn.setEnabled(bool(self.source_edit.text().strip()))
        self.finish_btn.setVisible(idx == 1)

    def _finish(self):
        source = self.source_edit.text().strip()
        output = self.output_edit.text().strip()
        if not source:
            QMessageBox.warning(self, "Source required", "Choose a source media file first.")
            self.stack.setCurrentIndex(0)
            self._update_nav()
            return
        if not output:
            QMessageBox.warning(self, "Output required", "Choose where to save the .mivf output.")
            return
        self.source_path = source
        self.output_path = output
        self.preset_name = self.preset_combo.currentData()
        self.accept()


class ProjectHomeDialog(QDialog):
    """Startup landing dialog. Every action here delegates to logic that
    either already exists on MainWindow (new/open) or is purely
    additive (recent list, guided wizard) -- Project Home never
    reimplements project creation or loading itself."""

    NEW = "new"
    OPEN = "open"
    RECENT = "recent"
    SKIP = "skip"

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("MIVF Toolkit")
        self.resize(420, 420)
        self.result_action: str | None = None
        self.recent_path: str | None = None

        layout = QVBoxLayout(self)
        title = QLabel("MIVF Toolkit")
        title.setStyleSheet("font-size: 18px; font-weight: bold;")
        layout.addWidget(title)
        subtitle = QLabel("Create a new movie project, or pick up where you left off.")
        subtitle.setWordWrap(True)
        layout.addWidget(subtitle)

        new_btn = QPushButton("New Project (guided)")
        open_btn = QPushButton("Open Existing Project...")
        new_btn.clicked.connect(self._choose_new)
        open_btn.clicked.connect(self._choose_open)
        layout.addWidget(new_btn)
        layout.addWidget(open_btn)

        layout.addWidget(QLabel("Recent projects (double-click to open):"))
        self.recent_list = QListWidget()
        recents = load_recent_projects()
        for p in recents:
            label = p if Path(p).exists() else f"{p}  (missing)"
            item = QListWidgetItem(label)
            item.setData(Qt.UserRole, p)
            self.recent_list.addItem(item)
        if not recents:
            placeholder = QListWidgetItem("(none yet)")
            placeholder.setFlags(Qt.NoItemFlags)
            self.recent_list.addItem(placeholder)
        self.recent_list.itemDoubleClicked.connect(self._choose_recent)
        layout.addWidget(self.recent_list)

        self.startup_checkbox = QCheckBox("Show this screen at startup")
        self.startup_checkbox.setChecked(should_show_at_startup())
        self.startup_checkbox.toggled.connect(set_show_at_startup)
        layout.addWidget(self.startup_checkbox)

        skip_btn = QPushButton("Skip — go straight to the editor")
        skip_btn.clicked.connect(self._choose_skip)
        layout.addWidget(skip_btn)

    def _choose_new(self):
        self.result_action = self.NEW
        self.accept()

    def _choose_open(self):
        self.result_action = self.OPEN
        self.accept()

    def _choose_recent(self, item: QListWidgetItem):
        path = item.data(Qt.UserRole)
        if not path:
            return
        if not Path(path).exists():
            QMessageBox.warning(
                self, "Project not found",
                f"{path}\n\nno longer exists; removing it from Recent Projects.",
            )
            remove_recent_project(path)
            self.recent_list.takeItem(self.recent_list.row(item))
            return
        self.result_action = self.RECENT
        self.recent_path = path
        self.accept()

    def _choose_skip(self):
        self.result_action = self.SKIP
        self.accept()
