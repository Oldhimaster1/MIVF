# Encoding Videos to MIVF

The `encode_mivf.py` script converts video files into the `.mivf` format for playback on your 3DS.

## Requirements

- Python 3
- `ffmpeg` on your system PATH

## Quick Start

```bash
# Single file, M2Y2 codec (recommended)
python encode_mivf.py input.mp4 output.mivf --m2y2

# Old 3DS optimized
python encode_mivf.py input.mp4 output.mivf --m2y2 --profile 3ds-fast

# Batch encode a folder
python encode_mivf.py ./videos/ ./output/ --m2y2
```

## Codec Choice

| Codec | Flag | Characteristics |
| :--- | :--- | :--- |
| M2Y1 | *(default)* | Raw token YUV420. Larger files, lower decode cost. |
| M2Y2 | `--m2y2` | Entropy-coded with binary range coder. ~20% smaller files, similar quality. Recommended. |

Both codecs produce visually identical output. M2Y2 adds a lossless compression pass.

## Profiles

| Profile | Flag | Use For |
| :--- | :--- | :--- |
| default | *(default)* | New 3DS, best quality |
| 3ds-fast | `--profile 3ds-fast` | Old 3DS — constrains per-packet video size for smoother decode |

## Quality Tuning

| Flag | Effect |
| :--- | :--- |
| `--qp N` | Quantization parameter. **Higher QP = smaller file, lower quality.** Lower QP = larger file, higher quality. Default: 28 |
| `--keep {4,8,16}` | Transform coefficients kept per 4×4 quadrant. 16 = max detail (larger), 4 = smallest files (less detail). Default: 16 |
| `--lambda N` | Rate-distortion tradeoff weight. Higher = more aggressive compression. |
| `--mv-range N` | Motion-vector search range. Smaller = faster encode, less efficient compression. |

### Tuning Examples

```bash
# Maximum quality (large file)
python encode_mivf.py input.mp4 output.mivf --m2y2 --qp 22 --keep 16

# Balanced (good quality, reasonable size)
python encode_mivf.py input.mp4 output.mivf --m2y2 --qp 28 --keep 8

# Small file, acceptable quality
python encode_mivf.py input.mp4 output.mivf --m2y2 --qp 38 --keep 4

# Old 3DS small file
python encode_mivf.py input.mp4 output.mivf --m2y2 --profile 3ds-fast --keep 4 --qp 34
```

## Audio Options

| Flag | Format | Characteristics |
| :--- | :--- | :--- |
| `--audio-codec ia4m` | IMA-ADPCM mono | Small, efficient. Default. |
| `--audio-codec pc16` | PCM 16-bit stereo | Higher quality, larger files. |

```bash
# High quality audio
python encode_mivf.py input.mp4 output.mivf --m2y2 --audio-codec pc16
```

## Frame Rate

```bash
# Override output frame rate
python encode_mivf.py input.mp4 output.mivf --m2y2 --fps 24

# Recommended for 24 fps sources: match audio rate to frame rate
python encode_mivf.py input.mp4 output.mivf --m2y2 --fps 24 --audio-rate 48000
```

The encoder defaults to the source frame rate. 24 or 30 fps are common choices for 3DS content. Using `--fps 24 --audio-rate 48000` for 24 fps sources yields exactly 2000 audio samples per frame (48000/24), aligning audio and reducing drift.

## Seek Index

The encoder generates a seek index by default. This is written both as:

1. A sidecar `.idx` file next to your `.mivf`
2. An embedded footer inside the `.mivf` file itself

To disable:

```bash
--no-seek-index        # Skip sidecar .idx
--no-embedded-index    # Skip embedded footer
```

See [SEEK_INDEX.md](SEEK_INDEX.md) for details.

## Packet Size Report

Use `--report-packet-sizes` to print a histogram of per-video-packet sizes after encoding. This helps diagnose playback performance issues:

```bash
python encode_mivf.py input.mp4 output.mivf --m2y2 --report-packet-sizes
```

Large packets (>100 KB) may cause decode stalls on Old 3DS. If you see many large packets, try `--profile 3ds-fast` or lower `--keep`.

## Parallel Encoding

```bash
# Use 4 parallel workers (default: 8)
python encode_mivf.py input.mp4 output.mivf --jobs 4

# Adjust chunk size per worker
python encode_mivf.py input.mp4 output.mivf --chunk-frames 120
```

## Full Flag Reference

Run `python encode_mivf.py --help` for the complete list.

