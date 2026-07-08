# Changelog

## Unreleased

- Hardened M2Y1/M2Y2 player decode against malformed/corrupted plane payload sizes (32-bit-overflow-safe validation in `source/main.c`); valid file decode behavior is unchanged.
- Added experimental `--motion-search hybrid` encoder mode: seeds the diamond search from both the zero vector and the frame's global motion estimate, with a capped local refine for blocks that still look unreliable. `full`/`diamond`/`fast` are unaffected.
- Added an aggregated `ENCODE SUMMARY` at the end of each encode run (settings, byte/frame counts, per-mode block histogram, approximate PSNR), sourced from the native helper's existing per-segment diagnostics.
- Documented the stable streaming chunk encoder pipeline.
- Added full documentation site (docs/).

## Observed

- Streaming chunk encoder uses FFmpeg -> ThreadPool -> native helper -> segment merge -> IA4M mux -> optional M2Y2.

