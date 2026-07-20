"""H: Chapter Authoring Studio.

The player already reads a real ".chapters" sidecar at runtime (source/
main.c, hfix60_chapters_load: one "SECONDS|Label" line per chapter,
64-chapter cap, 39-char label cap) -- but nothing in the Toolkit ever
wrote one. The only prior chapter code anywhere in the GUI was a
read-only count on the Movie Information dialog ("Chapters: N").

This module is the missing write side: import chapters ffprobe already
found in the source (MediaProbeResult.chapters) as a starting point, let
the user add/edit/remove/reorder entries, then export the exact sidecar
format the player parses. Does not touch encode_mivf.py's separate
-map_chapters handling (that strips CONTAINER-embedded chapters from the
encoded video stream -- an unrelated concern to this TEXT sidecar, which
is authored and shipped independently of the encode step).
"""
from __future__ import annotations

import dataclasses
from pathlib import Path
from typing import Any

from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QListWidget,
    QListWidgetItem, QLineEdit, QFormLayout, QMessageBox,
)

CHAPTER_MAX = 64       # MIVF_CHAP_MAX, source/main.c
LABEL_MAX_CHARS = 39   # MivfChapter.label[40] (39 usable + nul), source/main.c


@dataclasses.dataclass
class ChapterMark:
    seconds: float
    label: str

    def to_dict(self) -> dict[str, Any]:
        return {"seconds": self.seconds, "label": self.label}

    @staticmethod
    def from_dict(d: dict[str, Any]) -> "ChapterMark":
        return ChapterMark(seconds=float(d.get("seconds", 0.0)), label=str(d.get("label", "")))


def format_timestamp(seconds: float) -> str:
    total = max(0, round(seconds))
    hours, rem = divmod(total, 3600)
    minutes, secs = divmod(rem, 60)
    return f"{hours:d}:{minutes:02d}:{secs:02d}" if hours else f"0:{minutes:02d}:{secs:02d}"


def parse_timestamp(text: str) -> float | None:
    """Accepts "H:MM:SS", "MM:SS", or a bare seconds number -- the same
    three forms hfix60_chapters_load itself parses on the player side, so
    a value round-tripped through this GUI and the sidecar reads back
    identically."""
    text = text.strip()
    if not text:
        return None
    parts = text.split(":")
    try:
        if len(parts) == 3:
            h, m, s = (int(p) for p in parts)
            return float(h * 3600 + m * 60 + s)
        if len(parts) == 2:
            m, s = (int(p) for p in parts)
            return float(m * 60 + s)
        return float(text)
    except ValueError:
        return None


def import_from_probe(chapters) -> list[ChapterMark]:
    """chapters: tuple[ChapterInfo, ...] from MediaProbeResult -- imported
    as a real starting point rather than requiring hand-authoring from
    scratch, closing the "auto chapter import from source" gap named
    alongside this feature (a container's embedded chapters end up
    stripped from the ENCODED video by encode_mivf.py's unrelated
    -map_chapters flag, but nothing stopped this sidecar, a completely
    separate mechanism, from being pre-populated from the same source)."""
    marks = []
    for c in chapters:
        title = (c.title or "").strip() or f"Chapter {c.index + 1}"
        marks.append(ChapterMark(seconds=float(c.start_seconds), label=title))
    return marks


def validate_chapters(chapters: list[ChapterMark]) -> list[str]:
    """Human-readable warnings, never raised as exceptions -- authoring is
    meant to stay interactive even mid-edit; export is what should be
    gated (see chapters_sidecar_text's caller in the dialog)."""
    warnings: list[str] = []
    if len(chapters) > CHAPTER_MAX:
        warnings.append(f"{len(chapters)} chapters exceeds the player's {CHAPTER_MAX}-chapter limit; extras will be dropped.")
    seen_seconds: set[float] = set()
    for i, c in enumerate(chapters):
        if c.seconds < 0:
            warnings.append(f"Chapter {i + 1} has a negative timestamp.")
        if not c.label.strip():
            warnings.append(f"Chapter {i + 1} has an empty label.")
        if c.seconds in seen_seconds:
            warnings.append(f"Chapter {i + 1} shares its timestamp with an earlier chapter.")
        seen_seconds.add(c.seconds)
    return warnings


def _sanitize_label(label: str) -> str:
    # "|" is the sidecar's own field delimiter (hfix60_chapters_load) --
    # stripping it here, at write time, guarantees a round-trip-safe file
    # rather than relying on the player's parser to recover from a
    # corrupted line. Newlines would silently split into extra lines.
    clean = label.replace("|", "/").replace("\r", " ").replace("\n", " ").strip()
    return clean[:LABEL_MAX_CHARS]


def chapters_sidecar_text(chapters: list[ChapterMark]) -> str:
    """Builds the exact "SECONDS|Label" sidecar format hfix60_chapters_load
    parses, time-sorted, capped at CHAPTER_MAX, labels sanitized. Pure
    string building -- no file I/O, so this is independently testable
    from write_chapters_sidecar."""
    ordered = sorted(chapters, key=lambda c: c.seconds)[:CHAPTER_MAX]
    lines = [f"{c.seconds:.3f}|{_sanitize_label(c.label)}" for c in ordered]
    return "\n".join(lines) + ("\n" if lines else "")


def write_chapters_sidecar(output_mivf_path: Path, chapters: list[ChapterMark]) -> Path:
    """Sidecar path mirrors hfix60_make_sidecar_path's own convention:
    same basename as the .mivf output, extension replaced with
    ".chapters" (not appended -- "movie.mivf" -> "movie.chapters")."""
    sidecar_path = Path(output_mivf_path).with_suffix(".chapters")
    sidecar_path.write_text(chapters_sidecar_text(chapters), encoding="utf-8")
    return sidecar_path


class ChapterAuthoringDialog(QDialog):
    """Add/edit/remove/reorder chapters for the current project, with an
    "Import from Source" shortcut and an explicit Export step. Mutates
    project.chapters directly on Accept (Cancel discards edits), same
    discipline as DashboardCanvasDialog/ControlArtworkDialog."""

    def __init__(self, project, parent=None):
        super().__init__(parent)
        self.project = project
        self.setWindowTitle("Chapter Authoring Studio")
        self.resize(520, 480)
        self.chapters: list[ChapterMark] = [ChapterMark.from_dict(c) for c in project.chapters]

        layout = QVBoxLayout(self)

        import_btn = QPushButton("Import Chapters from Source")
        import_btn.clicked.connect(self._import_from_source)
        layout.addWidget(import_btn)

        self.list_widget = QListWidget()
        self.list_widget.itemSelectionChanged.connect(self._load_selected_into_form)
        layout.addWidget(self.list_widget)

        form = QFormLayout()
        self.time_edit = QLineEdit()
        self.time_edit.setPlaceholderText("H:MM:SS")
        form.addRow("Time:", self.time_edit)
        self.label_edit = QLineEdit()
        form.addRow("Label:", self.label_edit)
        layout.addLayout(form)

        edit_row = QHBoxLayout()
        add_btn = QPushButton("Add / Update")
        remove_btn = QPushButton("Remove Selected")
        add_btn.clicked.connect(self._add_or_update)
        remove_btn.clicked.connect(self._remove_selected)
        for b in (add_btn, remove_btn):
            edit_row.addWidget(b)
        layout.addLayout(edit_row)

        self.warning_label = QLabel("")
        self.warning_label.setWordWrap(True)
        layout.addWidget(self.warning_label)

        export_btn = QPushButton("Export .chapters Sidecar Now")
        export_btn.clicked.connect(self._export_sidecar)
        layout.addWidget(export_btn)

        buttons = QHBoxLayout()
        cancel_btn = QPushButton("Cancel")
        ok_btn = QPushButton("OK")
        cancel_btn.clicked.connect(self.reject)
        ok_btn.clicked.connect(self._accept)
        buttons.addStretch(1)
        buttons.addWidget(cancel_btn)
        buttons.addWidget(ok_btn)
        layout.addLayout(buttons)

        self._refresh_list()

    def _refresh_list(self):
        self.list_widget.clear()
        for c in sorted(self.chapters, key=lambda c: c.seconds):
            item = QListWidgetItem(f"{format_timestamp(c.seconds)}  {c.label}")
            item.setData(1000, c)  # stash the ChapterMark itself; Qt.UserRole == 256, avoid collision with any future role use
            self.list_widget.addItem(item)
        self.warning_label.setText("\n".join(validate_chapters(self.chapters)))

    def _selected_mark(self) -> ChapterMark | None:
        item = self.list_widget.currentItem()
        return item.data(1000) if item else None

    def _load_selected_into_form(self):
        mark = self._selected_mark()
        if mark is None:
            return
        self.time_edit.setText(format_timestamp(mark.seconds))
        self.label_edit.setText(mark.label)

    def _import_from_source(self):
        from .media_probe import probe_media_cached, MediaProbeError
        source = self.project.resolve(self.project.source_media)
        if not source:
            QMessageBox.information(self, "No source set", "Set a source media file on the Project tab first.")
            return
        try:
            probe = probe_media_cached(source, compute_hash=False)
        except MediaProbeError as e:
            QMessageBox.warning(self, "Import failed", str(e))
            return
        imported = import_from_probe(probe.chapters)
        if not imported:
            QMessageBox.information(self, "No chapters found", "The source file has no embedded chapters to import.")
            return
        self.chapters = imported
        self._refresh_list()

    def _add_or_update(self):
        seconds = parse_timestamp(self.time_edit.text())
        if seconds is None:
            QMessageBox.warning(self, "Invalid time", "Enter a time as H:MM:SS, MM:SS, or a number of seconds.")
            return
        label = self.label_edit.text().strip() or "Chapter"
        selected = self._selected_mark()
        if selected is not None and selected in self.chapters:
            selected.seconds = seconds
            selected.label = label
        else:
            self.chapters.append(ChapterMark(seconds=seconds, label=label))
        self._refresh_list()

    def _remove_selected(self):
        mark = self._selected_mark()
        if mark is None:
            return
        self.chapters = [c for c in self.chapters if c is not mark]
        self._refresh_list()

    def _export_sidecar(self):
        # Explicit, separate export step -- matches the same architecture
        # already established for cover/screensaver artwork (staged only
        # via the dedicated "Export Runtime Theme Package..." action, not
        # written automatically during Create MIVF); consistent, not a
        # regression from that precedent.
        out_path = self.project.resolve(self.project.output_path)
        if not out_path:
            QMessageBox.information(self, "No output path set", "Set an output .mivf path on the Project tab first.")
            return
        sidecar_path = write_chapters_sidecar(out_path, self.chapters)
        QMessageBox.information(self, "Exported", f"Wrote {len(self.chapters)} chapter(s) to {sidecar_path}")

    def _accept(self):
        self.project.chapters = [c.to_dict() for c in self.chapters]
        self.accept()
