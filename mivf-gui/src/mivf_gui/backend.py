"""GUI/backend contract: encode_mivf.py remains authoritative.

Implements mivf_customization_gui_20260716/ENCODER_GUI_BACKEND_CONTRACT.md.
This module never constructs a shell string and never sets shell=True --
every invocation is an explicit argument list, matching the binding rule in
CUSTOMIZATION_RISK_REGISTER.md's "unsafe shell construction" entry.

build_argv() is a pure function (project -> list[str]) with no side effects,
so it is unit-testable independently of actually running an encode -- see
mivf-gui/tests/test_backend_equivalence.py.
"""
from __future__ import annotations

import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from .presets import preset_flags
from .project import MivfProject

BOOL_FLAGS = {
    "no_deploy", "m2y2", "no_seek_index", "no_embedded_index",
    "report_packet_sizes", "resume_job", "keep_intermediates",
}


def _flag_name(key: str) -> str:
    return "--" + key.replace("_", "-")


def build_argv(project: MivfProject, encoder_script: Path, python_exe: str | None = None) -> list[str]:
    """Pure: project -> the exact argv that will be passed to subprocess.Popen.
    Never a string -- always a list, so no shell quoting/injection question
    can ever arise. Flags are emitted in sorted-key order so the same
    project always produces byte-identical output (required for the
    GUI/CLI equivalence golden-file test)."""
    python_exe = python_exe or sys.executable

    src = project.resolve(project.source_media)
    out = project.resolve(project.output_path)
    if not src:
        raise ValueError("project.source_media is not set")
    if not out:
        raise ValueError("project.output_path is not set")

    argv = [python_exe, str(encoder_script), str(src), str(out)]

    flags: dict[str, object] = dict(preset_flags(project.preset))
    flags.update(project.advanced_overrides)  # explicit user choices win over the preset

    for key in sorted(flags.keys()):
        value = flags[key]
        name = _flag_name(key)
        if key in BOOL_FLAGS:
            if value:
                argv.append(name)
        else:
            argv.append(name)
            argv.append(str(value))

    if project.video_stream_index is not None:
        argv.extend(["--video-stream", str(project.video_stream_index)])
    if project.audio_stream_index is not None:
        argv.extend(["--audio-stream", str(project.audio_stream_index)])
    if project.subtitle_stream_index is not None:
        argv.extend(["--subtitle-stream", str(project.subtitle_stream_index)])
        if project.subtitle_edition:
            argv.extend(["--subtitle-edition", str(project.subtitle_edition)])
    if project.job_dir:
        job_dir = project.resolve(project.job_dir)
        argv.append("--job-dir")
        argv.append(str(job_dir))
    if project.resume_job:
        argv.append("--resume-job")

    return argv




@dataclass(frozen=True)
class EncodeProgress:
    """One parsed, presentation-neutral progress update from encoder stdout.

    ``total_frames`` is optional because some sources do not report a reliable
    frame count.  Callers must use indeterminate UI in that case rather than
    inventing a percentage or ETA.
    """

    stage: str
    label: str
    current_frames: int | None = None
    total_frames: int | None = None
    speed_fps: float | None = None
    eta_seconds: float | None = None
    detail: str = ""

    @property
    def determinate(self) -> bool:
        return bool(self.total_frames and self.current_frames is not None)

    @property
    def percent(self) -> int | None:
        if not self.determinate:
            return None
        return max(0, min(100, round(100 * self.current_frames / self.total_frames)))


class EncodeProgressTracker:
    """Parses the live authoritative encoder's existing stdout.

    The parser deliberately recognizes only stable, source-audited messages
    emitted by encode_mivf.py. Unknown lines remain available in the raw log
    and do not fabricate progress. ETA is shown only during frame-counted
    video/audio stages and only when both a trustworthy total and a positive
    observed speed are available.
    """

    _VIDEO_RE = re.compile(
        r"^encoded segment \d+: start=\d+ frames=\d+ "
        r"total=(?P<current>\d+) speed=(?P<speed>[0-9]+(?:\.[0-9]+)?) fps$"
    )
    _AUDIO_RE = re.compile(r"^muxed (?P<current>\d+) frames$")

    def __init__(self, total_frames: int | None = None, clock=None):
        self.total_frames = total_frames if total_frames and total_frames > 0 else None
        self._clock = clock or time.monotonic
        self._stage = "preparing"
        self._stage_started = self._clock()
        self._last_audio_frames = 0
        self._last_audio_time = self._stage_started
        self._smoothed_audio_fps: float | None = None

    def _set_stage(self, stage: str) -> None:
        if stage != self._stage:
            self._stage = stage
            now = self._clock()
            self._stage_started = now
            if stage == "audio":
                self._last_audio_frames = 0
                self._last_audio_time = now
                self._smoothed_audio_fps = None

    def _eta(self, current: int, speed: float | None) -> float | None:
        if not self.total_frames or not speed or speed <= 0:
            return None
        return max(0.0, (self.total_frames - min(current, self.total_frames)) / speed)

    def update(self, line: str) -> EncodeProgress | None:
        text = line.strip()
        if not text:
            return None
        if text == "1. Streaming and Compressing Video":
            self._set_stage("video")
            return EncodeProgress("video", "Encoding video", 0, self.total_frames, detail="Starting encoder workers")
        if text.startswith("E0 RESUME: reusing "):
            self._set_stage("video")
            return EncodeProgress("video", "Video already encoded", self.total_frames, self.total_frames, detail="Reusing verified job intermediate")
        match = self._VIDEO_RE.match(text)
        if match:
            self._set_stage("video")
            current = int(match.group("current"))
            speed = float(match.group("speed"))
            return EncodeProgress("video", "Encoding video", current, self.total_frames, speed, self._eta(current, speed), text)
        if text.startswith("2. Multiplexing Audio ("):
            self._set_stage("audio")
            return EncodeProgress("audio", "Packaging audio", 0, self.total_frames, detail=text)
        if text == "Extracting Subtitle Edition":
            self._set_stage("subtitle")
            return EncodeProgress("subtitle", "Authoring subtitle sidecar", detail="Time remaining unavailable")
        match = self._AUDIO_RE.match(text)
        if match:
            self._set_stage("audio")
            current = int(match.group("current"))
            now = self._clock()
            delta_frames = current - self._last_audio_frames
            delta_time = now - self._last_audio_time
            instant = delta_frames / delta_time if delta_frames > 0 and delta_time > 0 else None
            if instant is not None:
                self._smoothed_audio_fps = instant if self._smoothed_audio_fps is None else (0.25 * instant + 0.75 * self._smoothed_audio_fps)
            self._last_audio_frames = current
            self._last_audio_time = now
            speed = self._smoothed_audio_fps
            return EncodeProgress("audio", "Packaging audio", current, self.total_frames, speed, self._eta(current, speed), text)
        if text.startswith("3. Range-coding video to M2Y2"):
            self._set_stage("m2y2")
            return EncodeProgress("m2y2", "Optimizing M2Y2 output", detail="Lossless range-coding; time remaining unavailable")
        if text == "4. Generating Seek Index Metadata":
            self._set_stage("seek_index")
            return EncodeProgress("seek_index", "Building seek index", detail="Time remaining unavailable")
        if text == "5. Packet Size Report":
            self._set_stage("packet_report")
            return EncodeProgress("packet_report", "Analyzing packet sizes", detail="Time remaining unavailable")
        if text == "STAGE TIMING":
            self._set_stage("finalizing")
            return EncodeProgress("finalizing", "Finalizing", detail="Verifying output")
        return None


def format_command_preview(argv: list[str]) -> str:
    """Read-only display string for the Review screen -- the literal array
    that will be passed to Popen, not a reconstructed approximation."""
    return " ".join(f'"{a}"' if " " in a else a for a in argv)


class EncodeRun:
    """Owns one subprocess.Popen invocation: streamed output, raw-log
    preservation, process-tree-aware cancellation, and output verification.
    Argument-array only (see module docstring); never shell=True."""

    def __init__(self, argv: list[str], job_dir: Path | None = None, total_frames: int | None = None):
        self.argv = argv
        self.job_dir = job_dir
        self.process: subprocess.Popen | None = None
        self.log_lines: list[str] = []
        self.progress = EncodeProgressTracker(total_frames=total_frames)

    def start(self) -> None:
        creationflags = 0
        if sys.platform == "win32":
            creationflags = subprocess.CREATE_NEW_PROCESS_GROUP
        self.process = subprocess.Popen(
            self.argv,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            creationflags=creationflags,
        )

    def poll_output(
        self,
        on_line: Callable[[str], None] | None = None,
        on_progress: Callable[[EncodeProgress], None] | None = None,
    ) -> bool:
        """Call repeatedly (e.g. from a Qt timer) to drain output without
        blocking the event loop. Returns True while the process is still
        running."""
        if self.process is None or self.process.stdout is None:
            return False
        line = self.process.stdout.readline()
        if line:
            self.log_lines.append(line.rstrip("\n"))
            clean_line = line.rstrip("\n")
            if on_line:
                on_line(clean_line)
            progress = self.progress.update(clean_line)
            if progress is not None and on_progress:
                on_progress(progress)
        return self.process.poll() is None

    def cancel(self) -> None:
        """Process-tree-aware termination -- the encoder's own --jobs mode
        spawns parallel workers (mivf_parallel_engine.py), so a plain
        Popen.terminate() on the parent alone can leave orphans."""
        if self.process is None:
            return
        if sys.platform == "win32":
            subprocess.run(
                ["taskkill", "/F", "/T", "/PID", str(self.process.pid)],
                capture_output=True,
            )
        else:
            import os
            import signal
            try:
                os.killpg(os.getpgid(self.process.pid), signal.SIGTERM)
            except ProcessLookupError:
                pass

    def save_raw_log(self, path: Path) -> None:
        path.write_text("\n".join(self.log_lines), encoding="utf-8")

    @property
    def returncode(self) -> int | None:
        return self.process.returncode if self.process else None


def verify_output(output_path: Path) -> tuple[bool, str]:
    """Direct lesson from this project's own real 0-byte disk-full incident
    (see the encoder-speed-audit portion of this session): a reported-success
    exit code is not sufficient proof the output is real. Checks existence
    and non-zero size, nothing more (a full container validity check belongs
    to the CLI itself, not the GUI)."""
    if not output_path.exists():
        return False, "output file does not exist"
    size = output_path.stat().st_size
    if size == 0:
        return False, "output file exists but is 0 bytes"
    return True, f"output file exists, {size} bytes"
