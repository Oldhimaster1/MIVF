"""D: Persistent multi-project encode queue -- pure-logic layer.

The sequential runner that actually drives EncodeRun per job needs a real
QWidget/QThread and lives in smoke_offscreen.py instead (same split
already used for dashboard_canvas.py and project_home.py).
"""
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "mivf-gui" / "src"))

from mivf_gui.encode_queue import (  # noqa: E402
    QueueJob, new_job, next_pending, move_job, remove_job,
    coerce_stale_running, load_queue, save_queue, status_label,
    STATUS_PENDING, STATUS_RUNNING, STATUS_DONE, STATUS_FAILED,
    _settings, _QUEUE_KEY,
)


@pytest.fixture(autouse=True)
def _isolated_queue_settings():
    """Every test starts from an empty persisted queue -- otherwise a
    real QSettings store left over from a prior local run (or another
    test) would leak state across tests, same isolation concern already
    handled for project_home.py's recent-projects list in smoke_offscreen.py."""
    _settings().remove(_QUEUE_KEY)
    yield
    _settings().remove(_QUEUE_KEY)


# --- new_job / job_dir assignment -----------------------------------------

def test_new_job_gets_a_unique_job_dir_under_the_given_root(tmp_path):
    a = new_job("proj_a.mivfproj", tmp_path)
    b = new_job("proj_a.mivfproj", tmp_path)  # same project, queued twice
    assert a.job_dir != b.job_dir
    assert Path(a.job_dir).parent == tmp_path
    assert a.status == STATUS_PENDING


def test_fresh_job_has_no_resumable_manifest_yet(tmp_path):
    job = new_job("proj.mivfproj", tmp_path)
    assert job.has_resumable_manifest() is False


def test_job_with_a_written_manifest_is_resumable(tmp_path):
    job = new_job("proj.mivfproj", tmp_path)
    Path(job.job_dir).mkdir(parents=True)
    (Path(job.job_dir) / "job_manifest.json").write_text("{}")
    assert job.has_resumable_manifest() is True


# --- next_pending / move_job / remove_job (pure list ops) -----------------

def test_next_pending_skips_running_and_done():
    jobs = [
        QueueJob("1", "a.mivfproj", "/jd/1", status=STATUS_DONE),
        QueueJob("2", "b.mivfproj", "/jd/2", status=STATUS_RUNNING),
        QueueJob("3", "c.mivfproj", "/jd/3", status=STATUS_PENDING),
    ]
    assert next_pending(jobs).job_id == "3"


def test_next_pending_returns_none_when_nothing_pending():
    jobs = [QueueJob("1", "a.mivfproj", "/jd/1", status=STATUS_DONE)]
    assert next_pending(jobs) is None


def test_move_job_up_and_down():
    jobs = [QueueJob(str(i), f"p{i}.mivfproj", f"/jd/{i}") for i in range(3)]
    moved = move_job(jobs, "2", -1)
    assert [j.job_id for j in moved] == ["0", "2", "1"]
    moved_back = move_job(moved, "2", 1)
    assert [j.job_id for j in moved_back] == ["0", "1", "2"]


def test_move_job_at_boundary_is_a_no_op():
    jobs = [QueueJob(str(i), f"p{i}.mivfproj", f"/jd/{i}") for i in range(3)]
    assert [j.job_id for j in move_job(jobs, "0", -1)] == ["0", "1", "2"]
    assert [j.job_id for j in move_job(jobs, "2", 1)] == ["0", "1", "2"]


def test_move_unknown_job_id_is_a_no_op():
    jobs = [QueueJob("1", "a.mivfproj", "/jd/1")]
    assert move_job(jobs, "does-not-exist", 1) == jobs


def test_remove_job():
    jobs = [QueueJob("1", "a.mivfproj", "/jd/1"), QueueJob("2", "b.mivfproj", "/jd/2")]
    remaining = remove_job(jobs, "1")
    assert [j.job_id for j in remaining] == ["2"]


# --- coerce_stale_running (the crash/restart safety rule) ------------------

def test_coerce_stale_running_resets_to_pending_not_lost_or_marked_failed():
    jobs = [QueueJob("1", "a.mivfproj", "/jd/1", status=STATUS_RUNNING)]
    coerced = coerce_stale_running(jobs)
    assert coerced[0].status == STATUS_PENDING


def test_coerce_stale_running_leaves_terminal_states_alone():
    jobs = [
        QueueJob("1", "a.mivfproj", "/jd/1", status=STATUS_DONE),
        QueueJob("2", "b.mivfproj", "/jd/2", status=STATUS_FAILED),
        QueueJob("3", "c.mivfproj", "/jd/3", status=STATUS_PENDING),
    ]
    coerced = coerce_stale_running(jobs)
    assert [j.status for j in coerced] == [STATUS_DONE, STATUS_FAILED, STATUS_PENDING]


# --- persistence round-trip -------------------------------------------------

def test_save_and_load_round_trips(tmp_path):
    jobs = [new_job("a.mivfproj", tmp_path), new_job("b.mivfproj", tmp_path)]
    save_queue(jobs)
    loaded = load_queue()
    assert [j.job_id for j in loaded] == [j.job_id for j in jobs]
    assert [j.project_path for j in loaded] == [j.project_path for j in jobs]


def test_load_queue_with_no_persisted_state_is_an_empty_list():
    assert load_queue() == []


def test_save_and_load_coerces_a_persisted_running_job_back_to_pending(tmp_path):
    job = new_job("a.mivfproj", tmp_path)
    job.status = STATUS_RUNNING
    save_queue([job])
    loaded = load_queue()
    assert loaded[0].status == STATUS_PENDING


def test_load_queue_ignores_malformed_entries_without_crashing():
    _settings().setValue(_QUEUE_KEY, '[{"not_a_valid_job": true}, {"project_path": "p.mivfproj", "job_dir": "/jd"}]')
    loaded = load_queue()
    assert len(loaded) == 1
    assert loaded[0].project_path == "p.mivfproj"


# --- status_label -----------------------------------------------------------

def test_status_label_covers_every_known_status():
    for status in (STATUS_PENDING, STATUS_RUNNING, STATUS_DONE, STATUS_FAILED):
        assert status_label(status)


def test_status_label_falls_back_to_the_raw_value_for_an_unknown_status():
    assert status_label("weird_future_status") == "weird_future_status"
