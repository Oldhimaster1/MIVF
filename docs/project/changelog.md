# Changelog

## Unreleased

- **Player**: Dashboard Canvas position override for the Premiere transport style
  (control positions only — the other 15 styles are unchanged); Advanced Screensaver
  Customization (speed/idle-delay/fade/reduce-motion, in-Settings rows, per-title custom
  bounce image); Touch Lock (hold L+R ~1.5s to lock/unlock touch input, with an
  on-screen indicator and help-screen entry). Build-verified; not physical-hardware-
  verified.
- **Player**: a genuine watched/unwatched/in-progress model, independent of the resume
  bookmark (a bookmark is cleared on natural end-of-file, so it can't represent "watched"
  by itself) — automatic tracking during playback, a manual long-press-Y correction, and
  a Continue Watching + Recently Added promoted section at the top of the Library list.
  Library Sort/Filter/Search (title/date-added/last-played/watched-state, plus a
  filename-only Series/Season/Episode grouping mode and a real on-screen-keyboard search)
  — see [Known Issues](../compatibility/known-issues.md) for the exact scope limits.
  Build-verified and host-test-verified (new pure-logic test files under `tools/`); not
  physical-hardware-verified.
- **Toolkit** (`mivf-gui/`): Project Home with a guided new-project wizard and a recent-
  projects list; a unified Create-MIVF preflight (disk-space and destination-writability
  checks, plus a fix for a real bug where editing a field after "Validate" without
  re-clicking it could launch a stale command); a persistent, resumable multi-project
  encode queue; a Make My Theme wizard; a Chapter Authoring Studio that writes the
  player's real `.chapters` sidecar; a reusable Preview Tour; and a Local Theme Browser
  that imports a previously-exported theme package's colors and dashboard layout (not its
  artwork — see the browser's own in-app explanation for why). 244 automated tests
  (pytest) plus an offscreen smoke suite, all passing; build-verified; not
  physical-hardware-verified (desktop-only feature set, N/A for hardware testing).

- Added a dual-screen Library browser (favorites, recents, numeric-aware natural sort,
  debounced previews with a `.cover`/`.preview.cover` fallback chain) and a DVD-style
  per-movie menu (`.menu.ini`, animated Ken Burns background, Scene Selection thumbnail
  grid, idle screensaver, cached full-frame rendering). Build-verified overall; the
  Library browser, DVD-style root menu, and Scene Selection are additionally
  emulator-tested in Azahar. Physical-hardware validation remains pending for all of
  it — see [Validation Method](../compatibility/validation-method.md) for the per-feature breakdown
  (not every part of this listed above is emulator-confirmed yet).
- Fixed a DVD-authenticity gap: playback launched from a movie's DVD menu now returns to
  that same menu when it ends or is exited, instead of falling back to the flat file
  browser.
- Added a persisted volume setting (0–300%, saved across sessions).
- Fixed multi-line SRT subtitle cues to render as two real centered lines instead of
  being joined with a `" | "` separator.
- Added exact rational frame rate support (e.g. `--fps 24000/1001`), parsed with exact
  fraction math, plus the audio-rate compatibility checks that come with it (see
  [Encoding Overview](../authoring/overview.md#frame-rate)).
- Added an experimental `--motion-search hierarchical` mode: deterministic
  coarse-to-fine grid search; in one measured 1-minute clip it matched
  `full --mv-range 12`'s size/PSNR within half a percent in under half the encode time
  (see [Performance Tuning](../technical/performance-tuning.md#motion-search-modes)).
- Added resumable encoder jobs (`--job-dir`, `--resume-job`, `--keep-intermediates`,
  `--finalize-from-video`) and per-stage timing reports (`--timing-json`) — see
  [Encoder Recovery & Profiling](../technical/encoder-recovery-and-profiling.md).
- Hardened M2Y1/M2Y2 player decode against malformed/corrupted plane payload sizes (32-bit-overflow-safe validation in `source/main.c`); valid file decode behavior is unchanged.
- Added experimental `--motion-search hybrid` encoder mode: seeds the diamond search from both the zero vector and the frame's global motion estimate, with a capped local refine for blocks that still look unreliable. `full`/`diamond`/`fast` are unaffected.
- Added an aggregated `ENCODE SUMMARY` at the end of each encode run (settings, byte/frame counts, per-mode block histogram, approximate PSNR), sourced from the native helper's existing per-segment diagnostics.
- Added an in-app controls/keybinds help screen (press X in the browser, or select CONTROLS in Settings during playback).
- Overhauled the README with badges, screenshots, benchmark charts, recommended settings, and MoFlex-vs-MIVF comparison notes.
- Documented encoder motion-search modes, packet-spike diagnostics, warm-start chunks, and release packaging requirements.
- Documented the stable streaming chunk encoder pipeline.
- Added full documentation site (docs/).

## Observed

- Streaming chunk encoder uses FFmpeg -> ThreadPool -> native helper -> segment merge -> IA4M mux -> optional M2Y2.