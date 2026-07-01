# Encoding (Recommended commands)

The encoder provides a stable streaming chunk pipeline that balances speed, memory usage, and the ability to recover progress.

Recommended commands

Fast / smaller:

```bat
encode_mivf.exe input.mkv output_fast.mivf --m2y2 --no-deploy --fps 24 --audio-rate 48000 --jobs 6 --chunk-frames 240 --keep 4 --mv-range 1 --qp 38 --lambda 34
```

Balanced:

```bat
encode_mivf.exe input.mkv output_balanced.mivf --m2y2 --no-deploy --fps 24 --audio-rate 48000 --jobs 6 --chunk-frames 240 --keep 8 --mv-range 2 --qp 36 --lambda 26
```

Higher quality:

```bat
encode_mivf.exe input.mkv output_hq.mivf --m2y2 --no-deploy --fps 24 --audio-rate 48000 --jobs 6 --chunk-frames 240 --keep 16 --mv-range 4 --qp 34 --lambda 20
```

Notes

- `--m2y2` performs a lossless final range-coding pass (M2Y2) of the already-encoded video packets; it reduces size without affecting decoded quality but adds extra processing time.
- `--fps 24 --audio-rate 48000` is recommended for 23.98/24 fps sources because it yields exactly 2000 audio samples per frame (48000/24 = 2000), aligning audio frames and reducing drift.
- Bare defaults favor higher quality and may not be optimal for 24 fps sources unless you set the recommended fps/audio rate.

Quality knobs

- `--keep` — transform coefficients kept per 4×4 quadrant (4/8/16). Larger keeps more detail at the cost of size.
- `--qp` — quantizer parameter; lower = higher quality and larger size. Typical values are in the mid 30s for 3DS targets.
- `--lambda` — RDO lambda controlling rate/distortion tradeoff; higher values favor smaller output.
- `--mv-range` — motion-vector search range (smaller values reduce CPU/time but can harm compression efficiency for complex motion).
- `--jobs` — number of parallel encoder workers.
- `--chunk-frames` — frames per streaming worker chunk (affects memory per worker and responsiveness of progress reporting).
