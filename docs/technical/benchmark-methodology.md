# MIVF benchmark graph data audit

Source bundle SHA-256: `87c4c90129a42bff50ff5bc5b98615ab69fd08b1914a7e4f7c8680ec21875d88`.

## Source selection
Only originals under `converted_encoder_e2_5min/`, `converted_final_speed_matrix_1min/`, and `converted_les_mis_quality_matrix_1min/` were quantitative sources. Audit-bundle copies, documentation, chat logs, historical patches, and release snapshots were excluded.

## E2
- All 12 timing JSON files use `mivf-encoder-stage-timings-v1`, report 7,193 frames at 24000/1001, and match paired timing CSV stage values.
- Paired outer-wall files match the roll-up outer times.
- Winner by outer wall: `j12_c240`: 47.778709s outer, 46.663227s encoder total, 28.681782s video stage.
- Correct throughput: `7193 / 28.681782 = 250.786 fps`. The roll-up's `8.367681` field is rejected.
- Cases with chunk sizes 240/480/960 share one output hash and size. All 120-frame cases share a different hash and larger size, consistent with more independent starts.

## Motion search
- JSON summary matches CSV. Medians were recomputed from all three run rows. Each case has identical MIVF and index hashes across repeats.
- `hierarchical12` timings are 32.5, 12.3, 13.1 seconds: median 13.1 is correct, but variance is high and must be disclosed.
- Warm-start serializes chunk dependencies and is not a parallel-throughput mode.

## Quality matrix
- Each case reports 1,441 video and audio packets. MiB matches exact bytes. Hashes are present in SHA256SUMS. Raw logs match final bytes and rounded PSNR.

## Limits
PSNR is the helper's frame-weighted approximate aggregate. E2 is one run per combination on one source/machine. Motion search is one source with three repeats. Never use the E2 roll-up `video_fps` field.
