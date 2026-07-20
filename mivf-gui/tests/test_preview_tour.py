"""Reusable Player/Theme Preview Tour -- pure-logic layer.

PreviewTourDialog needs a real QWidget and is exercised in
smoke_offscreen.py instead. This covers TourStep as a plain data
container -- there isn't much pure logic here beyond that, since
build_theme_preview_tour()'s only job is closing over a live MainWindow,
which is itself a widget-level concern.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "mivf-gui" / "src"))

from mivf_gui.preview_tour import TourStep  # noqa: E402


def test_tour_step_is_a_plain_data_container():
    calls = []
    step = TourStep("Title", "Caption", lambda: calls.append("ran"))
    assert step.title == "Title"
    assert step.caption == "Caption"
    step.apply()
    assert calls == ["ran"]
