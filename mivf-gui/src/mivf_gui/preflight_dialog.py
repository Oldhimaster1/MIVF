"""Phase C.5a: Check Project / Export Dry Run / Change Summary -- three
views over one theme_plan.PackagePlan, so what this dialog reports can
never disagree with what a real export then does. No player changes; this
dialog never writes into the destination folder."""
from __future__ import annotations

from pathlib import Path
from PySide6.QtCore import QThread, Signal
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (QDialog, QFileDialog, QFormLayout, QHBoxLayout,
    QHeaderView, QLabel, QLineEdit, QMessageBox, QPlainTextEdit, QPushButton,
    QTabWidget, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget)

from . import theme_plan

STATUS_COLOR = {"added": "#2a8a3f", "changed": "#b8860b", "unchanged": "#888", "removed": "#b02020"}


class _Worker(QThread):
    succeeded = Signal(object)
    failed = Signal(str)

    def __init__(self, project: Path, destination: Path, basename: str, parent=None):
        super().__init__(parent)
        self.project, self.destination, self.basename = project, destination, basename

    def run(self):
        try:
            self.succeeded.emit(theme_plan.build_plan(self.project, self.destination, self.basename))
        except Exception as exc:  # noqa: BLE001 -- surfaced to the user, never swallowed
            self.failed.emit(str(exc))


class PreflightDialog(QDialog):
    def __init__(self, parent=None, project_path: str = ""):
        super().__init__(parent)
        self.worker = None
        self.plan: theme_plan.PackagePlan | None = None
        self.setWindowTitle("Check Project")
        self.resize(880, 600)
        root = QVBoxLayout(self)

        form = QFormLayout()
        root.addLayout(form)
        self.project = QLineEdit(project_path)
        self.destination = QLineEdit()
        self.basename = QLineEdit()
        form.addRow("Saved .mivfproj", self._picker(self.project, self.pick_project))
        form.addRow("Destination folder", self._picker(self.destination, self.pick_destination))
        form.addRow("Output basename", self.basename)
        root.addWidget(QLabel(
            "Builds the exact same plan a real export would use -- nothing is written to the "
            "destination folder by this dialog."
        ))

        self.tabs = QTabWidget()
        root.addWidget(self.tabs, 1)

        self.report_view = QPlainTextEdit()
        self.report_view.setReadOnly(True)
        self.tabs.addTab(self.report_view, "Check Project")

        dry_run_tab = QWidget()
        dry_run_layout = QVBoxLayout(dry_run_tab)
        self.table = QTableWidget(0, 8)
        self.table.setHorizontalHeaderLabels(
            ["Filename", "Role", "Control", "State", "Dimensions", "Size (bytes)", "SHA-256", "Status"])
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.table.itemSelectionChanged.connect(self._update_asset_detail)
        dry_run_layout.addWidget(self.table, 1)
        self.asset_detail = QLabel("Select a row to see its effective recipe.")
        self.asset_detail.setWordWrap(True)
        dry_run_layout.addWidget(self.asset_detail)
        self.tabs.addTab(dry_run_tab, "Export Dry Run")

        self.summary_view = QPlainTextEdit()
        self.summary_view.setReadOnly(True)
        self.tabs.addTab(self.summary_view, "Change Summary")

        buttons = QHBoxLayout()
        self.run_btn = QPushButton("Check Project")
        self.close_btn = QPushButton("Close")
        buttons.addStretch(1)
        buttons.addWidget(self.run_btn)
        buttons.addWidget(self.close_btn)
        root.addLayout(buttons)
        self.run_btn.clicked.connect(self.start)
        self.close_btn.clicked.connect(self.reject)

        if project_path:
            p = Path(project_path)
            self.destination.setText(str(p.parent))
            self.basename.setText(p.stem)

    def _picker(self, edit: QLineEdit, fn) -> QWidget:
        w = QHBoxLayout()
        w.addWidget(edit, 1)
        b = QPushButton("Browse")
        b.clicked.connect(fn)
        w.addWidget(b)
        c = QWidget()
        c.setLayout(w)
        return c

    def pick_project(self):
        p, _ = QFileDialog.getOpenFileName(self, "Select MIVF project", self.project.text(),
                                            "MIVF project (*.mivfproj)")
        if p:
            self.project.setText(p)
            path = Path(p)
            self.destination.setText(str(path.parent))
            self.basename.setText(path.stem)

    def pick_destination(self):
        p = QFileDialog.getExistingDirectory(self, "Select export destination", self.destination.text())
        if p:
            self.destination.setText(p)

    def start(self):
        p = Path(self.project.text().strip())
        d = Path(self.destination.text().strip()) if self.destination.text().strip() else None
        b = self.basename.text().strip() or p.stem
        if not p.is_file() or d is None:
            QMessageBox.warning(self, "Check blocked", "Select an existing saved .mivfproj and a destination folder.")
            return
        self.run_btn.setEnabled(False)
        self.report_view.setPlainText("Checking project...")
        self.worker = _Worker(p, d, b, self)
        self.worker.succeeded.connect(self._done_ok)
        self.worker.failed.connect(self._done_bad)
        self.worker.start()

    def _done_bad(self, message: str):
        self.run_btn.setEnabled(True)
        self.report_view.setPlainText(f"Could not build a plan for this project:\n\n{message}")
        self.table.setRowCount(0)
        self.summary_view.setPlainText("")

    def _done_ok(self, plan: theme_plan.PackagePlan):
        self.run_btn.setEnabled(True)
        self.plan = plan
        self.report_view.setPlainText(theme_plan.format_check_project_report(plan))
        self.summary_view.setPlainText(theme_plan.format_change_summary(plan))
        self._populate_table(plan)

    def _populate_table(self, plan: theme_plan.PackagePlan):
        self.table.setRowCount(len(plan.files))
        for row, pf in enumerate(plan.files):
            dims = f"{pf.width}x{pf.height}" if pf.width and pf.height else ""
            values = [pf.filename, pf.role, pf.control or "", pf.state or "",
                      dims, f"{pf.size:,}", pf.sha256[:16] + "...", pf.status]
            for col, value in enumerate(values):
                item = QTableWidgetItem(value)
                if col == 7:
                    item.setForeground(QColor(STATUS_COLOR.get(pf.status, "#888")))
                self.table.setItem(row, col, item)

    def _update_asset_detail(self):
        rows = self.table.selectionModel().selectedRows()
        if not rows or self.plan is None:
            self.asset_detail.setText("Select a row to see its effective recipe.")
            return
        pf = self.plan.files[rows[0].row()]
        if pf.status == "removed":
            self.asset_detail.setText(
                f"{pf.filename}: present in the destination folder but no longer produced by this "
                "project -- Check Project never deletes this automatically."
            )
            return
        detail = f"{pf.filename}\nSHA-256: {pf.sha256}\nSource: {pf.source or '(no recipe -- legacy/background asset)'}"
        if pf.dedup_of:
            detail += f"\nReuses the same bytes as: {pf.dedup_of}"
        if pf.recipe:
            recipe_bits = ", ".join(f"{k}={v}" for k, v in sorted(pf.recipe.items()) if k != "source")
            detail += f"\nEffective recipe: {recipe_bits}"
        self.asset_detail.setText(detail)
