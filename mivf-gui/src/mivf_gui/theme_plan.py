"""Phase C.5a: PackagePlan -- the single read-only model that Check Project,
Export Dry Run, Change Summary, and the real transactional exporter all
build from. Resolving/rendering/hashing happens exactly once, here; every
consumer is a view over the same PackagePlan object, so a Preflight report
can never say "OK" to something the real exporter then refuses.

Everything in this module is read-only with respect to the user's project
and source artwork. The only filesystem writes are into a caller-managed
staging directory (a TemporaryDirectory); nothing is ever promoted into the
real destination folder from here -- that stays theme_export.py's job.
"""
from __future__ import annotations

import contextlib
import dataclasses
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Iterable

from . import control_recipe, background_recipe
from .theme_export_c3 import build_state_assets, LEG as STATE_LEG

# Real runtime constants, cited from source/mivf_customization.c -- kept in
# exactly one place so a preflight "will this fit" check can never silently
# drift from the actual static-pool budget the player enforces.
MIVF_CUST_MANIFEST_MAX_BYTES = 4096
MIVF_CUST_NAME_MAX = 48
MIVF_CUST_PATH_MAX = 512

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
class ValidationMessage:
    severity: str  # "ERROR" | "WARNING" | "INFO"
    category: str
    message: str
    role: str | None = None


@dataclasses.dataclass(frozen=True)
class PlannedFile:
    role: str               # "manifest" | "dashboard_bg" | "rewind_idle" | "rewind_focused" | ...
    filename: str
    size: int
    sha256: str
    status: str              # "added" | "changed" | "unchanged" | "removed"
    control: str | None = None
    state: str | None = None
    width: int | None = None
    height: int | None = None
    source: str | None = None
    recipe: dict | None = None
    dedup_of: str | None = None   # filename of the file whose bytes this one reuses, if deduped
    stage_path: Path | None = None  # None once the staging directory is gone (read-only plan mode)


@dataclasses.dataclass(frozen=True)
class PackagePlan:
    project_path: Path
    destination: Path
    basename: str
    files: tuple[PlannedFile, ...]
    messages: tuple[ValidationMessage, ...]
    manifest_text: str
    estimated_runtime_bytes: int

    @property
    def errors(self) -> tuple[ValidationMessage, ...]:
        return tuple(m for m in self.messages if m.severity == "ERROR")

    @property
    def warnings(self) -> tuple[ValidationMessage, ...]:
        return tuple(m for m in self.messages if m.severity == "WARNING")

    @property
    def infos(self) -> tuple[ValidationMessage, ...]:
        return tuple(m for m in self.messages if m.severity == "INFO")

    @property
    def ok_to_export(self) -> bool:
        return not self.errors


def _hash_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _hash_file(path: Path) -> str:
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


def _safe_name(name: str) -> str:
    if not name or name in (".", "..") or any(c in name for c in "\\/:*?\"<>|"):
        raise ThemeExportError(f"Unsafe output basename: {name!r}")
    return name


def _asset_ref(filename: str) -> str:
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


def _discover_cli(tool: Path, python: Path) -> str:
    proc = subprocess.run([str(python), str(tool), "--help"], capture_output=True, text=True, shell=False)
    text = (proc.stdout or "") + "\n" + (proc.stderr or "")
    return text.lower()


def _invoke(tool: Path, python: Path, source: Path, output: Path,
            width: int | None, height: int | None,
            control: str | None = None) -> None:
    help_text = _discover_cli(tool, python)
    candidates: list[list[str]] = []
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


def _manifest_text(project: dict[str, Any], names: dict[str, str]) -> str:
    """Compile the editable project to the exact Phase-C runtime grammar.
    Identical logic to the pre-C.5a theme_export.py version -- moved here so
    both PackagePlan and the real exporter build the manifest exactly once."""
    theme = project.get("theme", {})
    if not isinstance(theme, dict):
        theme = {}
    accent = _rgb(theme.get("accent_rgb", theme.get("accent")), [70, 120, 210])
    outline = _rgb(theme.get("outline_rgb", theme.get("outline")), [255, 255, 255])
    focused = [min(255, c + 40) for c in accent]
    refs = {role: _asset_ref(filename) for role, filename in names.items()}
    # A role that failed to render (missing source, etc.) is simply absent
    # from `names`/`refs` -- write a blank reference rather than crashing,
    # so the rest of the manifest still builds and _validate_manifest's
    # missing-reference / incomplete-reference checks can report it as one
    # of potentially several problems in the same Preflight pass.

    lines = [
        "MIVFTHEME_SCHEMA=1",
        "THEME_NAME=MIVF Toolkit Premiere Theme",
        "THEME_AUTHOR=MIVF Toolkit",
        "",
    ]
    if refs.get("dashboard_bg"):
        lines.append(f"DASHBOARD_BG={refs['dashboard_bg']}")
    lines.extend([
        f"PALETTE_ACCENT={accent[0]},{accent[1]},{accent[2]}",
        f"PALETTE_OUTLINE={outline[0]},{outline[1]},{outline[2]}",
        "",
    ])

    controls = (
        ("REWIND", "rewind", None, None),
        ("PLAY_PAUSE", "play_pause", accent, focused),
        ("FAST_FORWARD", "fast_forward", None, None),
        ("BACK", "movie_menu_back", None, None),
    )
    # C.6: state-independent per-control position offset (dx, dy), Premiere
    # dashboard only -- BACK (the DVD-menu control, not part of the C.6
    # canvas) never gets a position key even if present in the dict, since
    # the runtime resolver (mivf_customization_resolve_position) never
    # looks one up for it.
    layout = project.get("dashboard_layout", {})
    if not isinstance(layout, dict):
        layout = {}
    for control, role, idle_fill, focused_fill in controls:
        for state_name, fill in (("IDLE", idle_fill), ("FOCUSED", focused_fill)):
            lines.extend((
                f"CONTROL={control}",
                f"CONTROL.STATE={state_name}",
                f"CONTROL.UNDERLAY={refs.get(role + '_' + state_name.lower(), refs.get(role, ''))}",
            ))
            if fill is not None:
                lines.append(f"CONTROL.FILL={fill[0]},{fill[1]},{fill[2]}")
            lines.append(f"CONTROL.OUTLINE={outline[0]},{outline[1]},{outline[2]}")
            if control != "BACK" and state_name == "IDLE" and control in layout:
                dx, dy = layout[control]
                lines.append(f"CONTROL.POSITION={int(dx)},{int(dy)}")
            lines.extend((
                "CONTROL.END",
                "",
            ))
    return "\r\n".join(lines)


# C.6: mirrors mivf_c25_premiere_controls' real geometry (source/main.c) --
# base center (x, y) and disc radius per control, on the real 320x240
# bottom-screen dashboard canvas. Deliberately duplicated from C, not
# imported (there is no shared build step between the Toolkit and the
# player) -- matches this codebase's own established, documented-duplication
# pattern (see mivf_customization.c's own header comment on the same
# tradeoff for its sidecar-path and CVD-table duplication). If
# mivf_c25_premiere_controls' base x[3]/y or disc_r values ever change,
# these must be updated to match or validation silently drifts from the
# real runtime clamp.
DASHBOARD_CANVAS_W = 320
DASHBOARD_CANVAS_H = 240
PREMIERE_CONTROL_GEOMETRY = {
    # control: (base_x, base_y, disc_r)
    "REWIND": (66, 128, 27),
    "PLAY_PAUSE": (160, 128, 37),
    "FAST_FORWARD": (254, 128, 27),
}


def validate_dashboard_layout(layout: dict[str, tuple[int, int]]) -> list[ValidationMessage]:
    """Real, computable checks against the player's actual clamp geometry --
    off-canvas, overlap, and out-of-range offsets. Pure function, no I/O, so
    both PackagePlan and the live Toolkit editor call this exact same rule
    set (single source of truth for the RULES; the geometry constants above
    are the one unavoidable, documented duplication from C)."""
    out: list[ValidationMessage] = []
    centers: dict[str, tuple[int, int, int]] = {}

    # Overlap must be checked against every real control's actual resolved
    # position, including ones the user never explicitly moved (dx=dy=0) --
    # dragging Rewind onto Play/Pause's default spot is a real overlap even
    # though Play/Pause itself has no override entry in `layout`.
    for control, (base_x, base_y, r) in PREMIERE_CONTROL_GEOMETRY.items():
        dx, dy = layout.get(control, (0, 0))
        centers[control] = (base_x + dx, base_y + dy, r)

    for control, (dx, dy) in layout.items():
        if control not in PREMIERE_CONTROL_GEOMETRY:
            out.append(ValidationMessage("ERROR", "layout_unsupported_control",
                                          f"{control} is not a supported C.6 control (Rewind/Play-Pause/Fast Forward only)",
                                          role=control))
            continue
        if not (-160 <= dx <= 160 and -120 <= dy <= 120):
            out.append(ValidationMessage("ERROR", "layout_offset_out_of_range",
                                          f"{control} offset ({dx},{dy}) exceeds the +/-160,+/-120 sanity range",
                                          role=control))
            del centers[control]
            continue
        cx, cy, r = centers[control]
        margin = r + 3
        if cx < margin or cx > DASHBOARD_CANVAS_W - margin or cy < margin or cy > DASHBOARD_CANVAS_H - margin:
            out.append(ValidationMessage("ERROR", "layout_offscreen",
                                          f"{control} would be drawn partly off the {DASHBOARD_CANVAS_W}x{DASHBOARD_CANVAS_H} "
                                          "dashboard canvas at this offset (the player clamps it back on-screen at "
                                          "runtime, so this warns about a mismatch between the Toolkit preview and "
                                          "what actually renders, not a crash)",
                                          role=control))

    checked = list(centers.items())
    for i in range(len(checked)):
        name_a, (xa, ya, ra) = checked[i]
        for j in range(i + 1, len(checked)):
            name_b, (xb, yb, rb) = checked[j]
            dist = ((xa - xb) ** 2 + (ya - yb) ** 2) ** 0.5
            if dist < (ra + rb):
                out.append(ValidationMessage("WARNING", "layout_overlap",
                                              f"{name_a} and {name_b} overlap at these offsets (touch targets may be "
                                              "hard to hit precisely)",
                                              role=f"{name_a}+{name_b}"))
    return out


def _validate_manifest(manifest_text: str, project_path: Path, names: dict[str, str],
                        planned_roles: set[str]) -> list[ValidationMessage]:
    """The three self-checks the exporter has always run, as collected
    messages instead of immediate raises, plus grammar/shape checks a real
    Preflight should surface (block count, no stray suffix, references
    matching a planned output)."""
    out: list[ValidationMessage] = []
    if ".." in manifest_text or str(project_path.parent) in manifest_text:
        out.append(ValidationMessage("ERROR", "manifest_unsafe_path",
                                      "Manifest contains an unsafe or absolute path"))
    if ".mivfasset" in manifest_text:
        out.append(ValidationMessage("ERROR", "manifest_suffix_embedded",
                                      "Manifest must contain bare asset references; the runtime appends .mivfasset"))
    for role in planned_roles:
        if role not in names:
            continue
        try:
            reference = _asset_ref(names[role])
        except ThemeExportError as e:
            out.append(ValidationMessage("ERROR", "manifest_unsafe_reference", str(e), role=role))
            continue
        if reference not in manifest_text:
            out.append(ValidationMessage("ERROR", "manifest_missing_reference",
                                          f"Manifest does not reference generated role: {role}", role=role))
    if "CONTROL.UNDERLAY=\r\n" in manifest_text or manifest_text.endswith("CONTROL.UNDERLAY="):
        out.append(ValidationMessage("ERROR", "manifest_incomplete_reference",
                                      "Manifest has a blank CONTROL.UNDERLAY= reference "
                                      "(a required control asset failed to render)"))
    block_count = manifest_text.count("CONTROL.END")
    if block_count != 8:
        out.append(ValidationMessage("ERROR", "manifest_block_count",
                                      f"Manifest has {block_count} CONTROL.END blocks; expected exactly 8 "
                                      "(4 controls x Idle/Focused)"))
    manifest_bytes = len(manifest_text.encode("ascii"))
    if manifest_bytes > MIVF_CUST_MANIFEST_MAX_BYTES:
        out.append(ValidationMessage("ERROR", "manifest_budget",
                                      f"Manifest is {manifest_bytes} bytes, exceeding the runtime "
                                      f"{MIVF_CUST_MANIFEST_MAX_BYTES}-byte budget (MIVF_CUST_MANIFEST_MAX_BYTES)"))
    return out


def _mask_coverage_ratio(recipe: dict, control: str, project_path: Path) -> float | None:
    """Fraction of pixels the final hard 1-bit mask marks visible (0..1), or
    None if the recipe's source can't be rendered at all (already reported
    elsewhere as a render failure)."""
    try:
        base = project_path.parent
        r = control_recipe.render(control, recipe, base)
    except Exception:
        return None
    binary = r["binary"]
    total = binary.width * binary.height
    if total == 0:
        return None
    visible = sum(1 for v in binary.getdata() if v)
    return visible / total


def _advisory_checks(details: dict[str, dict], project_path: Path) -> list[ValidationMessage]:
    """Data-only Idle/Focused-similarity and mask-coverage checks. Icon-
    coverage / disc-clip / Back-label-conflict geometry checks land with the
    canvas in Phase C.6 (they need the same Geometry-Overlay Model that
    powers the live overlays there) -- this pass covers what's meaningfully
    checkable without it."""
    out: list[ValidationMessage] = []
    for control in ("rewind", "play_pause", "fast_forward", "movie_menu_back"):
        idle = details.get(f"{control}_idle")
        focused = details.get(f"{control}_focused")
        if not idle or not focused:
            continue
        # Similarity heuristic runs whether or not the bytes are identical --
        # identical bytes ARE the strongest form of "hard to distinguish"
        # (Focused inheriting Idle unchanged is a valid, common choice, so
        # this is a WARNING, not an error, and only fires when Focused was
        # actually authored independently rather than left to inherit).
        idle_recipe, focused_recipe = idle["recipe"], focused["recipe"]
        numeric_fields = ("scale", "x", "y", "brightness", "contrast", "saturation", "tint_strength")
        same_source = idle_recipe.get("source") == focused_recipe.get("source")
        near_identical_adjustments = all(
            abs(float(idle_recipe.get(f, 0)) - float(focused_recipe.get(f, 0))) < 0.05 for f in numeric_fields
        )
        if idle["sha"] != focused["sha"] and same_source and near_identical_adjustments:
            out.append(ValidationMessage(
                "WARNING", "state_similarity",
                f"{control.replace('_', ' ').title()} Focused is difficult to distinguish from Idle "
                "(same source, near-identical adjustments) -- players may not notice which control has focus.",
                role=f"{control}_focused",
            ))
        for state_name, d in (("idle", idle), ("focused", focused)):
            ratio = _mask_coverage_ratio(d["recipe"], control, project_path)
            if ratio is None:
                continue
            if ratio <= 0.0:
                out.append(ValidationMessage(
                    "WARNING", "empty_mask",
                    f"{control.replace('_', ' ').title()} {state_name.title()} mask is completely empty -- "
                    "nothing will be visible; the built-in control face will show through entirely.",
                    role=f"{control}_{state_name}",
                ))
            elif ratio >= 0.999:
                out.append(ValidationMessage(
                    "INFO", "full_mask",
                    f"{control.replace('_', ' ').title()} {state_name.title()} mask is fully opaque "
                    "(equivalent to a full-rectangle mask).",
                    role=f"{control}_{state_name}",
                ))
    return out


def _classify(filename: str, sha: str, destination: Path) -> str:
    existing = destination / filename
    if not existing.is_file():
        return "added"
    try:
        return "unchanged" if _hash_file(existing) == sha else "changed"
    except OSError:
        return "changed"


def _find_removed(destination: Path, basename: str, kept_filenames: set[str]) -> list[PlannedFile]:
    """Advisory only -- never deletes anything. Flags files from a prior
    export of this same basename that the current plan no longer produces."""
    removed: list[PlannedFile] = []
    if not destination.is_dir():
        return removed
    prefix = f"{basename}."
    for existing in sorted(destination.glob(f"{prefix}*")):
        if not existing.is_file() or existing.name in kept_filenames:
            continue
        if existing.suffix not in (".mivfasset", ".mivftheme"):
            continue
        try:
            sha = _hash_file(existing)
            size = existing.stat().st_size
        except OSError:
            continue
        removed.append(PlannedFile(role="removed", filename=existing.name, size=size, sha256=sha,
                                    status="removed"))
    return removed


def _build_plan(project_path: Path, destination: Path, basename: str,
                 repo_path: Path, python_path: Path, stage: Path) -> PackagePlan:
    messages: list[ValidationMessage] = []
    try:
        project = _load_project(project_path)
    except ThemeExportError as e:
        return PackagePlan(project_path=project_path, destination=destination, basename=basename,
                            files=(), messages=(ValidationMessage("ERROR", "project_schema", str(e)),),
                            manifest_text="", estimated_runtime_bytes=0)

    art = _artwork(project)
    tools = repo_path / "tools"
    names: dict[str, str] = {}
    planned: list[PlannedFile] = []

    if not destination.exists():
        messages.append(ValidationMessage("INFO", "destination", f"Destination folder does not yet exist: {destination}"))

    # --- dashboard background: C.5b.1 shared recipe path ---
    mode = background_recipe.effective_mode(art)
    if mode not in background_recipe.MODES:
        messages.append(ValidationMessage("ERROR", "background_mode", f"Unsupported dashboard background mode: {mode!r}", role="dashboard_bg"))
    elif mode != "builtin":
        filename = f"{basename}.dashboard_bg.mivfasset"
        source_value = _first(art, ("dashboard_bg", "dashboard_background"))
        theme = project.get("theme", {}) if isinstance(project.get("theme", {}), dict) else {}
        accent = _rgb(theme.get("accent_rgb", theme.get("accent")), [70, 120, 210])
        try:
            rendered = background_recipe.render(mode, source_value or None, art.get("dashboard_bg_recipe", {}), accent, project_path.parent)
        except Exception as e:
            category = "missing_source" if mode == "custom" else "background_render"
            messages.append(ValidationMessage("ERROR", category, f"Dashboard Background could not be rendered: {e}", role="dashboard_bg"))
        else:
            staged_path = stage / filename
            staged_path.write_bytes(rendered["asset"])
            names["dashboard_bg"] = filename
            planned.append(PlannedFile(role="dashboard_bg", filename=filename, size=len(rendered["asset"]), sha256=rendered["sha"], status=_classify(filename, rendered["sha"], destination), width=320, height=240, source=rendered["source"], recipe={"mode": mode, **rendered["recipe"]}, stage_path=staged_path))

    # --- control Idle/Focused states: the one real render path ---
    details: dict[str, dict] = {}
    try:
        state_names, _state_files, details, state_errors = build_state_assets(project, project_path, stage, basename)
        names.update(state_names)
        for key, reason in state_errors.items():
            control, state_name = key.rsplit("_", 1)
            category = "missing_source" if "Missing artwork" in reason or "does not exist" in reason else "render_failure"
            messages.append(ValidationMessage(
                "ERROR", category,
                f"{control.replace('_', ' ').title()} {state_name.title()} could not be rendered: {reason}",
                role=key,
            ))
    except Exception as e:  # noqa: BLE001 -- must surface, never crash a preflight pass
        messages.append(ValidationMessage("ERROR", "render_failure", f"Failed to render control assets: {e}"))

    for key, d in details.items():
        filename = d["filename"]
        stage_path = stage / filename
        planned.append(PlannedFile(
            role=key, filename=filename, size=d["size"], sha256=d["sha"],
            status=_classify(filename, d["sha"], destination),
            control=d["control"], state=d["state"], width=d["w"], height=d["h"],
            source=d["recipe"].get("source"), recipe=d["recipe"],
            dedup_of=None if d["is_new"] else _first_owner(details, d["sha"], key),
            stage_path=stage_path if stage_path.is_file() else None,
        ))

    # --- manifest ---
    manifest_text = _manifest_text(project, names)
    manifest_filename = f"{basename}.mivftheme"
    manifest_bytes_content = manifest_text.encode("ascii")
    manifest_sha = _hash_bytes(manifest_bytes_content)
    manifest_stage_path = stage / manifest_filename
    manifest_stage_path.write_bytes(manifest_bytes_content)
    planned.insert(0, PlannedFile(
        role="manifest", filename=manifest_filename, size=len(manifest_bytes_content), sha256=manifest_sha,
        status=_classify(manifest_filename, manifest_sha, destination), stage_path=manifest_stage_path,
    ))

    planned_roles = {pf.role for pf in planned}
    messages.extend(_validate_manifest(manifest_text, project_path, names, planned_roles))
    messages.extend(_advisory_checks(details, project_path))
    layout = project.get("dashboard_layout", {})
    if isinstance(layout, dict) and layout:
        messages.extend(validate_dashboard_layout({k: tuple(v) for k, v in layout.items()}))

    kept_filenames = {pf.filename for pf in planned}
    removed = _find_removed(destination, basename, kept_filenames)

    estimated_runtime_bytes = sum(pf.size for pf in planned if pf.role != "manifest")

    return PackagePlan(
        project_path=project_path, destination=destination, basename=basename,
        files=tuple(planned) + tuple(removed), messages=tuple(messages),
        manifest_text=manifest_text, estimated_runtime_bytes=estimated_runtime_bytes,
    )


def _first_owner(details: dict[str, dict], sha: str, exclude_key: str) -> str | None:
    for key, d in details.items():
        if key != exclude_key and d["sha"] == sha and d["is_new"]:
            return d["filename"]
    return None


def build_plan(project_file: str | Path, destination: str | Path,
               basename: str | None = None, repo: str | Path | None = None,
               python: str | Path | None = None) -> PackagePlan:
    """Read-only entry point for Check Project / Export Dry Run / Change
    Summary. Never writes into `destination`; stages into a throwaway temp
    directory that is cleaned up before returning (all metadata needed by
    callers -- hashes, sizes, resolved recipes -- is already captured into
    the returned dataclasses by then)."""
    project_path = Path(project_file).resolve()
    destination = Path(destination).resolve()
    basename = _safe_name(basename or project_path.stem)
    repo_path = Path(repo).resolve() if repo else Path(__file__).resolve().parents[3]
    python_path = Path(python).resolve() if python else Path(sys.executable).resolve()
    with tempfile.TemporaryDirectory(prefix=f".{basename}.mivf-plan-") as stage:
        plan = _build_plan(project_path, destination, basename, repo_path, python_path, Path(stage))
        # Strip stage_path references before the directory disappears -- a
        # read-only plan must never carry a path into a directory that no
        # longer exists.
        cleared = tuple(dataclasses.replace(pf, stage_path=None) for pf in plan.files)
        return dataclasses.replace(plan, files=cleared)


@contextlib.contextmanager
def staged_plan_for_export(project_file: str | Path, destination: str | Path,
                            basename: str | None = None, repo: str | Path | None = None,
                            python: str | Path | None = None):
    """Used only by theme_export.export_theme_package(): builds the exact
    same PackagePlan as build_plan(), but keeps the staging directory alive
    for the duration of the `with` block so the real exporter can promote
    the identical staged bytes the plan was built from -- never a second,
    possibly-divergent render pass."""
    project_path = Path(project_file).resolve()
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
    with tempfile.TemporaryDirectory(prefix=f".{basename}.mivf-export-", dir=destination) as stage:
        plan = _build_plan(project_path, destination, basename, repo_path, python_path, Path(stage))
        yield plan, Path(stage)


# --- "Check Project" report formatting -------------------------------------
# A small ordered checklist of derived pass/fail facts, each keyed to the
# ValidationMessage categories that would make it fail. Purely a display
# convenience over the same PackagePlan -- no new information.
_CHECKLIST: tuple[tuple[str, tuple[str, ...]], ...] = (
    ("Project schema recognized", ("project_schema",)),
    ("All required source artwork found", ("missing_source", "missing_tool")),
    ("Every control state rendered", ("render_failure", "empty_asset", "tool_failure")),
    ("Manifest grammar is valid", ("manifest_unsafe_path", "manifest_suffix_embedded",
                                    "manifest_block_count", "manifest_unsafe_reference")),
    ("Every manifest reference matches a planned file", ("manifest_missing_reference",
                                                          "manifest_incomplete_reference")),
    ("Manifest fits the runtime budget", ("manifest_budget",)),
)


def format_check_project_report(plan: PackagePlan) -> str:
    """The plain-language "Check Project" report -- one view over PackagePlan,
    alongside Export Dry Run (per-file table) and Change Summary (status
    grouping), all built from the same messages/files."""
    error_categories = {m.category for m in plan.errors}
    lines: list[str] = []
    lines.append("PROJECT READY" if plan.ok_to_export else
                  f"{len(plan.errors)} ERROR(S) FOUND -- EXPORT BLOCKED")
    for label, categories in _CHECKLIST:
        passed = not (error_categories & set(categories))
        lines.append(f"{'✓' if passed else '✖'} {label}")

    dedup_count = sum(1 for pf in plan.files if pf.dedup_of and pf.role != "manifest")
    if dedup_count:
        lines.append(f"✓ {dedup_count} identical asset(s) will be reused")
    manifest_pf = next((pf for pf in plan.files if pf.role == "manifest"), None)
    if manifest_pf:
        lines.append(f"✓ Estimated runtime allocation: {plan.estimated_runtime_bytes:,} bytes "
                      f"(manifest {manifest_pf.size:,} / {MIVF_CUST_MANIFEST_MAX_BYTES:,} bytes)")

    if plan.errors:
        lines.append("")
        lines.append("Errors:")
        for m in plan.errors:
            lines.append(f"✖ {m.message}")
    if plan.warnings:
        lines.append("")
        lines.append("Warnings:")
        for m in plan.warnings:
            lines.append(f"⚠ {m.message}")
    if plan.infos:
        lines.append("")
        lines.append("Info:")
        for m in plan.infos:
            lines.append(f"ℹ {m.message}")
    return "\n".join(lines)


def format_change_summary(plan: PackagePlan) -> str:
    """Changed/Unchanged/Added/Removed, grouped by hash-based status --
    the same PlannedFile.status the dry-run table shows, just regrouped."""
    def _label(pf: PlannedFile) -> str:
        if pf.role == "manifest":
            return "Manifest"
        if pf.role == "dashboard_bg":
            return "Dashboard Background"
        if pf.control:
            return f"{pf.control.replace('_', ' ').title()} {pf.state.title()}" if pf.state else pf.control.title()
        return pf.role.replace("_", " ").title()

    groups: dict[str, list[str]] = {"changed": [], "unchanged": [], "added": [], "removed": []}
    for pf in plan.files:
        groups.setdefault(pf.status, []).append(_label(pf))

    lines: list[str] = []
    for status, title in (("changed", "Changed"), ("unchanged", "Unchanged"),
                           ("added", "Added"), ("removed", "Removed")):
        lines.append(f"{title}:")
        items = groups.get(status) or []
        if items:
            lines.extend(f"• {name}" for name in items)
        else:
            lines.append("• (none)")
        lines.append("")
    return "\n".join(lines).rstrip()
