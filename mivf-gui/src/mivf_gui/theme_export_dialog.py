from __future__ import annotations

from pathlib import Path
from PySide6.QtCore import QThread, Signal
from PySide6.QtWidgets import (QDialog, QFileDialog, QFormLayout, QHBoxLayout,
    QLabel, QLineEdit, QMessageBox, QPlainTextEdit, QPushButton, QVBoxLayout)
from .theme_export import export_theme_package, format_report

class _Worker(QThread):
    succeeded = Signal(str)
    failed = Signal(str)
    def __init__(self, project: Path, destination: Path, basename: str, parent=None):
        super().__init__(parent); self.project=project; self.destination=destination; self.basename=basename
    def run(self):
        try: self.succeeded.emit(format_report(export_theme_package(self.project,self.destination,self.basename)))
        except Exception as exc: self.failed.emit(str(exc))

class ThemeExportDialog(QDialog):
    def __init__(self, parent=None, project_path: str = ""):
        super().__init__(parent); self.worker=None
        self.setWindowTitle("Export Runtime Theme Package"); self.resize(760,480)
        root=QVBoxLayout(self); form=QFormLayout(); root.addLayout(form)
        self.project=QLineEdit(project_path); self.destination=QLineEdit(); self.basename=QLineEdit()
        form.addRow("Saved .mivfproj", self._picker(self.project, self.pick_project, "Browse"))
        form.addRow("Destination folder", self._picker(self.destination, self.pick_destination, "Browse"))
        form.addRow("Output basename", self.basename)
        root.addWidget(QLabel("Exports the manifest plus dashboard, Rewind, Play/Pause, Fast Forward, and movie-menu Back assets. Existing valid files are preserved if generation fails."))
        self.log=QPlainTextEdit(); self.log.setReadOnly(True); root.addWidget(self.log,1)
        buttons=QHBoxLayout(); self.export=QPushButton("Export Runtime Theme Package"); self.close=QPushButton("Close")
        buttons.addStretch(1); buttons.addWidget(self.export); buttons.addWidget(self.close); root.addLayout(buttons)
        self.export.clicked.connect(self.start); self.close.clicked.connect(self.reject)
        if project_path:
            p=Path(project_path); self.destination.setText(str(p.parent)); self.basename.setText(p.stem)
    def _picker(self, edit, fn, label):
        w=QHBoxLayout(); w.addWidget(edit,1); b=QPushButton(label); b.clicked.connect(fn); w.addWidget(b)
        from PySide6.QtWidgets import QWidget
        c=QWidget(); c.setLayout(w); return c
    def pick_project(self):
        p,_=QFileDialog.getOpenFileName(self,"Select MIVF project",self.project.text(),"MIVF project (*.mivfproj)")
        if p: self.project.setText(p); path=Path(p); self.destination.setText(str(path.parent)); self.basename.setText(path.stem)
    def pick_destination(self):
        p=QFileDialog.getExistingDirectory(self,"Select package destination",self.destination.text())
        if p: self.destination.setText(p)
    def start(self):
        p=Path(self.project.text().strip()); d=Path(self.destination.text().strip()); b=self.basename.text().strip() or p.stem
        if not p.is_file() or not d:
            QMessageBox.warning(self,"Export blocked","Select an existing saved .mivfproj and a destination folder."); return
        self.export.setEnabled(False); self.log.setPlainText("Validating and exporting...")
        self.worker=_Worker(p,d,b,self); self.worker.succeeded.connect(self.done_ok); self.worker.failed.connect(self.done_bad); self.worker.start()
    def done_ok(self, report):
        self.export.setEnabled(True); self.log.setPlainText(report); QMessageBox.information(self,"Export complete","The runtime theme package was exported and validated.")
    def done_bad(self, message):
        self.export.setEnabled(True); self.log.setPlainText(message); QMessageBox.critical(self,"Export failed",message+"\n\nNo existing valid package was intentionally removed.")