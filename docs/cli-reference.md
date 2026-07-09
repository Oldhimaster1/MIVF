# CLI Reference — encode_mivf.py

Usage: `encode_mivf.py input output [options]`

Options

- `input` — input video file or a directory of videos (batch mode)
- `output` — output `.mivf` file or output directory for batch mode
- `--width` — output width (default 400)
- `--height` — output height (default 240)
- `--fps` — output frame rate (default 30)
- `--audio-rate` — audio sample rate (default 44100)
- `--audio-channels` — audio channels (default 1)
  - Note: IA4M mux currently supports mono only; setting stereo is not supported for muxing into IA4M audio in the current implementation.
- `--keyint` — keyframe interval (default 240)
- `--qp` — quantizer parameter (default 34)
- `--c-qp-offset` — chroma QP offset
- `--lambda` — rate-distortion lambda
- `--y-skip`, `--c-skip` — luma/chroma skip thresholds
- `--y-delta`, `--c-delta` — luma/chroma delta controls
- `--mv-range` — motion vector search range
- `--motion-search {full,diamond,fast,hybrid}` — per-block motion search algorithm (default `full`, exhaustive)
  - `full` — exhaustive search over the full `--mv-range` window; slowest, best quality/size, unchanged/stable behavior.
  - `diamond` — experimental iterative cross-search; faster, small quality/size cost.
  - `fast` — experimental, more speed-biased than `diamond`; larger quality/size cost.
  - `hybrid` — experimental; runs the diamond search from both the zero vector and the frame's global motion estimate, then applies a small bounded local refine to blocks that still look unreliable (capped at 25% of a plane's blocks per frame). Aims for close to `diamond`'s speed with less of its quality/size cost. See [Performance Tuning](PERFORMANCE_TUNING.md#motion-search-modes) for validation notes.
- `--keep` — transform coefficients to keep per 4×4 quadrant (choices: 4, 8, 16)
- `--jobs` — number of parallel encoder workers
- `--chunk-frames` — frames per streaming-chunk (default 240)
- `--warm-start-chunks` — feed each parallel chunk after the first the prior chunk's actual reconstructed last frame as a throwaway warm-up, so the chunk's real first frame can use ordinary motion-compensated coding instead of a forced keyframe reset at every chunk boundary. Serializes chunk encoding (no cross-chunk parallelism) since each chunk needs its predecessor's finished reconstruction first.
- `--max-video-packet-kb` — if set, any chunk whose largest video packet exceeds this size (KB) is retried at a higher QP (bounded retries), keeping whichever attempt produced the smallest max packet. Retry granularity is a whole `--chunk-frames` chunk, not a single frame.
- `--report-packet-sizes` — print a per-video-packet size histogram and percentile breakdown after encoding
- `--no-deploy` — skip automatic deployment to SD card
- `--m2y2` — perform range-coding pass to produce M2Y2 video (lossless with smaller file)

Notes

All options mirror the encoder front-end arguments. For advanced tuning, combine `--qp`, `--lambda`, `--keep` and `--mv-range` to balance quality vs size vs encode time.

The encoder also prints an `ENCODE SUMMARY` at the end of each run (settings, frame/byte counts, per-mode block-type histogram, and approximate PSNR), aggregated from the native helper's own diagnostics — useful for comparing `--motion-search` modes or tuning settings run to run.
