# Troubleshooting

Common issues and how to resolve them.

FFmpeg not found

- Error: "FFmpeg was not found". Solution: install ffmpeg and ensure it is on PATH, or place a `ffmpeg`/`ffmpeg.exe` next to `encode_mivf.exe`.

m2y2_transcode.exe not found

- Error: when `--m2y2` is requested but `m2y2_transcode` cannot be found. Solution: build `tools/m2y2_transcode.c` or download the prebuilt helper and place it in `tools/` or next to the frontend executable.

miv2y_moflex_tier.exe not found

- The native encoder helper is required for chunk encoding. Build it with gcc or use a prebuilt binary.

Output frame count differs by a few frames

- Small off-by-one frame differences may occur due to source timestamps or ffmpeg vs internal frame counting. Inspect the input with `ffprobe` and prefer explicit `--fps` to force a frame rate.

High RAM usage

- Lower `--chunk-frames`, reduce `--jobs`, or run the parallel slice engine only on machines with large RAM.

Slow M2Y2 final pass

- `--m2y2` performs an extra range-coding pass; it is CPU-intensive but lossless. If encode time is critical, omit `--m2y2`.

Audio mux mono-only

- The IA4M mux currently supports mono only. Convert audio to mono before running the mux (ffmpeg `-ac 1`) or set `--audio-channels 1`.

SD card deployment problems

- The encoder attempts to deploy to `/d` by default (Windows SD-mount convention). If your SD card is on another drive, copy the `.mivf` manually to `sdmc:/mivf/`.
