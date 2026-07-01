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
- `--keep` — transform coefficients to keep per 4×4 quadrant (choices: 4, 8, 16)
- `--jobs` — number of parallel encoder workers
- `--chunk-frames` — frames per streaming-chunk (default 240)
- `--no-deploy` — skip automatic deployment to SD card
- `--m2y2` — perform range-coding pass to produce M2Y2 video (lossless with smaller file)

Notes

All options mirror the encoder front-end arguments. For advanced tuning, combine `--qp`, `--lambda`, `--keep` and `--mv-range` to balance quality vs size vs encode time.
