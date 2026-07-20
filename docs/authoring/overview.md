# Encoding Videos to MIVF

The `encode_mivf.py` script converts video files into the `.mivf` format for playback on
your 3DS.

## Requirements

- Python 3
- `ffmpeg` on your system PATH

## Quick Start

```bash
# Single file, M2Y2 codec (recommended)
python encode_mivf.py input.mkv output.mivf --m2y2

# Reduced packet-size profile, intended for lower-performance hardware
python encode_mivf.py input.mkv output.mivf --m2y2 --profile 3ds-fast

# Batch encode a folder
python encode_mivf.py ./videos/ ./output/ --m2y2
```

## Codec Choice

| Codec | Flag | Characteristics |
| :--- | :--- | :--- |
| M2Y1 | *(default)* | Block-mode YUV420 codec (SKIP/RAW/DELTA/QRES/motion-compensated modes chosen via RDO). Larger files, lower decode cost. |
| M2Y2 | `--m2y2` | A lossless range-coded re-encode of the same M2Y1 payloads. ~20–30% smaller files, identical decoded output. Recommended. |

Both codecs produce visually identical output — M2Y2 is a lossless compression pass on
top of M2Y1, self-verified byte-exact by the transcoder.

## Profiles

| Profile | Flag | Use For |
| :--- | :--- | :--- |
| default | *(default)* | Highest configured quality |
| 3ds-fast | `--profile 3ds-fast` | Constrains per-packet video size, intended to reduce decode-load pressure on lower-performance hardware — see [Hardware Matrix](../compatibility/hardware-matrix.md) for what has actually been tested |

## Quality Tuning

| Flag | Effect |
| :--- | :--- |
| `--qp N` | Quantization parameter. **Higher QP = smaller file, lower quality.** Default: 34 |
| `--keep {4,8,16}` | Transform coefficients kept per 4×4 quadrant. 16 = max detail (larger), 4 = smallest files (less detail). Default: 16 |
| `--lambda N` | Rate-distortion tradeoff weight. Higher = more aggressive compression. Default: 20.0 |
| `--mv-range N` | Motion-vector search range. Smaller = faster encode, less efficient compression. Default: 4 |
| `--motion-search {full,diamond,fast,hybrid,hierarchical}` | Motion search algorithm; see [cli-reference.md](../technical/cli-reference.md#quality-and-motion-search) and [PERFORMANCE_TUNING.md](../technical/performance-tuning.md#motion-search-modes) |

### Tuning Examples

```bash
# Maximum quality (large file)
python encode_mivf.py input.mkv output.mivf --m2y2 --qp 22 --keep 16

# Balanced (good quality, reasonable size)
python encode_mivf.py input.mkv output.mivf --m2y2 --qp 28 --keep 8

# Small file, acceptable quality
python encode_mivf.py input.mkv output.mivf --m2y2 --qp 38 --keep 4

# Reduced packet-size profile + small file settings combined
python encode_mivf.py input.mkv output.mivf --m2y2 --profile 3ds-fast --keep 4 --qp 34
```

## Audio Options

| Flag | Format | Characteristics |
| :--- | :--- | :--- |
| `--audio-codec ia4m` | IMA-ADPCM mono | Small, efficient. Default. |
| `--audio-codec pc16` | PCM 16-bit stereo | Higher quality, larger files. |

```bash
# High quality audio
python encode_mivf.py input.mkv output.mivf --m2y2 --audio-codec pc16
```

IA4M requires `--audio-channels 1` — the encoder will refuse otherwise.

## Frame Rate

MIVF supports both integer and **exact rational** frame rates. Rates are parsed with
Python's `Fraction` type — never a float approximation — and validated to fit the
container's rate fields before encoding starts.

```bash
# Integer rate
python encode_mivf.py input.mkv output.mivf --m2y2 --fps 24

# Exact rational rate (~23.976 fps), the validated production rate for film-sourced content
python encode_mivf.py input.mkv output.mivf --m2y2 --fps 24000/1001 --audio-rate 48000
```

The encoder defaults to 30 fps. The audio muxer requires an **exact whole number of
samples per video frame** — this is where the choice of audio rate matters:

| Frame rate | Audio rate | Samples/frame | Works? |
| :--- | :--- | :--- | :--- |
| `24000/1001` | 48000 Hz | 2002 | Yes — exact |
| `24/1` | 48000 Hz | 2000 | Yes — exact |
| `30/1` | 48000 Hz | 1600 | Yes — exact |
| `24000/1001` | 44100 Hz | — | **No** — 44100 doesn't divide evenly against `24000/1001`; the encoder raises an error rather than rounding |

If you're using an exact rational rate, pick an audio rate that divides evenly into it —
48000 Hz covers the common film-derived rates. This is a measured fact about these
specific combinations, not a guarantee for every rate you might try; if in doubt, run a
short test encode first.

## Resumable Jobs and Timing Reports E0 and E1

For long encodes, `--job-dir` keeps a persistent, resumable job directory, and
`--timing-json` writes a per-stage timing breakdown. See
[encoder-recovery-and-profiling.md](../technical/encoder-recovery-and-profiling.md) for the full
workflow, the exact resume-validation rules, and how to read the timing output correctly
(the console's early progress lines are not the authoritative FPS figure — the final
`ENCODE SUMMARY`/JSON `stages` block is).

## Seek Index

The encoder generates a seek index by default. This is written both as:

1. A sidecar `.idx` file next to your `.mivf`
2. An embedded footer inside the `.mivf` file itself

To disable:

```bash
--no-seek-index        # Skip sidecar .idx
--no-embedded-index    # Skip embedded footer
```

See [Seek Index](../technical/seek-index.md) for details.

## Packet Size Report

Use `--report-packet-sizes` to print a histogram of per-video-packet sizes after
encoding. This helps diagnose playback performance issues:

```bash
python encode_mivf.py input.mkv output.mivf --m2y2 --report-packet-sizes
```

Large packets (>100 KB) may cause decode stalls on Old 3DS. If you see many large
packets, try `--profile 3ds-fast` or lower `--keep`.

## Parallel Encoding

```bash
# Use 4 parallel workers (default: min(8, cpu_count))
python encode_mivf.py input.mkv output.mivf --jobs 4

# Adjust chunk size per worker
python encode_mivf.py input.mkv output.mivf --chunk-frames 120
```

See [PERFORMANCE_TUNING.md](../technical/performance-tuning.md#e2-benchmarking-jobs-and-chunk-frames)
for measured jobs/chunk-frames tradeoffs.

## Menus, Chapters & Other Sidecars

Subtitles, chapter markers, cover art, and DVD-style menu assets are all separate
sidecar files, not encoder flags — see [Files & Sidecars](files-and-sidecars.md) and
[menu-authoring.md](menu-authoring.md).

## Full Flag Reference

Run `python encode_mivf.py --help` for the complete list, or see
[Technical CLI Reference](../technical/cli-reference.md).
