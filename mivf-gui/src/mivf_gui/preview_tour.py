"""Reusable Player/Theme Preview Tour.

Two halves, deliberately kept separate:

- TourStep / PreviewTourDialog: a generic, domain-agnostic step-sequencer
  shell (title + caption + Next/Back/Close, calling each step's apply()
  callback as it becomes current). Knows nothing about themes or the
  player -- this is the "reusable" part, and could drive a future,
  unrelated guided walkthrough (e.g. an onboarding tour) without change.

- build_theme_preview_tour(): the concrete tour used here. Deliberately
  does NOT rebuild or duplicate any preview-rendering logic -- it drives
  MainWindow's own existing, already-correct Preview tab (self.preview /
  self.back_preview / _refresh_preview()) through its real focus states,
  the same states a user could reach by clicking through the tab by
  hand. The dialog just automates stepping through them and switches the
  main window to the Preview tab so the live result is visible while it
  runs. The original focus state is restored on close, so running (or
  cancelling) a tour is never destructive to whatever the user had
  selected before.
"""
from __future__ import annotations

import dataclasses
from typing import Callable

from PySide6.QtWidgets import QDialog, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QTabWidget


@dataclasses.dataclass
class TourStep:
    title: str
    caption: str
    apply: Callable[[], None]


class PreviewTourDialog(QDialog):
    """Generic step-sequencer -- see module docstring. Non-modal-safe: all
    navigation is button-driven, no auto-advance timer, so a host
    application never has to worry about a background timer firing after
    the dialog (or the window it refers to) has gone away."""

    def __init__(self, steps: list[TourStep], parent=None, on_close: Callable[[], None] | None = None):
        super().__init__(parent)
        self.setWindowTitle("Preview Tour")
        self.resize(420, 180)
        self.steps = steps
        self.index = 0
        self._on_close = on_close

        self.title_label = QLabel()
        self.title_label.setStyleSheet("font-weight: bold;")
        self.caption_label = QLabel()
        self.caption_label.setWordWrap(True)
        self.progress_label = QLabel()

        layout = QVBoxLayout(self)
        layout.addWidget(self.title_label)
        layout.addWidget(self.caption_label)
        layout.addWidget(self.progress_label)

        nav = QHBoxLayout()
        self.back_btn = QPushButton("Back")
        self.next_btn = QPushButton("Next")
        close_btn = QPushButton("Close")
        self.back_btn.clicked.connect(self._go_back)
        self.next_btn.clicked.connect(self._go_next)
        close_btn.clicked.connect(self.accept)
        nav.addWidget(self.back_btn)
        nav.addStretch(1)
        nav.addWidget(close_btn)
        nav.addWidget(self.next_btn)
        layout.addLayout(nav)

        if self.steps:
            self._show_current()

    def _show_current(self):
        step = self.steps[self.index]
        step.apply()
        self.title_label.setText(step.title)
        self.caption_label.setText(step.caption)
        self.progress_label.setText(f"Stop {self.index + 1} of {len(self.steps)}")
        self.back_btn.setEnabled(self.index > 0)
        self.next_btn.setEnabled(self.index < len(self.steps) - 1)

    def _go_back(self):
        if self.index > 0:
            self.index -= 1
            self._show_current()

    def _go_next(self):
        if self.index < len(self.steps) - 1:
            self.index += 1
            self._show_current()

    def done(self, result):
        if self._on_close:
            self._on_close()
        super().done(result)


def build_theme_preview_tour(window) -> list[TourStep]:
    """window: a MainWindow instance. Reads/restores window.preview and
    window.back_preview's real focus state; every apply() callback ends
    with window._refresh_preview() so per-state Control Artwork Studio
    recipes (idle vs. focused can differ) are honored exactly as they
    would be for a manually-clicked focus change -- no shortcut that
    could show a state the real Preview tab wouldn't."""
    return [
        TourStep(
            "Rewind — focused",
            "How the Rewind control looks when it has D-pad focus.",
            lambda: (window._set_preview_focus_index(0), window._refresh_preview()),
        ),
        TourStep(
            "Play / Pause — focused",
            "How the Play/Pause control looks when it has D-pad focus (the default landing focus during real playback).",
            lambda: (window._set_preview_focus_index(1), window._refresh_preview()),
        ),
        TourStep(
            "Fast Forward — focused",
            "How the Fast Forward control looks when it has D-pad focus.",
            lambda: (window._set_preview_focus_index(2), window._refresh_preview()),
        ),
        TourStep(
            "Back — focused",
            "How the Movie Menu Back row looks when it has D-pad focus.",
            lambda: (window._set_back_focus(True), window._refresh_preview()),
        ),
        TourStep(
            "Back — idle",
            "How the Movie Menu Back row looks the rest of the time.",
            lambda: (window._set_back_focus(False), window._refresh_preview()),
        ),
    ]


def run_theme_preview_tour(window):
    """Switches to the Preview tab, restores the pre-tour focus state on
    close (Cancel-equivalent discipline -- running a tour never leaves
    the editor in a different state than before it started)."""
    central = window.centralWidget()
    if isinstance(central, QTabWidget):
        central.setCurrentIndex(2)  # "3. Preview"

    original_focus_index = window.preview.focused_index
    original_back_focused = window.back_preview.focused

    def restore():
        window._set_preview_focus_index(original_focus_index)
        window._set_back_focus(original_back_focused)
        window._refresh_preview()

    dialog = PreviewTourDialog(build_theme_preview_tour(window), window, on_close=restore)
    dialog.exec()
