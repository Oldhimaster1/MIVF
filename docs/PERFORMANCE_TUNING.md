# Performance Tuning

MIVF decode performance depends on the encoded file characteristics and the target 3DS hardware.

## Hardware Differences

| Hardware | CPU | Recommendation |
| :--- | :--- | :--- |
| New 3DS | 4× ARM11 @ 804 MHz | Default encoder settings work well for most content. |
| Old 3DS | 2× ARM11 @ 268 MHz | Use `--profile 3ds-fast` and consider lowering `--keep`. |

## Encoder Profile: 3ds-fast

The `--profile 3ds-fast` flag constrains per-video-packet sizes to prevent decode stalls on the slower Old 3DS CPU.

```bash
python encode_mivf.py input.mp4 output.mivf --m2y2 --profile 3ds-fast
```

This adjusts encoder parameters to produce smaller, more evenly-sized video packets at the cost of some compression efficiency.

## Reducing Per-Packet Cost

If playback is still choppy on Old 3DS, try progressively more aggressive settings:

```bash
# Step 1: 3ds-fast profile
--profile 3ds-fast

# Step 2: Reduce transform coefficients
--keep 8

# Step 3: More aggressive
--keep 4 --qp 34

# Step 4: Lower frame rate
--fps 24
```

## Motion Search Modes

`--motion-search` trades encode time against output size/quality. `full` (the default) is an exhaustive search and is the only mode with a long production track record — it's the safe choice.

`diamond`, `fast`, and `hybrid` are experimental. In local validation on a handful of test clips:

- `full`, `diamond`, and `fast` each produce **byte-identical output** to their own prior baselines across repeated runs — `hybrid` is purely additive and doesn't change their behavior at all.
- `diamond` encoded noticeably faster than `full`, at a cost of roughly a few percent larger files (varied by content, up to ~5-6% on some clips) and a small PSNR loss.
- `hybrid` tracked `diamond`'s speed closely (single-digit percent slower in local timing) while generally landing closer to `full`'s file size than `diamond` did — the improvement over `diamond` was clip-dependent and not uniform across all content tested.

These are small-sample local results, not a guarantee for every source — if size/quality matters more than encode time for a given project, `full` remains the safest choice. If you try `diamond`/`fast`/`hybrid`, use `--report-packet-sizes` and compare the `ENCODE SUMMARY` output (frame-weighted PSNR, mode histogram, byte counts) against a `full` encode of the same source before committing to it for a release.

## Diagnosing with Packet Size Report

Use `--report-packet-sizes` to see the distribution of video packet sizes:

```bash
python encode_mivf.py input.mp4 output.mivf --m2y2 --report-packet-sizes
```

Output example:
```
Video packet size histogram (bytes):
    0- 16k: #### 42
   16- 32k: ##### 55
   32- 64k: ## 28
   64-128k: # 12
  128-256k: 3
```

**Guideline:** Keep most packets under 64 KB for Old 3DS. If you see many packets above 128 KB, apply more aggressive tuning.

## Content Factors

- **High motion** (action scenes, camera pans): produces larger packets. Consider `--profile 3ds-fast`.
- **Static scenes** (interviews, slideshows): encode efficiently, rarely need tuning.
- **High resolution:** 400×240 is the recommended maximum. Larger resolutions multiply decode cost.
- **Frame rate:** 30 fps at 400×240 is the typical sweet spot. 24 fps reduces decode load by ~20%.

## Browser Performance

The file browser has been optimized for Old 3DS:

- Preview thumbnail loading is debounced — the preview only loads after the cursor has been still for ~200 ms, keeping list scrolling smooth.
- Settings are saved only on menu close, not on every value change, preventing SD-card write stalls during adjustment.

## MoFlex Performance

MoFlex (MobiClip) playback is more demanding than MIVF codecs:

- New 3DS handles typical content well.
- Old 3DS may struggle with 3D (frame-interleaved) MoFlex content.
- 2D MoFlex content is generally easier to decode.
