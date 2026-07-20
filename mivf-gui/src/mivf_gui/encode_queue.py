"""D: Persistent multi-project encode queue.

Each entry references a saved .mivfproj file (never an inline/duplicated
project representation -- one project format, one source of truth) plus
an assigned job_dir so a job interrupted mid-encode resumes via the SAME
existing single-job stage-skip primitive this project already has
(project.job_dir / --resume-job, encode_mivf.py's job_manifest.json
fingerprinting -- see e0_prepare_job) rather than a new resume mechanism.
This module is the queue's own primitive; per-job resume is delegated
entirely to that existing mechanism, not reimplemented here.

Persisted via QSettings, same org/app pair project_home.py's
recent-projects list already established as this app's one convention.

Pure list/dataclass logic only -- no subprocess, no Qt widgets, so it's
host-testable without a display. The sequential runner that actually
drives EncodeRun per job lives in main_window.py, which already owns all
subprocess/progress-polling code for the single-project Run tab.
"""
from __future__ import annotations

import dataclasses
import json
import time
import uuid
from pathlib import Path

from PySide6.QtCore import QSettings

_SETTINGS_ORG = "MIVF"
_SETTINGS_APP = "MIVFToolkit"
_QUEUE_KEY = "encode_queue/jobs"

STATUS_PENDING = "pending"
STATUS_RUNNING = "running"
STATUS_DONE = "done"
STATUS_FAILED = "failed"
STATUS_CANCELLED = "cancelled"


def _settings() -> QSettings:
    return QSettings(_SETTINGS_ORG, _SETTINGS_APP)


@dataclasses.dataclass
class QueueJob:
    job_id: str
    project_path: str
    job_dir: str
    status: str = STATUS_PENDING
    added_at: float = 0.0
    error: str | None = None

    def to_dict(self) -> dict:
        return dataclasses.asdict(self)

    @staticmethod
    def from_dict(d: dict) -> "QueueJob":
        return QueueJob(
            job_id=d.get("job_id") or uuid.uuid4().hex,
            project_path=d["project_path"],
            job_dir=d["job_dir"],
            status=d.get("status", STATUS_PENDING),
            added_at=float(d.get("added_at", 0.0)),
            error=d.get("error"),
        )

    def has_resumable_manifest(self) -> bool:
        """True once a prior attempt got far enough to write job_manifest.json
        (e0_prepare_job writes it unconditionally once --job-dir is set,
        before the first stage even completes) -- exactly the condition
        encode_mivf.py's own --resume-job requires. A fresh, never-attempted
        job has no manifest yet, so its first run must NOT pass
        --resume-job (encode_mivf.py raises SystemExit if it did)."""
        return (Path(self.job_dir) / "job_manifest.json").exists()


def coerce_stale_running(jobs: list[QueueJob]) -> list[QueueJob]:
    """A job persisted as RUNNING means the app closed or crashed mid-encode
    -- no process is actually running anymore at load time, so treat it as
    an interrupted-but-resumable pending job. Safe by construction: the
    underlying job_dir/--resume-job stage-skip primitive is exactly what
    makes re-attempting a partially-completed job correct rather than
    wasteful or dangerous."""
    for job in jobs:
        if job.status == STATUS_RUNNING:
            job.status = STATUS_PENDING
    return jobs


def load_queue() -> list[QueueJob]:
    raw = _settings().value(_QUEUE_KEY, "[]")
    try:
        data = json.loads(raw) if isinstance(raw, str) else list(raw)
    except (TypeError, ValueError):
        data = []
    jobs = [QueueJob.from_dict(d) for d in data if isinstance(d, dict) and "project_path" in d and "job_dir" in d]
    return coerce_stale_running(jobs)


def save_queue(jobs: list[QueueJob]) -> None:
    _settings().setValue(_QUEUE_KEY, json.dumps([j.to_dict() for j in jobs]))


def new_job(project_path: str, jobs_root: Path) -> QueueJob:
    """jobs_root is supplied by the caller (kept out of this pure module so
    tests can point it at a tmp_path) -- a fresh, collision-free job_dir
    for this queue entry is created under it, named by a random id rather
    than derived from the project path so re-queuing the same project
    twice (e.g. after editing it) never collides with an earlier
    attempt's partial state."""
    job_id = uuid.uuid4().hex
    job_dir = str(Path(jobs_root) / job_id)
    return QueueJob(job_id=job_id, project_path=str(project_path), job_dir=job_dir, added_at=time.time())


def next_pending(jobs: list[QueueJob]) -> QueueJob | None:
    for job in jobs:
        if job.status == STATUS_PENDING:
            return job
    return None


def move_job(jobs: list[QueueJob], job_id: str, delta: int) -> list[QueueJob]:
    """Pure reordering: delta=-1 moves up, +1 moves down. A no-op past
    either boundary rather than raising, so a UI can wire this directly to
    a "Move Up"/"Move Down" button without a separate boundary check."""
    idx = next((i for i, j in enumerate(jobs) if j.job_id == job_id), None)
    if idx is None:
        return jobs
    new_idx = idx + delta
    if new_idx < 0 or new_idx >= len(jobs):
        return jobs
    reordered = list(jobs)
    reordered[idx], reordered[new_idx] = reordered[new_idx], reordered[idx]
    return reordered


def remove_job(jobs: list[QueueJob], job_id: str) -> list[QueueJob]:
    return [j for j in jobs if j.job_id != job_id]


def status_label(status: str) -> str:
    return {
        STATUS_PENDING: "Pending",
        STATUS_RUNNING: "Running",
        STATUS_DONE: "Done",
        STATUS_FAILED: "Failed",
        STATUS_CANCELLED: "Cancelled",
    }.get(status, status)
