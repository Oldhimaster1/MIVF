# MOFLEX_FOR_MIVF Review (2026-07-05)

## Scope
This document records the review of the extracted incoming bundle at:
- `moflex_for_mivf/moflex_for_mivf/`

The goal was to determine whether the incoming code should be integrated into MIVF safely.

## Executive Verdict
Do not integrate the incoming bundle wholesale.

Review-only outcome was selected because the package is a broad replacement/alternate stack, not a small patch.

## Incoming Bundle Summary
- Total files: 729
- Approximate composition:
  - 715 headers (`.h`)
  - 12 C source files (`.c`)
  - 1 assembly file (`.s`)
  - 1 markdown file (`INTEGRATION.md`)
- Counterpart overlap with `source/moflex`: 725 files matched existing paths.
- New files not present in current tree:
  - `decoder/cia_moflex.c`
  - `decoder/cia_moflex.h`
  - `decoder/mc_asm.s`

## Why Full Integration Is Unsafe
Hard-stop conditions were met.

1. The incoming package replaces multiple compiled MoFlex files.
2. Incoming playback code modifies NDSP/audio control behavior.
3. Incoming playback code adds threading and CPU-time tuning behavior.
4. Incoming integration notes require build-system/library changes (`citro2d`/`citro3d`).
5. The diff surface is large (core playback and demux deltas), increasing regression risk.

## Highest-Risk Areas Observed
- Playback path rewrite in incoming `moflex/playback/moflex_playback.c`.
- NDSP/audio ownership changes and worker-thread logic.
- Additional runtime controls (`threadCreate`, `APT_SetAppCpuTimeLimit`).
- New helper path for CIA-embedded MoFlex (`cia_moflex.*`) with container parsing.
- Build implication notes from incoming `INTEGRATION.md`.

## Potentially Useful Ideas (Not Yet Integrated)
- CIA embedded MoFlex helper parsing (`cia_moflex.c/h`).
- Demux windowing and duration-sanity patterns in incoming demux delta.

These should only be considered as isolated, staged work.

## Safe Staged Plan
### Stage 0 (completed)
- Review-only report.
- No source integration.

### Stage 1 (recommended next)
- CIA helper audit only (`cia_moflex.c/h`) as isolated code review.
- No playback wiring.
- No dispatch changes in `source/main.c`.
- No NDSP/audio path changes.
- No default behavior change.

### Stage 2 (optional)
- If Stage 1 passes review, add helper code as non-called, non-default implementation.
- Keep integration guarded and reversible.

### Stage 3+
- Consider one demux bugfix at a time.
- Defer playback/audio path changes until isolated tests and hardware validation are available.

## Guardrails for Future Work
- Never import the incoming folder as a bulk copy.
- Keep patches tiny and single-purpose.
- Avoid touching MIVF core playback, NDSP startup, stream/io ring, seek-index, and encoder paths.
- Build after each patch and stop after one feature slice.

## Workspace Hygiene Notes
To reduce accidental commits, local excludes were added to `.git/info/exclude` for:
- `moflex_for_mivf/`
- `_backup_before_moflex_review_*.patch`

This is local-only and does not modify tracked repo files.

## Commit Recommendation
No integration commit should be made from this review alone.

If desired, commit only this document in a dedicated docs commit after review:
- `docs/MOFLEX_FOR_MIVF_REVIEW.md`
