"""E.3.1: Device Storage Planner dialog -- read-only report over
storage_plan.StoragePlan. Never deletes, never offers cleanup, never
modifies the project. Every displayed number carries its classification
label explicitly (see storage_plan.SizeClass)."""
from __future__ import annotations

from pathlib import Path
from PySide6.QtCore import QThread, Signal
from PySide6.QtWidgets import QDialog, QLabel, QMessageBox, QPlainTextEdit, QPushButton, QVBoxLayout

from . import storage_plan as sp

CLASS_LABEL = {
    sp.SizeClass.KNOWN_EXACT: "KNOWN EXACT",
    sp.SizeClass.MEASURED: "MEASURED",
    sp.SizeClass.ESTIMATED: "ESTIMATED",
    sp.SizeClass.APPROXIMATE_PROJECTION: "APPROXIMATE PROJECTION",
    sp.SizeClass.UNKNOWN: "UNKNOWN",
}


def _fmt(est: sp.SizeEstimate) -> str:
    tag = f"[{CLASS_LABEL[est.classification]}]"
    if est.bytes is None:
        return f"{tag} {est.label}"
    return f"{tag} {est.bytes:,} bytes -- {est.label}"


def format_report(plan: sp.StoragePlan) -> str:
    lines = [
        "SOURCE & OUTPUT",
        f"  Source media size:        {_fmt(plan.source_size)}",
        f"  Existing output size:     {_fmt(plan.existing_output_size)}",
        f"  Theme/sidecar size:       {_fmt(plan.theme_sidecar_size)}",
        "",
        "ESTIMATED FINAL OUTPUT",
        f"  Audio component:          {_fmt(plan.estimated_audio_bytes)}",
        f"  Video component:          {_fmt(plan.estimated_video_bytes)}",
        f"  Total estimated output:   {_fmt(plan.estimated_output_size)}",
        "",
        "WORKING SPACE",
        f"  Peak working space:       {_fmt(plan.working_space_required)}",
        f"  Job-recovery allowance:   {_fmt(plan.job_recovery_allowance)}",
        f"  Recommended safety margin: {plan.safety_margin_bytes:,} bytes (heuristic, not measured)",
        "",
        "VOLUMES",
    ]
    if plan.destination_volume:
        v = plan.destination_volume
        lines.append(f"  Destination ({v.path}): {v.free_bytes:,} / {v.total_bytes:,} bytes free")
    else:
        lines.append("  Destination: unavailable")
    if plan.working_volume:
        v = plan.working_volume
        lines.append(f"  Working dir ({v.path}): {v.free_bytes:,} / {v.total_bytes:,} bytes free")
    else:
        lines.append("  Working dir: unavailable")
    lines.append(f"  Projected destination free after this operation: {_fmt(plan.projected_destination_free)}")
    lines.append(f"  Projected working free after this operation:     {_fmt(plan.projected_working_free)}")

    if plan.same_volume_warnings:
        lines.append("")
        lines.append("VOLUME WARNINGS")
        for w in plan.same_volume_warnings:
            lines.append(f"  - {w}")

    if plan.filesystem_warnings:
        lines.append("")
        lines.append("FILESYSTEM ADVISORIES")
        for w in plan.filesystem_warnings:
            lines.append(f"  - {w}")

    for est, label in ((plan.projected_destination_free, "destination"), (plan.projected_working_free, "working")):
        if est.bytes is not None and est.bytes < 0:
            lines.append("")
            lines.append(f"  *** WARNING: projected {label} free space is NEGATIVE -- likely insufficient space. ***")

    if plan.assumptions:
        lines.append("")
        lines.append("ASSUMPTIONS & FORMULAS")
        for a in plan.assumptions:
            lines.append(f"  - {a}")

    return "\n".join(lines)


class _Worker(QThread):
    succeeded = Signal(object)
    failed = Signal(str)

    def __init__(self, project, parent=None):
        super().__init__(parent)
        self.project = project

    def run(self):
        try:
            self.succeeded.emit(sp.build_storage_plan(self.project))
        except Exception as exc:  # noqa: BLE001 -- surfaced to the user, never swallowed
            self.failed.emit(str(exc))


class StoragePlanDialog(QDialog):
    def __init__(self, project, parent=None):
        super().__init__(parent)
        self.project = project
        self.worker = None
        self.setWindowTitle("Device Storage Planner")
        self.resize(760, 620)

        root = QVBoxLayout(self)
        root.addWidget(QLabel(
            "Never deletes anything and offers no automatic cleanup. Every number below is labelled with "
            "how confident it is -- KNOWN EXACT / MEASURED are real measurements; ESTIMATED / APPROXIMATE "
            "PROJECTION are formulas or calibrated guesses; UNKNOWN means there isn't enough data to say."
        ))
        self.report_view = QPlainTextEdit()
        self.report_view.setReadOnly(True)
        root.addWidget(self.report_view, 1)
        refresh_btn = QPushButton("Refresh")
        refresh_btn.clicked.connect(self.refresh)
        root.addWidget(refresh_btn)

        self.refresh()

    def refresh(self):
        self.report_view.setPlainText("Calculating...")
        self.worker = _Worker(self.project, self)
        self.worker.succeeded.connect(self._done_ok)
        self.worker.failed.connect(self._done_bad)
        self.worker.start()

    def _done_ok(self, plan: sp.StoragePlan):
        self.report_view.setPlainText(format_report(plan))

    def _done_bad(self, message: str):
        self.report_view.setPlainText(f"Could not build a storage plan:\n\n{message}")
