"""C.6: Advanced Interactive Dashboard Canvas.

Premiere-only (source/main.c's default/style-0 dashboard renderer) drag
editor for the three real, functional playback controls: Rewind,
Play/Pause, Fast Forward. Deliberately does not attempt a free-form editor
for all 16 transport styles -- see PHASE_STATE.md's shared-audit finding:
only Premiere's geometry is a single, small, parametrizable function
(mivf_c25_premiere_controls); every other style hardcodes its own layout
across a separate renderer with no override hook, and building one for all
16 would mean rewriting every renderer (an explicit stop condition for this
phase).

The visual and the touch hitbox are the SAME (dx, dy) offset by
construction here and at runtime (mivf_customization_resolve_position is
consulted from both mivf_c25_premiere_controls and
hfix57_premiere_touch_rect with the identical control id) -- there is no
code path in this editor that can move one without the other.
"""
from __future__ import annotations

from PySide6.QtCore import Qt, QRect, QPoint
from PySide6.QtGui import QPainter, QPen, QBrush, QColor
from PySide6.QtWidgets import (
    QDialog, QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QCheckBox, QSpinBox, QFormLayout, QMessageBox, QListWidget,
)

from .theme_plan import PREMIERE_CONTROL_GEOMETRY, DASHBOARD_CANVAS_W, DASHBOARD_CANVAS_H, validate_dashboard_layout

CONTROL_ORDER = ("REWIND", "PLAY_PAUSE", "FAST_FORWARD")
CONTROL_LABELS = {"REWIND": "Rewind", "PLAY_PAUSE": "Play / Pause", "FAST_FORWARD": "Fast Forward"}
GRID_SIZE = 8


class _CanvasWidget(QWidget):
    """Renders the real 320x240 dashboard canvas at 2x for a comfortable
    drag target, and reports drags back as (control, dx, dy) through the
    owning dialog -- no state lives here beyond the active drag."""

    SCALE = 2

    def __init__(self, dialog: "DashboardCanvasDialog"):
        super().__init__()
        self.dialog = dialog
        self.setFixedSize(DASHBOARD_CANVAS_W * self.SCALE, DASHBOARD_CANVAS_H * self.SCALE)
        self.setMouseTracking(True)
        self._dragging: str | None = None
        self._drag_start_mouse = QPoint()
        self._drag_start_offset = (0, 0)

    def _control_center(self, control: str) -> tuple[int, int]:
        base_x, base_y, _ = PREMIERE_CONTROL_GEOMETRY[control]
        dx, dy = self.dialog.layout_offsets.get(control, [0, 0])
        return base_x + dx, base_y + dy

    def _hit_control(self, pos: QPoint) -> str | None:
        px, py = pos.x() / self.SCALE, pos.y() / self.SCALE
        for control in CONTROL_ORDER:
            cx, cy = self._control_center(control)
            _, _, r = PREMIERE_CONTROL_GEOMETRY[control]
            if (px - cx) ** 2 + (py - cy) ** 2 <= r * r:
                return control
        return None

    def mousePressEvent(self, event):
        hit = self._hit_control(event.position().toPoint())
        if hit:
            self._dragging = hit
            self._drag_start_mouse = event.position().toPoint()
            self._drag_start_offset = tuple(self.dialog.layout_offsets.get(hit, [0, 0]))
            self.dialog.select_control(hit)

    def mouseMoveEvent(self, event):
        if not self._dragging:
            return
        delta = event.position().toPoint() - self._drag_start_mouse
        dx = self._drag_start_offset[0] + round(delta.x() / self.SCALE)
        dy = self._drag_start_offset[1] + round(delta.y() / self.SCALE)
        if self.dialog.snap_enabled():
            dx = round(dx / GRID_SIZE) * GRID_SIZE
            dy = round(dy / GRID_SIZE) * GRID_SIZE
        self.dialog.set_offset(self._dragging, dx, dy, push_undo=False)
        self.update()

    def mouseReleaseEvent(self, event):
        if self._dragging:
            self.dialog.commit_drag(self._dragging)
        self._dragging = None

    def paintEvent(self, event):
        p = QPainter(self)
        p.fillRect(self.rect(), QColor(12, 16, 24))
        s = self.SCALE

        # Alignment guides: dashed line when a dragged control's center
        # shares an axis with another control's center or the canvas center.
        if self._dragging:
            dcx, dcy = self._control_center(self._dragging)
            guide_pen = QPen(QColor(90, 220, 255), 1, Qt.DashLine)
            targets = [(DASHBOARD_CANVAS_W // 2, DASHBOARD_CANVAS_H // 2)]
            for c in CONTROL_ORDER:
                if c != self._dragging:
                    targets.append(self._control_center(c))
            for tx, ty in targets:
                p.setPen(guide_pen)
                if abs(dcx - tx) <= 1:
                    p.drawLine(tx * s, 0, tx * s, DASHBOARD_CANVAS_H * s)
                if abs(dcy - ty) <= 1:
                    p.drawLine(0, ty * s, DASHBOARD_CANVAS_W * s, ty * s)

        overlap_pairs = {
            frozenset(m.role.split("+")) for m in self.dialog.current_messages() if m.category == "layout_overlap"
        }
        for control in CONTROL_ORDER:
            cx, cy = self._control_center(control)
            _, _, r = PREMIERE_CONTROL_GEOMETRY[control]
            selected = control == self.dialog.selected_control
            overlapping = any(control in pair for pair in overlap_pairs)
            color = QColor(255, 120, 90) if overlapping else (QColor(244, 183, 64) if selected else QColor(120, 150, 190))
            p.setPen(QPen(color, 2))
            p.setBrush(QBrush(QColor(color.red(), color.green(), color.blue(), 60)))
            p.drawEllipse(QPoint(cx * s, cy * s), r * s, r * s)
            p.setPen(QPen(QColor(230, 235, 245)))
            p.drawText(QRect(cx * s - 40, cy * s - 8, 80, 16), Qt.AlignCenter, CONTROL_LABELS[control])
        p.end()


class DashboardCanvasDialog(QDialog):
    """Cancel discipline: self.layout_offsets is a working copy; the caller
    only sees committed changes if the dialog is Accepted (mirrors
    ControlArtworkDialog's existing Cancel-discards-edits contract, verified
    by an existing smoke-test precedent)."""

    def __init__(self, project, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Dashboard Canvas (Premiere)")
        self.project = project
        self.layout_offsets: dict[str, list[int]] = {
            k: list(v) for k, v in (project.dashboard_layout or {}).items()
        }
        self.selected_control: str | None = None
        self.undo_stack: list[dict[str, list[int]]] = []
        self.redo_stack: list[dict[str, list[int]]] = []

        root = QHBoxLayout(self)
        self.canvas = _CanvasWidget(self)
        root.addWidget(self.canvas)

        side = QVBoxLayout()
        root.addLayout(side)

        side.addWidget(QLabel(
            "Drag a control to reposition it. This moves the same control's touch\n"
            "hitbox by the identical amount -- they can never diverge. Premiere\n"
            "dashboard style only; every other style is unaffected."
        ))

        self.snap_check = QCheckBox(f"Snap to {GRID_SIZE}px grid")
        self.snap_check.setChecked(True)
        side.addWidget(self.snap_check)

        form = QFormLayout()
        self.dx_spin = QSpinBox(); self.dx_spin.setRange(-160, 160)
        self.dy_spin = QSpinBox(); self.dy_spin.setRange(-120, 120)
        self.dx_spin.valueChanged.connect(self._spin_changed)
        self.dy_spin.valueChanged.connect(self._spin_changed)
        form.addRow("Offset X:", self.dx_spin)
        form.addRow("Offset Y:", self.dy_spin)
        side.addLayout(form)

        reset_row = QHBoxLayout()
        self.reset_btn = QPushButton("Reset control")
        self.reset_btn.clicked.connect(self._reset_selected)
        self.reset_all_btn = QPushButton("Reset all")
        self.reset_all_btn.clicked.connect(self._reset_all)
        reset_row.addWidget(self.reset_btn)
        reset_row.addWidget(self.reset_all_btn)
        side.addLayout(reset_row)

        undo_row = QHBoxLayout()
        self.undo_btn = QPushButton("Undo")
        self.undo_btn.clicked.connect(self.undo)
        self.redo_btn = QPushButton("Redo")
        self.redo_btn.clicked.connect(self.redo)
        undo_row.addWidget(self.undo_btn)
        undo_row.addWidget(self.redo_btn)
        side.addLayout(undo_row)

        self.messages_list = QListWidget()
        side.addWidget(QLabel("Validation:"))
        side.addWidget(self.messages_list)

        button_row = QHBoxLayout()
        self.ok_btn = QPushButton("OK")
        self.ok_btn.clicked.connect(self.accept)
        self.cancel_btn = QPushButton("Cancel")
        self.cancel_btn.clicked.connect(self.reject)
        button_row.addWidget(self.ok_btn)
        button_row.addWidget(self.cancel_btn)
        side.addLayout(button_row)

        self.select_control(CONTROL_ORDER[0])
        self._refresh_messages()

    def snap_enabled(self) -> bool:
        return self.snap_check.isChecked()

    def select_control(self, control: str):
        self.selected_control = control
        dx, dy = self.layout_offsets.get(control, [0, 0])
        self.dx_spin.blockSignals(True); self.dy_spin.blockSignals(True)
        self.dx_spin.setValue(dx); self.dy_spin.setValue(dy)
        self.dx_spin.blockSignals(False); self.dy_spin.blockSignals(False)
        self.canvas.update()

    def _spin_changed(self, _value):
        if self.selected_control:
            self.set_offset(self.selected_control, self.dx_spin.value(), self.dy_spin.value(), push_undo=True)

    def set_offset(self, control: str, dx: int, dy: int, push_undo: bool):
        if push_undo:
            self._push_undo()
        if dx == 0 and dy == 0:
            self.layout_offsets.pop(control, None)
        else:
            self.layout_offsets[control] = [dx, dy]
        if control == self.selected_control:
            self.dx_spin.blockSignals(True); self.dy_spin.blockSignals(True)
            self.dx_spin.setValue(dx); self.dy_spin.setValue(dy)
            self.dx_spin.blockSignals(False); self.dy_spin.blockSignals(False)
        self.canvas.update()
        self._refresh_messages()

    def commit_drag(self, control: str):
        """Called once on mouse release -- the whole drag becomes one undo
        entry, not one per intermediate mouseMoveEvent."""
        self._push_undo()
        self.canvas.update()
        self._refresh_messages()

    def _push_undo(self):
        import copy
        self.undo_stack.append(copy.deepcopy(self.layout_offsets))
        self.redo_stack.clear()

    def undo(self):
        import copy
        if not self.undo_stack:
            return
        self.redo_stack.append(copy.deepcopy(self.layout_offsets))
        self.layout_offsets = self.undo_stack.pop()
        self.select_control(self.selected_control or CONTROL_ORDER[0])
        self._refresh_messages()

    def redo(self):
        import copy
        if not self.redo_stack:
            return
        self.undo_stack.append(copy.deepcopy(self.layout_offsets))
        self.layout_offsets = self.redo_stack.pop()
        self.select_control(self.selected_control or CONTROL_ORDER[0])
        self._refresh_messages()

    def _reset_selected(self):
        if self.selected_control:
            self.set_offset(self.selected_control, 0, 0, push_undo=True)

    def _reset_all(self):
        self._push_undo()
        self.layout_offsets = {}
        self.select_control(self.selected_control or CONTROL_ORDER[0])
        self._refresh_messages()

    def current_messages(self):
        return validate_dashboard_layout({k: tuple(v) for k, v in self.layout_offsets.items()})

    def _refresh_messages(self):
        self.messages_list.clear()
        messages = self.current_messages()
        if not messages:
            self.messages_list.addItem("No issues.")
        for m in messages:
            self.messages_list.addItem(f"[{m.severity}] {m.message}")
        self.canvas.update()

    def accept(self):
        errors = [m for m in self.current_messages() if m.severity == "ERROR"]
        if errors:
            QMessageBox.warning(self, "Cannot save", "Resolve all errors before saving:\n" +
                                 "\n".join(m.message for m in errors))
            return
        self.project.dashboard_layout = {k: list(v) for k, v in self.layout_offsets.items()}
        super().accept()
