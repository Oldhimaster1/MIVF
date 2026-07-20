# Encoder Internals (overview)

This describes the pipeline `encode_mivf.py` actually runs today. An older, standalone
`tools/encode_mivf.sh` + `tools/mivf_parallel_engine.py` pair also exists in the repo —
it predates the current front-end (hardcoded 30 fps, 44100 Hz audio, a hardcoded SD card
drive letter, and a reference to a long-superseded player build name). It is **legacy,
not part of the current supported pipeline** — don't reach for it; everything below
describes the real, current path.

## Streaming chunk pipeline (current)

1. FFmpeg is launched in rawvideo pipe mode to produce YUV420 frames at the target
   width/height/frame rate.
2. The Python front-end reads chunks of raw frames (`--chunk-frames`) from the ffmpeg
   pipe.
3. Each chunk is submitted to a `ThreadPoolExecutor` task that calls the native helper
   (`miv2y_moflex_tier`), which compresses the chunk into a temporary MIVF segment using
   the selected `--motion-search` algorithm (`full`, `diamond`, `fast`, `hybrid`, or
   `hierarchical` — see [PERFORMANCE_TUNING.md](performance-tuning.md#motion-search-modes)).
4. Completed segments are merged in sequence into a single video-only `.mivf` container.
5. The front-end muxes audio (IA4M or PC16) into the merged video stream, at whatever
   exact rational or integer frame rate was requested.
6. Optionally, a final `--m2y2` pass runs `m2y2_transcode` to range-code the video
   payloads for a smaller, still-lossless file — the transcoder verifies its own output
   against the input before finishing.

### Why this pipeline

- Streaming avoids keeping the entire raw YUV in memory for long inputs.
- Chunking enables multiple parallel encoder instances while preserving deterministic
  ordering of frames.
- Segment merging keeps the container layout simple and allows per-chunk progress
  reporting and recovery — this is also the basis for the E0 resumable-job mechanism
  (see [encoder-recovery-and-profiling.md](encoder-recovery-and-profiling.md)).

### Warm-start chunks

`--warm-start-chunks` feeds each chunk (after the first) the previous chunk's actual
reconstructed last frame as a throwaway warm-up, avoiding a hard keyframe reset at every
chunk boundary. This **serializes** chunk encoding — each chunk now depends on its
predecessor finishing first — which measured 3–4× slower in local testing for a modest
size reduction. It is not a free win; see
[PERFORMANCE_TUNING.md](performance-tuning.md#motion-search-modes) for the measured
numbers.

## Audio mux

Audio is encoded into IA4M (4-bit IMA-style ADPCM, mono only) or PC16 (16-bit PCM,
stereo-capable) containers, at exactly the number of samples per frame implied by the
requested frame rate and audio rate — see [Encoding Overview](../authoring/overview.md#frame-rate) for which
combinations divide evenly.

## Native helpers

- `miv2y_moflex_tier` — native encoder that consumes raw YUV and produces MIVF-encoded
  pages using the M2Y1 block-mode codec (SKIP/RAW/DELTA/SOLID/RUN_SKIP/QRES/MVCOPY and
  related motion-compensated modes, chosen per-block via RDO).
- `m2y2_transcode` — lossless range-coder pass that verifies its output against the
  input and produces smaller files (M2Y2).

**A real build gotcha:** `encode_mivf.py` loads this native helper from the repository
root, not from `tools/` — see
[encoder-recovery-and-profiling.md](encoder-recovery-and-profiling.md#native-helper-binary-gotcha)
before assuming a rebuilt helper is actually being used.

## Job recovery & profiling (E0 / E1 / E2)

Resumable job directories, per-stage timing reports, and the jobs/chunk-frames benchmark
workflow are covered in full in
[encoder-recovery-and-profiling.md](encoder-recovery-and-profiling.md) — including the
exact resume-fingerprint mechanism and a real, corrected case where a preserved benchmark
summary's FPS figure was wrong by roughly 30×.

## Performance & tuning

- Increase `--jobs` to use more CPU cores; monitor memory usage, since each worker holds
  its raw chunk in memory.
- Reduce `--chunk-frames` when memory is constrained (also affects throughput — see the
  E2 jobs/chunk-frames results in
  [PERFORMANCE_TUNING.md](performance-tuning.md#e2-benchmarking-jobs-and-chunk-frames)).
- Use `--keep`, `--qp`, `--mv-range`, and `--motion-search` to trade quality vs. size vs.
  encode time.
