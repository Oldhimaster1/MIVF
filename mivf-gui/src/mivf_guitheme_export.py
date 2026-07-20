from __future__ import annotations

import argparse
import configparser
import dataclasses
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Iterable
from .theme_export_c3 import build_state_assets

ROLE_SPECS = (
    # role, accepted project keys, tool, width, height, canonical suffix, required
    ("dashboard_bg", ("dashboard_bg", "dashboard_background"), "mivf_make_dashboard_bg.py", 320, 240, "dashboard_bg.mivfasset", True),
    ("rewind", ("rewind_underlay", "rewind", "control_rewind"), "mivf_make_control_asset.py", 64, 60, "rewind.mivfasset", True),
    ("play_pause", ("play_pause_underlay", "play_pause", "control_play_pause"), "mivf_make_control_asset.py", 74, 78, "play_pause.mivfasset", True),
    ("fast_forward", ("fast_forward_underlay", "forward_underlay", "fast_forward", "control_fast_forward"), "mivf_make_control_asset.py", 64, 60, "fast_forward.mivfasset", True),
    ("menu_back", ("menu_back", "back_underlay", "movie_menu_back", "control_back"), "mivf_make_control_asset.py", None, None, "menu_back.mivfasset", True),
)

class ThemeExportError(RuntimeError):
    pass

@dataclasses.dataclass(frozen=True)
class ExportedFile:
    role: str
    path: Path
    size: int
    sha256: str

@dataclasses.dataclass(frozen=True)
class ThemePackageResult:
    manifest: Path
    files: tuple[ExportedFile, ...]


def _hash(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def _load_project(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except Exception as exc:
        raise ThemeExportError(f"Cannot read project JSON: {exc}") from exc
    if not isinstance(data, dict):
        raise ThemeExportError("Project root must be a JSON object")
    schema = data.get("schema")
    if schema not in (None, "mivf-toolkit-project-v1"):
        raise ThemeExportError(f"Unsupported project schema: {schema!r}")
    return data


def _artwork(project: dict[str, Any]) -> dict[str, Any]:
    obj = project.get("artwork", {})
    return obj if isinstance(obj, dict) else {}


def _first(mapping: dict[str, Any], keys: Iterable[str]) -> str:
    for key in keys:
        value = mapping.get(key)
        if isinstance(value, str) and value.strip():
            return value.strip()
    return ""


def _resolve(project_path: Path, value: str) -> Path:
    p = Path(value).expanduser()
    if not p.is_absolute():
        p = project_path.parent / p
    p = p.resolve()
    if not p.is_file():
        raise ThemeExportError(f"Source artwork does not exist: {p}")
    return p


def _discover_cli(tool: Path, python: Path) -> str:
    proc = subprocess.run([str(python), str(tool), "--help"], capture_output=True, text=True, shell=False)
    text = (proc.stdout or "") + "\n" + (proc.stderr or "")
    return text.lower()


def _invoke(tool: Path, python: Path, source: Path, output: Path,
            width: int | None, height: int | None,
            control: str | None = None) -> None:
    help_text = _discover_cli(tool, python)
    # The Phase-C tools have existed in positional and named-argument revisions.
    # Select only from explicit argv layouts; never invoke a shell.
    candidates: list[list[str]] = []
    # Current Phase-C control converter requires a semantic --control value.
    # Dashboard generation remains unchanged.
    control_args = ["--control", control] if control else []
    if "--input" in help_text and "--output" in help_text:
        base = [str(python), str(tool)] + control_args + ["--input", str(source), "--output", str(output)]
        if width is not None and "--width" in help_text:
            base += ["--width", str(width)]
        if height is not None and "--height" in help_text:
            base += ["--height", str(height)]
        candidates.append(base)
    if "--source" in help_text and "--output" in help_text:
        base = [str(python), str(tool)] + control_args + ["--source", str(source), "--output", str(output)]
        if width is not None and "--width" in help_text:
            base += ["--width", str(width)]
        if height is not None and "--height" in help_text:
            base += ["--height", str(height)]
        candidates.append(base)
    # Known Phase-C positional contract is retained as a final compatibility path.
    positional = [str(python), str(tool)] + control_args + [str(source), str(output)]
    if width is not None and height is not None and "width" in help_text and "height" in help_text:
        candidates.append(positional + [str(width), str(height)])
    candidates.append(positional)

    errors: list[str] = []
    for argv in candidates:
        if output.exists():
            output.unlink()
        proc = subprocess.run(argv, capture_output=True, text=True, shell=False)
        if proc.returncode == 0 and output.is_file() and output.stat().st_size > 0:
            return
        errors.append(f"argv={argv!r}\nstdout={proc.stdout[-1000:]}\nstderr={proc.stderr[-1000:]}")
    raise ThemeExportError(f"Asset tool failed: {tool.name}\n" + "\n---\n".join(errors))


def _safe_name(name: str) -> str:
    if not name or name in (".", "..") or any(c in name for c in "\\/:*?\"<>|"):
        raise ThemeExportError(f"Unsafe output basename: {name!r}")
    return name


def _asset_ref(filename: str) -> str:
    """Return the bare runtime asset name; the player appends .mivfasset."""
    suffix = ".mivfasset"
    if not filename.endswith(suffix):
        raise ThemeExportError(f"Runtime asset filename lacks {suffix}: {filename}")
    bare = filename[:-len(suffix)]
    if not bare or any(c in bare for c in "\\/:*?\"<>|") or ".." in bare:
        raise ThemeExportError(f"Unsafe runtime asset reference: {filename!r}")
    return bare


def _rgb(value: Any, fallback: list[int]) -> list[int]:
    if isinstance(value, list) and len(value) == 3 and all(isinstance(x, int) and 0 <= x <= 255 for x in value):
        return value
    return fallback


def _manifest_text(project: dict[str, Any], names: dict[str, str]) -> str:
    """Compile the editable project to the exact Phase-C runtime grammar."""
    theme = project.get("theme", {})
    if not isinstance(theme, dict):
        theme = {}
    accent = _rgb(theme.get("accent_rgb", theme.get("accent")), [70, 120, 210])
    outline = _rgb(theme.get("outline_rgb", theme.get("outline")), [255, 255, 255])
    focused = [min(255, c + 40) for c in accent]
    refs = {role: _asset_ref(filename) for role, filename in names.items()}

    lines = [
        "MIVFTHEME_SCHEMA=1",
        "THEME_NAME=MIVF Toolkit Premiere Theme",
        "THEME_AUTHOR=MIVF Toolkit",
        "",
        f"DASHBOARD_BG={refs['dashboard_bg']}",
        f"PALETTE_ACCENT={accent[0]},{accent[1]},{accent[2]}",
        f"PALETTE_OUTLINE={outline[0]},{outline[1]},{outline[2]}",
        "",
    ]

    controls = (
        ("REWIND", "rewind", None, None),
        ("PLAY_PAUSE", "play_pause", accent, focused),
        ("FAST_FORWARD", "fast_forward", None, None),
        ("BACK", "menu_back", None, None),
    )
    for control, role, idle_fill, focused_fill in controls:
        for state, fill in (("IDLE", idle_fill), ("FOCUSED", focused_fill)):
            lines.extend((
                f"CONTROL={control}",
                f"CONTROL.STATE={state}",
                f"CONTROL.UNDERLAY={refs[role + '_' + state.lower()] if role + '_' + state.lower() in refs else refs[role]}",
            ))
            if fill is not None:
                lines.append(f"CONTROL.FILL={fill[0]},{fill[1]},{fill[2]}")
            lines.extend((
                f"CONTROL.OUTLINE={outline[0]},{outline[1]},{outline[2]}",
                "CONTROL.END",
                "",
            ))
    return "\r\n".join(lines)


def export_theme_package(project_file: str | Path, destination: str | Path,
                         basename: str | None = None, repo: str | Path | None = None,
                         python: str | Path | None = None) -> ThemePackageResult:
    project_path = Path(project_file).resolve()
    project = _load_project(project_path)
    destination = Path(destination).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    probe = destination / ".mivf_write_probe"
    try:
        probe.write_bytes(b"ok")
    finally:
        probe.unlink(missing_ok=True)
    basename = _safe_name(basename or project_path.stem)
    repo_path = Path(repo).resolve() if repo else Path(__file__).resolve().parents[3]
    python_path = Path(python).resolve() if python else Path(sys.executable).resolve()
    tools = repo_path / "tools"
    art = _artwork(project)
    names = {role: f"{basename}.{suffix}" for role, _, _, _, _, suffix, _ in ROLE_SPECS}
    final_manifest = destination / f"{basename}.mivftheme"
    final_outputs = [destination / names[spec[0]] for spec in ROLE_SPECS]

    with tempfile.TemporaryDirectory(prefix=f".{basename}.mivf-export-", dir=destination) as td:
        stage = Path(td)
        staged: list[tuple[str, Path, Path]] = []
        for role, aliases, tool_name, width, height, _, required in ROLE_SPECS:
            value = _first(art, aliases)
            if not value:
                if required:
                    raise ThemeExportError(f"Required artwork is unassigned: {role}; accepted project keys: {', '.join(aliases)}")
                continue
            source = _resolve(project_path, value)
            staged_path = stage / names[role]
            tool = tools / tool_name
            if not tool.is_file():
                raise ThemeExportError(f"Required tool does not exist: {tool}")
            control = {
                "rewind": "rewind",
                "play_pause": "play_pause",
                "fast_forward": "fast_forward",
                "menu_back": "movie_menu_back",
            }.get(role)
            _invoke(tool, python_path, source, staged_path, width, height, control)
            if staged_path.stat().st_size <= 0:
                raise ThemeExportError(f"Generated empty asset: {role}")
            staged.append((role, staged_path, destination / names[role]))

        # Phase C.3 state assets are rendered from nondestructive recipes; remove legacy control staging.
        staged = [x for x in staged if x[0] == 'dashboard_bg']
        state_names, state_files = build_state_assets(project, project_path, stage, basename)
        names.update(state_names)
        staged.extend((role, path, destination / path.name) for role, path in state_files)
        manifest_stage = stage / final_manifest.name
        manifest_stage.write_text(_manifest_text(project, names), encoding="ascii", newline="")
        text = manifest_stage.read_text(encoding="ascii")
        if ".." in text or str(project_path.parent) in text:
            raise ThemeExportError("Manifest contains an unsafe or absolute path")
        for role, _, _ in staged:
            reference = _asset_ref(names[role])
            if reference not in text:
                raise ThemeExportError(f"Manifest does not reference generated role: {role}")
        if ".mivfasset" in text:
            raise ThemeExportError("Manifest must contain bare asset references; the runtime appends .mivfasset")

        # Transactional promotion: preserve every replaced file until all staged files validate.
        rollback = stage / "rollback"
        rollback.mkdir()
        promoted: list[Path] = []
        targets = [(manifest_stage, final_manifest)] + [(s, f) for _, s, f in staged]
        try:
            for _, final in targets:
                if final.exists():
                    shutil.copy2(final, rollback / final.name)
            for source, final in targets:
                os.replace(source, final)
                promoted.append(final)
        except Exception:
            for final in promoted:
                final.unlink(missing_ok=True)
            for old in rollback.iterdir():
                os.replace(old, destination / old.name)
            raise

    results = tuple(ExportedFile(role, final, final.stat().st_size, _hash(final))
                    for role, _, final in staged)
    return ThemePackageResult(final_manifest, (ExportedFile("manifest", final_manifest,
        final_manifest.stat().st_size, _hash(final_manifest)),) + results)


def format_report(result: ThemePackageResult) -> str:
    lines = [f"Theme package exported: {result.manifest.parent}", ""]
    for item in result.files:
        lines.append(f"{item.role:16} {item.path.name:40} {item.size:10d} bytes  {item.sha256}")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="Export a complete MIVF runtime theme package")
    p.add_argument("project", type=Path)
    p.add_argument("destination", type=Path)
    p.add_argument("--basename")
    p.add_argument("--repo", type=Path)
    p.add_argument("--python", type=Path)
    ns = p.parse_args(argv)
    try:
        result = export_theme_package(ns.project, ns.destination, ns.basename, ns.repo, ns.python)
    except ThemeExportError as exc:
        p.error(str(exc))
    print(format_report(result))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())