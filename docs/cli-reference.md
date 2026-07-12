# CLI Reference — encode_mivf.py

Usage: `encode_mivf.py input output [options]`

This page is regenerated against the current `python encode_mivf.py --help` output —
re-run that command yourself if anything here looks out of date.

## Positional arguments

- `input` — input video file or a directory of videos (batch mode)
- `output` — output `.mivf` file or output directory for batch mode

## Video

- `--width` — output width (default 400)
- `--height` — output height (default 240)
- `--fps` — output frame rate: integer or exact rational, e.g. `24` or `24000/1001`
  (default 30). Parsed with Python's exact `Fraction` type — never a float
  approximation — and validated against the container's rate fields before encoding
  starts. See [encoding.md](encoding.md#frame-rate) for which audio rates are compatible
  with a given rational rate.
- `--m2y2` — range-code video to the smaller M2Y2 codec (lossless, ~20–30% smaller)
- `--keyint` — keyframe interval (default 240)

## Quality and Motion Search

- `--qp` — quantizer parameter (default 34)
- `--c-qp-offset` — chroma QP offset (default 5)
- `--lambda` — rate-distortion lambda (default 20.0)
- `--y-skip`, `--c-skip` — luma/chroma skip thresholds
- `--y-delta`, `--c-delta` — luma/chroma delta controls
- `--mv-range` — motion vector search range (default 4)
- `--motion-search {full,diamond,fast,hybrid,hierarchical}` — per-block motion search
  algorithm (default `full`)
  - `full` — exhaustive search over the full `--mv-range` window; slowest, best
    quality/size, the only mode with a long production track record.
  - `diamond` — experimental iterative cross-search; faster, small quality/size cost.
  - `fast` — experimental, more speed-biased than `diamond`; larger quality/size cost.
  - `hybrid` — experimental; runs the diamond search from both the zero vector and the
    frame's global motion estimate, then a capped local refine on blocks that still look
    unreliable (capped at 25% of a plane's blocks per frame).
  - `hierarchical` — experimental; deterministic coarse-to-fine grid search, aiming to
    approach `full`-search quality much faster. In one measured 1-minute clip it matched
    `full --mv-range 12`'s size/PSNR within half a percent in under half the time — see
    [PERFORMANCE_TUNING.md](PERFORMANCE_TUNING.md#motion-search-modes).
  - See [validation-status.md](validation-status.md) for exactly what's been verified
    about each mode.
- `--keep {4,8,16}` — transform coefficients kept per 4×4 quadrant (default 16; 16 = max
  detail, 4 = smallest legacy files)
- `--warm-start-chunks` — feed each parallel chunk after the first the prior chunk's
  actual reconstructed last frame as a throwaway warm-up, so the chunk's real first frame
  can use ordinary motion-compensated coding instead of a forced keyframe reset at every
  chunk boundary. **Serializes chunk encoding** (no cross-chunk parallelism), since each
  chunk needs its predecessor's finished reconstruction first — measured 3–4× slower in
  local testing for a modest size reduction; see
  [PERFORMANCE_TUNING.md](PERFORMANCE_TUNING.md#motion-search-modes).
- `--max-video-packet-kb` — if set, any chunk whose largest video packet exceeds this
  size (KB) is retried at a higher QP (bounded retries), keeping whichever attempt
  produced the smallest max packet. Retry granularity is a whole `--chunk-frames` chunk,
  not a single frame.
- `--profile {default,3ds-fast}` — encoder quality/speed preset: `default` for quality,
  `3ds-fast` for smaller packets and smoother Old-3DS playback

## Audio

- `--audio-rate` — audio sample rate (default 44100)
- `--audio-channels` — audio channels (default 1)
- `--audio-codec {ia4m,pc16}` — audio mux format (default `ia4m`): `ia4m` is small ADPCM
  mono, `pc16` is larger high-quality PCM. IA4M requires `--audio-channels 1`.
- `--audio-offset-ms` — shift audio relative to video by this many ms; positive delays
  audio (prepends silence), negative advances it (trims samples from the start)

## Parallelism

- `--jobs` — parallel encoder workers (default: `min(8, cpu_count)`)
- `--chunk-frames` — frames per streaming worker chunk (default 240)

## Seek index

- `--no-seek-index` — do not generate the `.idx` sidecar seek index
- `--no-embedded-index` — do not append the embedded seek index footer to the `.mivf`
- `--seek-index-sidecar` — optional explicit path for the generated `.idx` sidecar

## Resumable jobs (E0) & stage timing (E1)

See [encoder-recovery-and-profiling.md](encoder-recovery-and-profiling.md) for the full
workflow and the exact resume-validation mechanism.

- `--job-dir JOB_DIR` — persistent E0 job directory
- `--resume-job` — reuse a validated video-only intermediate in `--job-dir`
- `--keep-intermediates` — preserve E0 job/intermediate files
- `--finalize-from-video FINALIZE_FROM_VIDEO` — skip the video encode and finalize this
  video-only `.mivf`
- `--timing-json TIMING_JSON` — E1 JSON timing report path (a CSV is written alongside)

## Misc

- `--report-packet-sizes` — print a per-video-packet size histogram and percentile
  breakdown after encoding
- `--no-deploy` — skip automatic deployment to SD card

## Notes

For advanced tuning, combine `--qp`, `--lambda`, `--keep`, and `--mv-range` to balance
quality vs. size vs. encode time.

The encoder also prints an `ENCODE SUMMARY` at the end of each run (settings, frame/byte
counts, per-mode block-type histogram, and approximate PSNR), aggregated from the native
helper's own diagnostics — useful for comparing `--motion-search` modes or tuning
settings run to run. When requesting `--timing-json`, parse the JSON/CSV's `stages` list
for authoritative timing figures rather than an early FFmpeg progress line — see
[encoder-recovery-and-profiling.md](encoder-recovery-and-profiling.md) for a concrete
case where that distinction mattered.
