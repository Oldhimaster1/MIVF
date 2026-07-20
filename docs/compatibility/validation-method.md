# Validation Status

MIVF's source tree currently contains committed, hardware-tested foundations alongside a
substantial amount of **uncommitted** work-in-progress (`git status` shows `main.c`,
`mivf_settings.c/.h`, `mivf_subtitles.c`, `encode_mivf.py`, and `tools/miv2y_moflex_tier.c`
all modified relative to `HEAD`). This page exists so that every feature below can be
checked against one consistent bar, instead of the README implying a single blanket
"working" status for the whole project.

## Status vocabulary

| Status | Meaning |
| :--- | :--- |
| **Build-verified** | Compiles and links successfully. Confirmed by a real build (object files / `.elf` / `.3dsx` newer than the source edit). Says nothing about runtime correctness. |
| **Emulator-tested** | Observed running correctly in an emulator (e.g. Azahar). |
| **Preliminary physical-hardware run reported** | A physical console run was reported successful by the tester, without yet a recorded exact console model, artifact hash, stage-by-stage results, or objective performance measurements. Not a completed compatibility certification — see [Project Status](../status.md). |
| **Documented physical-hardware validation** | A physical console run with an identified console model, exact tested artifact (and its SHA-256), stage-by-stage results, and known limitations, all on record. |
| **Hardware-verified** | Confirmed working on a physical New/Old 3DS, predating and unaffected by the current documented-validation process above. |
| **Hardware-unverified** | Build-verified and/or emulator-tested, but not yet confirmed on physical hardware. |
| **Experimental** | Intentionally exposed as an option; behavior, defaults, or output may still change. |
| **Planned** | Not implemented. Described only for roadmap purposes — never listed as a current feature. |

The 3DS-specific tiers (Emulator-tested / Hardware-verified / Hardware-unverified) apply to
**player (3DS runtime)** features. The **encoder** is ordinary PC-side Python/C tooling — it
never runs on 3DS hardware — so encoder features are rated on whether they were actually
exercised and what that exercise showed (determinism checks, structural audits, measured
output), not on 3DS hardware access.

## Player (3DS runtime)

| Feature | Status | Notes |
| :--- | :--- | :--- |
| Core playback: NDSP audio clock, rational-FPS timing, video decode, seeking, frame scheduling | **Hardware-verified** (as of the last committed release) | Frozen foundation. Nothing in the current working-tree changes touches this code path. |
| MoFlex (`.moflex`) playback | **Hardware-verified** (as of the last committed release) | See [MoFlex Status](../technical/moflex.md). Unaffected by the current uncommitted diff. |
| Dual-screen Library browser (list + top-screen preview, 7 visible rows) | **Build-verified, Emulator-tested (Azahar)** | Confirmed running: metadata line (`M2Y2 400X240 FPS 24000/1001`, `IA4M 48000 HZ 1 CH`), duration, item count, and the seven-row list all render correctly in a real capture. Physical-hardware validation still pending. |
| `.cover` (88×50) / `.preview.cover` (176×100) thumbnail loading + LRU cache | **Build-verified, partially emulator-observed** | The no-artwork fallback path (generic placeholder card) was confirmed running correctly in a real capture; actual `.cover`/`.preview.cover` sidecar loading was not exercised in that same capture (no sidecar was present for the test file), so only the fallback branch is emulator-confirmed so far. |
| Favorites / Recents (natural, numeric-aware sort) | **Build-verified** | Natural sort itself predates this work (Phase 6) and has a longer track record; the Favorites/Recents list logic on top of it is newer and share the same unverified-at-runtime caveat. |
| Resume/bookmark modal + pixel progress bar | **Build-verified** | B and START both mean "start over" in this modal — see [Controls](../player/controls.md). |
| MIVF-vs-MoFlex badge, FAVORITE/RESUME/RECENT badges | **Build-verified, partial** | The MIVF/MoFlex distinction is a plain text label only (no icon/color system), and is overridden by the other three badges when present. |
| DVD-style menu (root menu, action dispatch) | **Build-verified, Emulator-tested (Azahar)** | Confirmed running: title card and custom background render, Play Movie/Resume (correctly shown disabled with no bookmark)/Scene Selection/Back all display and are selectable, bottom-screen dashboard shows chapter count and format. Only `play`/`resume`/`chapters`/`back` actions exist; anything else is silently disabled. Physical-hardware validation still pending. |
| Scene Selection (chapter thumbnail grid) | **Build-verified, Emulator-tested (Azahar)** | Confirmed 2 columns × 3 visible rows = 6 thumbnails/page — verified both from source and from a real Azahar capture (20 chapters shows "PAGE 1 OF 4," which only checks out at 6/page). |
| Return-to-menu after menu-launched playback | **Build-verified** | Newly fixed this cycle (previously fell back to the flat file browser instead of the originating DVD menu). Uncommitted. |
| Menu fades, session-scoped cursor memory | **Build-verified** | Cursor/chapter-page memory is session-only (a `static` in-RAM variable), never written to disk. |
| Cached full-frame menu rendering (dirty-rect style) | **Build-verified** | Real mechanism: two full RGB565 framebuffer caches with dirty invalidation, not just "should be fast." |
| Ken Burns background animation | **Build-verified; background render emulator-observed, animation itself unconfirmed** | The background image itself was confirmed rendering correctly in a real capture. A still image can't confirm the pan/zoom motion is correct — that part remains build-verified only. Fixed-point/integer only, no floats. Current pass lengthened the pan/zoom cycles, reduced zoom magnitude, and added smoothstep easing versus the prior pass. |
| Idle screensaver + `.screensaver.cover` | **Build-verified, partially emulator-tested (Azahar)** | Custom `.screensaver.cover` loading and screensaver rendering were confirmed in Azahar via a one-off showcase-capture build. Normal idle-timeout activation, continuous bounce behavior, collision handling, and dismissal-by-every-input remain unverified this cycle — a still capture from a forced showcase path doesn't exercise those. Root-menu view only; sidecar-only image, no MASB embedding. |
| Persisted volume level (0–300%) | **Build-verified** | Small, additive settings change; unrelated to the Library/menu work above. |
| Subtitle multi-line rendering fix | **Build-verified** | Two-line SRT cues now render as two real lines instead of being joined with `" | "`. |
| Settings / Help overlays, direct-play-vs-DVD-menu dispatch | **Build-verified** | No regressions found; behavior matches the previously released design. |

Rows marked **Build-verified** only (no "Emulator-tested" annotation) have not been
confirmed running anywhere since the changes were made — treat them as
implemented-and-compiling, not yet feature-tested. Rows marked
**Emulator-tested (Azahar)** have been confirmed running correctly in the Azahar
emulator via real captures, but **none of the current Library/DVD-menu work has been
confirmed on physical 3DS hardware yet** — don't conflate emulator confirmation with
hardware confirmation for any row above.

## Encoder (PC-side tooling)

| Feature | Status | Notes |
| :--- | :--- | :--- |
| M2Y1 / M2Y2 codecs | **Verified** | M2Y2 is a self-verifying lossless re-encode of M2Y1 (`m2y2_transcode` checks its own output). |
| Exact rational frame rate (`--fps 24000/1001` etc.) | **Verified** | Covered by unit tests (`test_parse_rational_frame_rate`, `test_rational_ticks_and_audio_packet_size`, `test_patch_video_timing_metadata`) and by real encodes in the benchmark matrices below. |
| Motion search: `full` | **Verified**, production default | Long track record; exhaustive search. |
| Motion search: `diamond`, `fast`, `hybrid` | **Experimental** | Byte-identical-to-baseline determinism confirmed; quality/size tradeoffs measured on a small local clip set — see [Performance Tuning](../technical/performance-tuning.md). |
| Motion search: `hierarchical` | **Experimental** | Determinism-checked (repeat runs hash-identical) and structurally audited (including M2Y2 losslessness) via `run_hierarchical_motion_v2_matrix_1min.sh`; in the 1-minute Les Misérables clip matrix it matched `full`-range-12's size/quality in under half the time. One clip, one machine — not a universal guarantee. |
| `--warm-start-chunks` | **Experimental** | Confirmed to shrink output slightly by avoiding a hard keyframe reset at chunk boundaries, but serializes chunk encoding — measured 3-4x wall-time increase on the same clip. Real, measured tradeoff, not a free win. |
| E0 resumable jobs (`--job-dir`, `--resume-job`, `--keep-intermediates`, `--finalize-from-video`) | **Implemented, code-verified** | Fingerprint/manifest mechanism (input hash+size+mtime plus every quality-affecting setting) confirmed by reading the source. Not exercised end-to-end this session (i.e. no encode was deliberately killed and resumed to confirm the refusal/resume path live). |
| E1 stage timing (`--timing-json`) | **Verified** | Exercised for real during the E2 five-minute benchmark; JSON/CSV/log output all cross-checked against each other. |
| E2 job/chunk-frames benchmarking | **Verified, single machine/sample** | Ran for real (12 combinations, jobs {8,12,16} × chunk-frames {120,240,480,960}). The preserved `e2_5min_summary.md`'s own "Video FPS" column was found to be wrong during this documentation pass (see [Encoder Recovery & Profiling](../technical/encoder-recovery-and-profiling.md)) — use the corrected figures, not that file's table. |
| Menu/asset authoring tools (`mivf_build_chapter_thumbs.py`, `mivf_embed_assets.py`, `mivf_make_screensaver_cover.py`, `mivf_list_assets.py`) | **Verified** | `--help` output confirmed current; screensaver-cover tool's output format matches the player's loader exactly (96×54 RGB565, 10,368 bytes). |
| DVD bonus-feature ripping pipeline (`tools/mivf_dvd_pipeline.py`, `tools/mivf_dvd_probe.py`) | **Experimental, personal workflow tool** | A separate, unrelated feature from the DVD-style *menu UI* above — this is a read-only DVD/ISO inventory and bonus-feature compile planner for the maintainer's own disc backups. Not part of the player and not documented as a general-purpose feature. |

## What this means for the rest of the docs

Anywhere else in this documentation set that a Phase 8/8.1 Library or DVD-menu feature is
described, assume **Build-verified, Hardware-unverified** unless stated otherwise — and
check the table above, since the Library browser, the DVD-style root menu, and Scene
Selection have since been confirmed **Emulator-tested (Azahar)** via real captures. The
project's last tagged release (predating this work) remains the most recent
**Hardware-verified** state; that hardware validation covered the frozen playback
foundation (generation-safe NDSP audio clock, rational-FPS timing, decode, seek) and
MoFlex — it does **not** extend to any of the current Library/DVD-menu UI work, emulator
confirmation notwithstanding.

An initial physical-hardware run of the Showcase (Revision 3) was reported successful,
with strong observed performance on the tested console. This is a **preliminary
physical-hardware run reported**, not a **documented physical-hardware validation** —
see [Project Status](../status.md) for exactly what is and isn't recorded yet, and
[Hardware Matrix](hardware-matrix.md) for the (currently placeholder) model-specific
compatibility table. Do not read the preliminary report above as covering every row in
this page's Player table individually; it was one run of the Showcase sequence, not a
feature-by-feature hardware regression pass.
