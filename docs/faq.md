# FAQ

What is MIVF?
- MIVF is a small, page-based video container format designed for Nintendo 3DS playback with custom software codecs (M2Y1/M2Y2) and IA4M audio.

What is M2Y2?
- M2Y2 is a range-coded variant of the M2Y1 video packets: it is lossless with respect to the decoded pixels and achieves ~20–25% smaller files in many cases.

Is M2Y2 lossless?
- Yes: the provided `m2y2_transcode` performs a verification during conversion and ensures decoded quality is identical.

Why use 48000 Hz with 24 fps?
- 48000 / 24 = 2000 samples per frame, which yields an exact integer samples-per-frame value and avoids timing drift for 24 fps sources.

What about film-rate (23.976 fps) sources?
- Use `--fps 24000/1001 --audio-rate 48000` — that's exactly 2002 samples per frame, computed with exact fraction math rather than a rounded float. `--audio-rate 44100` does **not** divide evenly against `24000/1001`, so the encoder rejects that combination outright rather than producing audio that slowly drifts out of sync. See [encoding.md](encoding.md#frame-rate).

Why is the file larger or smaller than expected?
- Size depends on settings (QP/keep/lambda) and source complexity. Use the recommended presets for predictable sizes.

Can I play MP4s directly on 3DS?
- No: the hardware player expects MIVF-formatted files and the native codecs used here. Use the encoder to transcode MP4/MKV into `.mivf`.

Where do subtitles go?
- Place SRT sidecar files next to the `.mivf` file (e.g., `yourvideo.srt`). The player supports multiple subtitle tracks named `yourvideo.N.srt`.
