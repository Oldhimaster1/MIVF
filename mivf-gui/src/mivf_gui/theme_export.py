from __future__ import annotations

import argparse
import dataclasses
import os
import shutil
from pathlib import Path

from . import theme_plan
from .theme_plan import ThemeExportError, ROLE_SPECS  # re-exported for existing importers/tests

__all__ = ["ThemeExportError", "ROLE_SPECS", "ExportedFile", "ThemePackageResult",
           "export_theme_package", "format_report", "main"]


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


def export_theme_package(project_file: str | Path, destination: str | Path,
                         basename: str | None = None, repo: str | Path | None = None,
                         python: str | Path | None = None) -> ThemePackageResult:
    """Real, transactional export. Phase C.5a: the resolve/render/hash/
    manifest/validate work all happens exactly once, inside
    theme_plan.staged_plan_for_export() -- the same PackagePlan a Preflight
    or Dry Run would see. This function's only remaining job is: refuse if
    the plan has any ERROR, otherwise promote the exact staged bytes the
    plan was built from into `destination`, with the same copy-aside/
    rollback-on-any-exception transaction as before Phase C.5a."""
    with theme_plan.staged_plan_for_export(project_file, destination, basename, repo, python) as (plan, stage):
        if not plan.ok_to_export:
            details = "\n".join(f"[{m.severity}] {m.category}: {m.message}" for m in plan.errors)
            raise ThemeExportError(
                f"Project has {len(plan.errors)} validation error(s); export refused:\n{details}"
            )

        destination_path = plan.destination
        rollback = stage / "rollback"
        rollback.mkdir()
        promoted: list[Path] = []
        # Several PlannedFiles can share one physical staged file (dedup --
        # e.g. Rewind Idle and Focused rendering identically); promote each
        # unique (stage_path -> destination) pair exactly once.
        seen_filenames: set[str] = set()
        targets: list[tuple[Path, Path]] = []
        for pf in plan.files:
            if pf.status == "removed" or pf.filename in seen_filenames:
                continue
            seen_filenames.add(pf.filename)
            targets.append((pf.stage_path, destination_path / pf.filename))
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
                os.replace(old, destination_path / old.name)
            raise

        manifest_pf = next(pf for pf in plan.files if pf.role == "manifest")
        other_pfs = [pf for pf in plan.files if pf.role != "manifest" and pf.status != "removed"]
        manifest_file = ExportedFile("manifest", destination_path / manifest_pf.filename,
                                      manifest_pf.size, manifest_pf.sha256)
        results = tuple(ExportedFile(pf.role, destination_path / pf.filename, pf.size, pf.sha256)
                        for pf in other_pfs)
        return ThemePackageResult(destination_path / manifest_pf.filename, (manifest_file,) + results)


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
