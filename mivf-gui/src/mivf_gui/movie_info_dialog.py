"""E.1.2: Movie Information Authoring dialog. Same working-copy discipline
as ControlArtworkDialog: edits happen on a deep-copied MovieInformation,
project.movie_info is only written on Apply, and Cancel leaves the real
project untouched."""
from __future__ import annotations

import copy
from pathlib import Path

from PySide6.QtWidgets import (QDialog, QDialogButtonBox, QFormLayout, QHBoxLayout,
    QLabel, QLineEdit, QMessageBox, QPlainTextEdit, QPushButton, QScrollArea,
    QTabWidget, QVBoxLayout, QWidget)

from . import movie_info as mi
from .media_probe import MediaProbeError, probe_media_cached

SHORT_FIELDS = (
    ("title", "Title"), ("original_title", "Original Title"), ("release_year", "Release Year"),
    ("displayed_runtime", "Displayed Runtime"), ("genre", "Genre"), ("content_rating", "Content Rating"),
    ("director", "Director"), ("production_studio", "Production Studio"),
    ("languages", "Languages"), ("edition_name", "Edition Name"),
)
LONG_FIELDS = (
    ("synopsis", "Synopsis (the one field the real player can show, 38 characters max)"),
    ("short_synopsis", "Short Synopsis (desktop-only, no player counterpart)"),
    ("cast_text", "Cast"),
    ("custom_notes", "Notes (desktop-only)"),
)
ALL_FIELDS = SHORT_FIELDS + LONG_FIELDS


class MovieInfoDialog(QDialog):
    def __init__(self, project, parent=None):
        super().__init__(parent)
        self.p = project
        self.work = copy.deepcopy(project.movie_info)
        self.loading = False
        self.setWindowTitle("Movie Information")
        self.resize(820, 640)

        root = QVBoxLayout(self)
        note = QLabel(
            "Desktop authoring only. The real player currently shows just one field here -- "
            "Synopsis, up to 38 characters, from an optional \".nfo\" sidecar the Toolkit does not "
            "yet write. Every other field is for your own reference and future export/sharing; "
            "none of it appears in the player today."
        )
        note.setWordWrap(True)
        root.addWidget(note)

        tabs = QTabWidget()
        root.addWidget(tabs, 1)

        form_scroll = QScrollArea()
        form_scroll.setWidgetResizable(True)
        form_widget = QWidget()
        form = QFormLayout(form_widget)
        self.widgets: dict[str, QLineEdit | QPlainTextEdit] = {}
        self.reset_buttons: dict[str, QPushButton] = {}
        for field_name, label in SHORT_FIELDS:
            edit = QLineEdit()
            edit.textChanged.connect(lambda _t, f=field_name: self._on_field_changed(f))
            self.widgets[field_name] = edit
            form.addRow(label, self._row_with_reset(field_name, edit))
        for field_name, label in LONG_FIELDS:
            edit = QPlainTextEdit()
            edit.setFixedHeight(70)
            edit.textChanged.connect(lambda f=field_name: self._on_field_changed(f))
            self.widgets[field_name] = edit
            form.addRow(label, self._row_with_reset(field_name, edit))
        form_scroll.setWidget(form_widget)
        tabs.addTab(form_scroll, "Edit")

        import_row = QHBoxLayout()
        self.import_btn = QPushButton("Import from Source")
        self.import_btn.clicked.connect(self._import_from_source)
        import_row.addWidget(self.import_btn)
        import_row.addStretch(1)
        import_container = QWidget()
        import_container.setLayout(import_row)
        form.addRow(import_container)
        self.import_status = QLabel("")
        self.import_status.setWordWrap(True)
        form.addRow(self.import_status)

        preview_tab = QWidget()
        preview_layout = QVBoxLayout(preview_tab)
        preview_layout.addWidget(QLabel("Concise preview"))
        self.concise_view = QPlainTextEdit()
        self.concise_view.setReadOnly(True)
        self.concise_view.setFixedHeight(50)
        preview_layout.addWidget(self.concise_view)
        preview_layout.addWidget(QLabel("Detailed preview"))
        self.detailed_view = QPlainTextEdit()
        self.detailed_view.setReadOnly(True)
        preview_layout.addWidget(self.detailed_view, 1)
        preview_layout.addWidget(QLabel("Real player synopsis preview (browser panel, if exported)"))
        self.synopsis_view = QLabel("")
        self.synopsis_view.setWordWrap(True)
        self.synopsis_view.setStyleSheet("background:#111; color:#ddd; padding:6px;")
        preview_layout.addWidget(self.synopsis_view)
        self.technical_view = QLabel("")
        self.technical_view.setWordWrap(True)
        preview_layout.addWidget(QLabel("Technical facts (from MediaProbe -- never edited here)"))
        preview_layout.addWidget(self.technical_view)
        tabs.addTab(preview_tab, "Preview")

        buttons = QDialogButtonBox(QDialogButtonBox.Apply | QDialogButtonBox.Cancel)
        buttons.button(QDialogButtonBox.Apply).clicked.connect(self.apply)
        buttons.rejected.connect(self.reject)
        root.addWidget(buttons)

        self._load()

    def _row_with_reset(self, field_name: str, widget) -> QWidget:
        row = QHBoxLayout()
        row.addWidget(widget, 1)
        reset_btn = QPushButton("Reset to source")
        reset_btn.setToolTip("Restore this field to the value last imported from the source file's own metadata.")
        reset_btn.clicked.connect(lambda _=False, f=field_name: self._reset_field(f))
        self.reset_buttons[field_name] = reset_btn
        row.addWidget(reset_btn)
        w = QWidget()
        w.setLayout(row)
        return w

    def _load(self):
        self.loading = True
        for field_name, _ in ALL_FIELDS:
            widget = self.widgets[field_name]
            value = getattr(self.work, field_name) or ""
            if isinstance(widget, QPlainTextEdit):
                widget.setPlainText(value)
            else:
                widget.setText(value)
        self.loading = False
        self._refresh_previews()

    def _on_field_changed(self, field_name: str):
        if self.loading:
            return
        widget = self.widgets[field_name]
        text = widget.toPlainText() if isinstance(widget, QPlainTextEdit) else widget.text()
        setattr(self.work, field_name, text or None)
        mi.mark_manual(self.work, field_name)
        self._refresh_previews()

    def _reset_field(self, field_name: str):
        if not mi.reset_field_to_probed(self.work, field_name):
            self.import_status.setText(f"No source value available for \"{field_name.replace('_', ' ')}\" -- nothing to reset to.")
            return
        self._load()

    def _import_from_source(self):
        source = self.p.source_media
        if not source:
            QMessageBox.information(self, "No source set", "Set this project's source media before importing metadata.")
            return
        resolved = self.p.resolve(source)
        try:
            probe = probe_media_cached(resolved)
        except MediaProbeError as e:
            QMessageBox.warning(self, "Import failed", f"Could not read metadata from the source file:\n\n{e}")
            return
        populated = mi.import_from_probe(self.work, probe, overwrite=False)
        self._load()
        if populated:
            self.import_status.setText("Imported from source: " + ", ".join(f.replace("_", " ") for f in populated))
        else:
            self.import_status.setText(
                "This source has no embedded metadata tags matching any field here "
                "(or every matching field was already hand-edited)."
            )

    def _refresh_previews(self):
        self.concise_view.setPlainText(mi.concise_summary(self.work))
        self.detailed_view.setPlainText(mi.detailed_summary(self.work))
        syn = mi.synopsis_preview(self.work.synopsis)
        lines = f"{syn['line1']}\n{syn['line2']}"
        if syn["truncated"]:
            lines += f"\n(truncated -- {syn['collapsed_length']} characters collapsed, only the first {syn['max_length']} would show)"
        self.synopsis_view.setText(lines or "(no synopsis entered)")
        self._refresh_technical_facts()

    def _refresh_technical_facts(self):
        source = self.p.source_media
        if not source:
            self.technical_view.setText("(no source media set)")
            return
        resolved = self.p.resolve(source)
        if not resolved or not Path(resolved).is_file():
            self.technical_view.setText(f"(source file not found: {resolved})")
            return
        try:
            probe = probe_media_cached(resolved, compute_hash=False)
        except MediaProbeError as e:
            self.technical_view.setText(f"(could not probe source: {e})")
            return
        bits = []
        if probe.duration_seconds:
            bits.append(f"Duration: {probe.duration_seconds:.1f}s")
        if probe.video_streams:
            v = probe.video_streams[0]
            bits.append(f"Video: {v.codec} {v.width}x{v.height}")
        if probe.audio_streams:
            a = probe.audio_streams[0]
            bits.append(f"Audio: {a.codec} {a.channels}ch {a.sample_rate}Hz")
        if probe.chapters:
            bits.append(f"Chapters: {len(probe.chapters)}")
        if probe.subtitle_streams:
            langs = ", ".join(s.language or "?" for s in probe.subtitle_streams)
            bits.append(f"Subtitle streams: {len(probe.subtitle_streams)} ({langs})")
        self.technical_view.setText(" | ".join(bits) if bits else "(no technical facts available)")

    def apply(self):
        self.p.movie_info = copy.deepcopy(self.work)
        self.accept()
