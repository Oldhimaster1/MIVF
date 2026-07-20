"""Editable MIVF Toolkit project format ("*.mivfproj").

Implements mivf_customization_gui_20260716/ENCODER_GUI_PROJECT_SCHEMA.md.
Versioned JSON, schema name mirrors the real E0 job-recovery file's own
convention (encode_mivf.py:1023, "schema":"mivf-encode-job-v1") -- same
naming pattern, same fail-safe-on-mismatch philosophy: an unrecognized
schema value is refused outright, never partially interpreted.

Desktop-only. Never touches the 3DS; not the same format as the runtime
".mivftheme" manifest (source/mivf_customization.c) -- see
CUSTOMIZATION_SCHEMA.md for why those are deliberately different formats.
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any

SCHEMA = "mivf-toolkit-project-v1"
TOOL_VERSION = "0.1.0"


class ProjectSchemaError(Exception):
    """Raised when a .mivfproj file's schema is missing or unrecognized.
    Deliberately not caught silently anywhere -- fail-safe over guessing."""


@dataclass
class ProjectArtwork:
    cover: str | None = None
    preview_cover: str | None = None
    menu_bg: str | None = None
    screensaver: str | None = None
    dashboard_bg: str | None = None       # Phase C: source image, converted via mivf_make_dashboard_bg.py
    fast_forward_underlay: str | None = None  # source image, converted via mivf_make_control_asset.py --control fast_forward
    play_pause_underlay: str | None = None    # source image, converted via mivf_make_control_asset.py --control play_pause
    movie_menu_back: str | None = None        # Phase C.1: source image, --control movie_menu_back (real, functional Back row)
    rewind_underlay: str | None = None        # Phase C.1: source image, --control rewind (real dashboard control, same size as fast_forward)
    # Phase C.1: per-slot desktop authoring fit mode ("contain"/"cover"/
    # "stretch"/"center_crop", see asset_pipeline.FitMode), keyed by the
    # field names above. Missing key -> "contain" (Phase C's original,
    # unchanged default). Runtime assets stay exact-dimension/preconverted
    # either way -- this only controls how the desktop tool prepares them.
    fit_modes: dict[str, str] = field(default_factory=dict)
    control_edits: dict[str, Any] = field(default_factory=dict)


@dataclass
class ProjectTheme:
    accent_rgb: tuple[int, int, int] | None = None
    outline_rgb: tuple[int, int, int] | None = None
    back_fill_rgb: tuple[int, int, int] | None = None  # Phase C.1: optional Back-row fill override


@dataclass
class MivfProject:
    schema: str = SCHEMA
    tool_version: str = TOOL_VERSION
    source_media: str | None = None
    output_path: str | None = None
    preset: str = "balanced"  # one of PRESET_NAMES in presets.py
    advanced_overrides: dict[str, Any] = field(default_factory=dict)
    artwork: ProjectArtwork = field(default_factory=ProjectArtwork)
    theme: ProjectTheme = field(default_factory=ProjectTheme)
    job_dir: str | None = None
    resume_job: bool = False
    project_path: Path | None = None  # not serialized; set on load/save for relative-path resolution

    def to_dict(self) -> dict[str, Any]:
        d = asdict(self)
        d.pop("project_path", None)
        return d

    @staticmethod
    def from_dict(d: dict[str, Any]) -> "MivfProject":
        if d.get("schema") != SCHEMA:
            raise ProjectSchemaError(
                f"unrecognized project schema {d.get('schema')!r}; expected {SCHEMA!r}. "
                "Refusing to guess-parse an incompatible or newer project file."
            )
        # Forward from an OLDER (Phase C, pre-C.1) artwork dict: it simply
        # lacks movie_menu_back/fit_modes, and ProjectArtwork's own
        # defaults fill them in -- an old .mivfproj loads unchanged.
        artwork_raw = dict(d.get("artwork", {}))
        artwork_raw.setdefault("movie_menu_back", None)
        artwork_raw.setdefault("rewind_underlay", None)
        artwork_raw.setdefault("fit_modes", {})
        artwork_raw.setdefault("control_edits", {})
        artwork = ProjectArtwork(**artwork_raw)
        theme_raw = d.get("theme", {})
        theme = ProjectTheme(
            accent_rgb=tuple(theme_raw["accent_rgb"]) if theme_raw.get("accent_rgb") else None,
            outline_rgb=tuple(theme_raw["outline_rgb"]) if theme_raw.get("outline_rgb") else None,
            back_fill_rgb=tuple(theme_raw["back_fill_rgb"]) if theme_raw.get("back_fill_rgb") else None,
        )
        return MivfProject(
            schema=d["schema"],
            tool_version=d.get("tool_version", TOOL_VERSION),
            source_media=d.get("source_media"),
            output_path=d.get("output_path"),
            preset=d.get("preset", "balanced"),
            advanced_overrides=dict(d.get("advanced_overrides", {})),
            artwork=artwork,
            theme=theme,
            job_dir=d.get("job_dir"),
            resume_job=bool(d.get("resume_job", False)),
        )

    def save(self, path: Path) -> None:
        path = Path(path)
        path.write_text(json.dumps(self.to_dict(), indent=2, sort_keys=False), encoding="utf-8")
        self.project_path = path

    @staticmethod
    def load(path: Path) -> "MivfProject":
        path = Path(path)
        raw = json.loads(path.read_text(encoding="utf-8"))
        project = MivfProject.from_dict(raw)
        project.project_path = path
        return project

    def resolve(self, rel_path: str | None) -> Path | None:
        """Resolve a stored relative path against this project's own directory
        (portable-project rule from ENCODER_GUI_PROJECT_SCHEMA.md)."""
        if not rel_path:
            return None
        p = Path(rel_path)
        if p.is_absolute() or self.project_path is None:
            return p
        return (self.project_path.parent / p).resolve()

    def missing_files(self) -> list[str]:
        """Relink check: which recorded paths don't exist on disk right now."""
        missing = []
        for label, rel in (
            ("source_media", self.source_media),
            ("artwork.cover", self.artwork.cover),
            ("artwork.preview_cover", self.artwork.preview_cover),
            ("artwork.dashboard_bg", self.artwork.dashboard_bg),
            ("artwork.fast_forward_underlay", self.artwork.fast_forward_underlay),
            ("artwork.play_pause_underlay", self.artwork.play_pause_underlay),
            ("artwork.movie_menu_back", self.artwork.movie_menu_back),
            ("artwork.rewind_underlay", self.artwork.rewind_underlay),
        ):
            if rel:
                resolved = self.resolve(rel)
                if resolved and not resolved.exists():
                    missing.append(label)
        return missing
