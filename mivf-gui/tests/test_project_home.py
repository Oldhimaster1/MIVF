"""UX.1: Project Home -- pure-logic layer (recent-projects list math).

Dialog construction/interaction (ProjectHomeDialog, NewProjectWizard,
MainWindow's wizard-result/dispatch handoff) needs a real QWidget and
lives in smoke_offscreen.py instead, matching the existing split used
for dashboard_canvas.py (test_dashboard_canvas.py for pure logic,
smoke_offscreen.py for the widget-driven checks).
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "mivf-gui" / "src"))

from mivf_gui.project_home import dedup_cap_recent  # noqa: E402


def test_new_path_goes_first():
    result = dedup_cap_recent(["a.mivfproj", "b.mivfproj"], "c.mivfproj")
    assert result[0] == "c.mivfproj"
    assert result == ["c.mivfproj", "a.mivfproj", "b.mivfproj"]


def test_reopening_an_existing_entry_moves_it_to_front_without_duplicating(tmp_path):
    a = str(tmp_path / "a.mivfproj")
    b = str(tmp_path / "b.mivfproj")
    result = dedup_cap_recent([a, b], a)
    assert result == [a, b]
    assert result.count(a) == 1


def test_cap_is_enforced():
    existing = [f"p{i}.mivfproj" for i in range(10)]
    result = dedup_cap_recent(existing, "new.mivfproj", cap=8)
    assert len(result) == 8
    assert result[0] == "new.mivfproj"


def test_none_new_path_is_a_no_op_add():
    result = dedup_cap_recent(["a.mivfproj"], None)
    assert result == ["a.mivfproj"]


def test_empty_existing_list_with_new_path():
    assert dedup_cap_recent([], "only.mivfproj") == ["only.mivfproj"]


def test_relative_and_absolute_forms_of_the_same_file_dedupe(tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    f = tmp_path / "proj.mivfproj"
    f.write_text("{}")
    result = dedup_cap_recent([str(f)], "proj.mivfproj")
    assert len(result) == 1
