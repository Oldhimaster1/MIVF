"""Make My Theme wizard: a guided path through the existing accent/
outline/back-fill/background theme fields already on the Artwork & Theme
tab, for a first-time user who doesn't want to learn every control there
up front.

Deliberately thin: collects the same four fields _pick_accent/
_pick_outline/_pick_back_fill/the background row already write, then
hands off to MainWindow's own existing apply logic and lands on the
Artwork & Theme tab for further fine-tuning -- exactly the same "guided
front door, then the real editor" pattern project_home.py's
NewProjectWizard already established. Never a parallel theme
representation; the wizard's result is applied through the identical
accent_swatch/outline_swatch/back_fill_swatch/back_fill_enabled/bg_row
state the manual controls use.

QUICK_PRESETS reuses the exact 4 RGB triples the in-player theme picker
already cycles through on KEY_Y (source/main.c, mivf_theme_picker_input)
-- not reinvented, so a preset picked here and the player's own quick-
preset cycle stay visually consistent.
"""
from __future__ import annotations

from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QCheckBox,
    QStackedWidget, QWidget, QLineEdit, QFileDialog, QColorDialog, QFormLayout,
)

QUICK_PRESETS: tuple[tuple[str, tuple[int, int, int]], ...] = (
    ("Ocean", (70, 120, 210)),
    ("Emerald", (0, 170, 95)),
    ("Violet", (150, 80, 220)),
    ("Amber", (235, 140, 40)),
)


def _swatch_style(color: QColor) -> str:
    return f"background-color: rgb({color.red()},{color.green()},{color.blue()}); border: 1px solid #333;"


class ThemeWizard(QDialog):
    """Three-page guided flow: accent + outline, optional back-fill +
    background, summary + Finish."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Make My Theme")
        self.resize(480, 320)

        self.accent_color = QColor(70, 120, 210)
        self.outline_color = QColor(255, 255, 255)
        self.back_fill_color = QColor(200, 60, 60)
        self.back_fill_enabled = False
        self.background_path: str = ""

        self.stack = QStackedWidget()
        self.stack.addWidget(self._build_colors_page())
        self.stack.addWidget(self._build_extras_page())
        self.stack.addWidget(self._build_summary_page())

        self.back_btn = QPushButton("Back")
        self.next_btn = QPushButton("Next")
        self.finish_btn = QPushButton("Finish")
        cancel_btn = QPushButton("Cancel")
        self.back_btn.clicked.connect(self._go_back)
        self.next_btn.clicked.connect(self._go_next)
        self.finish_btn.clicked.connect(self.accept)
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

    # ---- page 1: accent + outline -----------------------------------

    def _build_colors_page(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)
        intro = QLabel("Pick a quick preset, or choose your own accent and outline colors.")
        intro.setWordWrap(True)
        layout.addWidget(intro)

        presets_row = QHBoxLayout()
        for name, rgb in QUICK_PRESETS:
            btn = QPushButton(name)
            btn.setStyleSheet(_swatch_style(QColor(*rgb)))
            btn.clicked.connect(lambda _checked, rgb=rgb: self._apply_preset(rgb))
            presets_row.addWidget(btn)
        layout.addLayout(presets_row)

        form = QFormLayout()
        self.accent_btn = QPushButton("Choose accent color...")
        self.accent_btn.clicked.connect(self._pick_accent)
        self._update_accent_button()
        form.addRow("Accent:", self.accent_btn)

        self.outline_btn = QPushButton("Choose outline color...")
        self.outline_btn.clicked.connect(self._pick_outline)
        self._update_outline_button()
        form.addRow("Outline:", self.outline_btn)
        layout.addLayout(form)
        return w

    def _apply_preset(self, rgb: tuple[int, int, int]):
        self.accent_color = QColor(*rgb)
        self._update_accent_button()

    def _pick_accent(self):
        color = QColorDialog.getColor(self.accent_color, self, "Choose accent color")
        if color.isValid():
            self.accent_color = color
            self._update_accent_button()

    def _pick_outline(self):
        color = QColorDialog.getColor(self.outline_color, self, "Choose outline color")
        if color.isValid():
            self.outline_color = color
            self._update_outline_button()

    def _update_accent_button(self):
        self.accent_btn.setStyleSheet(_swatch_style(self.accent_color))

    def _update_outline_button(self):
        self.outline_btn.setStyleSheet(_swatch_style(self.outline_color))

    # ---- page 2: optional back-fill + background ----------------------

    def _build_extras_page(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)
        intro = QLabel("Both of these are optional -- skip either and the built-in appearance is used.")
        intro.setWordWrap(True)
        layout.addWidget(intro)

        form = QFormLayout()
        self.back_fill_checkbox = QCheckBox("Override the Back row's fill color")
        self.back_fill_checkbox.toggled.connect(self._toggle_back_fill)
        form.addRow(self.back_fill_checkbox)
        self.back_fill_btn = QPushButton("Choose Back fill color...")
        self.back_fill_btn.setEnabled(False)
        self.back_fill_btn.clicked.connect(self._pick_back_fill)
        self._update_back_fill_button()
        form.addRow("Back fill:", self.back_fill_btn)

        self.background_edit = QLineEdit()
        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_background)
        row = QWidget()
        row_layout = QHBoxLayout(row)
        row_layout.setContentsMargins(0, 0, 0, 0)
        row_layout.addWidget(self.background_edit)
        row_layout.addWidget(browse_btn)
        form.addRow("Dashboard background image:", row)
        layout.addLayout(form)
        return w

    def _toggle_back_fill(self, checked: bool):
        self.back_fill_enabled = checked
        self.back_fill_btn.setEnabled(checked)

    def _pick_back_fill(self):
        color = QColorDialog.getColor(self.back_fill_color, self, "Choose Back fill color")
        if color.isValid():
            self.back_fill_color = color
            self._update_back_fill_button()

    def _update_back_fill_button(self):
        self.back_fill_btn.setStyleSheet(_swatch_style(self.back_fill_color))

    def _browse_background(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select dashboard background image")
        if path:
            self.background_edit.setText(path)

    # ---- page 3: summary -----------------------------------------------

    def _build_summary_page(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)
        self.summary_label = QLabel()
        self.summary_label.setWordWrap(True)
        layout.addWidget(self.summary_label)
        return w

    def _refresh_summary(self):
        self.background_path = self.background_edit.text().strip()
        lines = [
            f"Accent: rgb({self.accent_color.red()},{self.accent_color.green()},{self.accent_color.blue()})",
            f"Outline: rgb({self.outline_color.red()},{self.outline_color.green()},{self.outline_color.blue()})",
        ]
        if self.back_fill_enabled:
            lines.append(
                f"Back fill: rgb({self.back_fill_color.red()},{self.back_fill_color.green()},{self.back_fill_color.blue()})"
            )
        else:
            lines.append("Back fill: built-in (unchanged)")
        lines.append(f"Dashboard background: {self.background_path or 'built-in (unchanged)'}")
        lines.append("")
        lines.append("Finish to apply these to the Artwork && Theme tab, where you can keep refining them.")
        self.summary_label.setText("\n".join(lines))

    # ---- navigation ------------------------------------------------------

    def _go_back(self):
        self.stack.setCurrentIndex(max(0, self.stack.currentIndex() - 1))
        self._update_nav()

    def _go_next(self):
        self.stack.setCurrentIndex(min(2, self.stack.currentIndex() + 1))
        if self.stack.currentIndex() == 2:
            self._refresh_summary()
        self._update_nav()

    def _update_nav(self):
        idx = self.stack.currentIndex()
        self.back_btn.setEnabled(idx > 0)
        self.next_btn.setVisible(idx < 2)
        self.finish_btn.setVisible(idx == 2)
