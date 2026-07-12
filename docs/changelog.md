# Changelog

## Unreleased

- Added a dual-screen Library browser (favorites, recents, numeric-aware natural sort,
  debounced previews with a `.cover`/`.preview.cover` fallback chain) and a DVD-style
  per-movie menu (`.menu.ini`, animated Ken Burns background, Scene Selection thumbnail
  grid, idle screensaver, cached full-frame rendering). Build-verified overall; the
  Library browser, DVD-style root menu, and Scene Selection are additionally
  emulator-tested in Azahar. Physical-hardware validation remains pending for all of
  it — see [validation-status.md](validation-status.md) for the per-feature breakdown
  (not every part of this listed above is emulator-confirmed yet).
- Fixed a DVD-authenticity gap: playback launched from a movie's DVD menu now returns to
  that same menu when it ends or is exited, instead of falling back to the flat file
  browser.
- Added a persisted volume setting (0–300%, saved across sessions).
- Fixed multi-line SRT subtitle cues to render as two real centered lines instead of
  being joined with a `" | "` separator.
- Added exact rational frame rate support (e.g. `--fps 24000/1001`), parsed with exact
  fraction math, plus the audio-rate compatibility checks that come with it (see
  [encoding.md](encoding.md#frame-rate)).
- Added an experimental `--motion-search hierarchical` mode: deterministic
  coarse-to-fine grid search; in one measured 1-minute clip it matched
  `full --mv-range 12`'s size/PSNR within half a percent in under half the encode time
  (see [PERFORMANCE_TUNING.md](PERFORMANCE_TUNING.md#motion-search-modes)).
- Added resumable encoder jobs (`--job-dir`, `--resume-job`, `--keep-intermediates`,
  `--finalize-from-video`) and per-stage timing reports (`--timing-json`) — see
  [encoder-recovery-and-profiling.md](encoder-recovery-and-profiling.md).
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