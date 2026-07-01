# Encoder internals (overview)

The encoder front-end implements two complementary engines: a streaming parallel engine optimized for low memory footprint and responsiveness, and a parallel-slice engine for high-throughput batch encoding.

Streaming chunk pipeline (stable)

1. FFmpeg is launched in a rawvideo pipe mode to produce YUV420 frames at the target width/height.
2. The Python front-end reads chunks of raw frames (`--chunk-frames`) from the ffmpeg pipe.
3. Each chunk is submitted to a ThreadPoolExecutor as a task that calls the native helper (`miv2y_moflex_tier`) which compresses the chunk into a temporary MIVF segment.
4. Completed segments are merged in sequence into a single video-only `.mivf` container.
5. The front-end muxes IA4M audio (4-bit IMA ADPCM derived format) into the merged video stream.
6. Optionally, a final `--m2y2` pass runs `m2y2_transcode` to range-code the video payloads for a smaller file.

Why this pipeline

- Streaming avoids keeping the entire raw YUV in memory for long inputs.
- Chunking enables multiple parallel encoder instances while preserving deterministic ordering of frames.
- Segment merging keeps the container layout simple and allows per-chunk progress reporting and recovery.

Audio mux

- Audio is encoded into IA4M containers using IMA-like ADPCM nibble packing. The front-end currently supports mono only when muxing into IA4M.

Native helpers

- `miv2y_moflex_tier` — native encoder that consumes raw YUV and produces MIVF-encoded pages.
- `m2y2_transcode` — lossless range-coder pass that verifies its output against the input and produces smaller files.

Performance & tuning

- Increase `--jobs` to use more CPU cores; monitor memory usage as each worker holds its raw chunk in memory.
- Reduce `--chunk-frames` when memory is constrained.
- Use `--keep` and `--qp` to trade quality vs size.
