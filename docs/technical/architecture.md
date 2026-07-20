# Architecture Overview

MIVF is a homebrew video player for the Nintendo 3DS. This document describes the high-level architecture.

## Pipeline

```
Video File (.mp4, .mkv)
        │
        ▼
    [ffmpeg decode]
        │
        ▼
    [Native encoder]  ← tools/
        │
        ▼
    .mivf file  ────────→  SD card
        │
        ▼
    [MIVF Player]  ← source/
        │
        ▼
    3DS screens + audio
```

## Encoder (PC Side)

`encode_mivf.py` orchestrates the pipeline:

1. **ffmpeg** decodes the source video to raw YUV420 frames and audio, at an integer or
   exact rational frame rate (e.g. `24000/1001`, parsed with exact fraction math).
2. The **native encoder** (C binaries under `tools/`) compresses frames:
   - M2Y1: block-mode YUV420 codec, RDO-chosen per block, fast decode.
   - M2Y2: a lossless range-coded re-encode of M2Y1 for ~20–30% smaller files.
   - Motion search is selectable (`full`/`diamond`/`fast`/`hybrid`/`hierarchical`) — see
     [encoder-internals.md](encoder-internals.md) and
     [PERFORMANCE_TUNING.md](performance-tuning.md#motion-search-modes).
3. Audio is encoded as IA4M (IMA-ADPCM mono) or PC16 (PCM stereo), at a sample rate
   compatible with the chosen frame rate.
4. A **seek index** is generated, mapping frame numbers to file byte offsets.
5. Everything is muxed into the `.mivf` container.
6. Optionally, the encode runs as a resumable job with per-stage timing output — see
   [encoder-recovery-and-profiling.md](encoder-recovery-and-profiling.md).

## MIVF Container

A `.mivf` file is a page-based container:

- **Pages:** fixed-size blocks containing header + payload.
- **Packets:** individual video frames or audio chunks within a page.
- **Streams:** independent data tracks (video, audio) identified by stream ID.
- **Seek index footer:** appended at the end for fast random access.

See [MIVF Format](format.md) for the detailed format specification.

## Player (3DS Side)

### Startup

1. `main()` initializes the 3DS hardware (GSP, NDSP, APT).
2. Opens the file browser on the bottom screen.
3. User selects a `.mivf` or `.moflex` file.

### Streaming

`mivf_stream.c` handles reading from the SD card:

- Pages are read into a ring buffer (`mivf_io_ring.c`).
- A background reader thread prefetches pages ahead of the decoder.
- The streamer recognizes the embedded seek index footer and stops before it.

### Decoding

- **Video:** M2Y1/M2Y2/M2Y0/RAWV codec paths in `main.c`. Frames are decoded to YUV420, converted to RGB565, and blitted to the top screen framebuffers.
- **Audio:** IA4M (ADPCM) or PC16 (PCM) samples are decoded and queued to NDSP wave buffers.

### UI

The entire UI is drawn directly into RGB565 framebuffers:

- **Top screen:** video playback (left/right eyes for 3D); or, when the DVD-style menu
  is open, the animated background with the root-menu buttons (Play/Resume/Scene
  Selection/Back) laid over it.
- **Bottom screen:** file browser, transport controls, settings overlay, alerts; or,
  when the DVD-style menu is open, a contextual dashboard panel in the root view, or the
  Scene Selection thumbnail grid.

UI functions follow a naming convention:
- `hfix51c_*`: core presentation
- `hfix58*_*`: browser, file selection, preview (Library UI — see [Movie Menus](../player/movie-menus.md))
- `hfix58d_*`: fluent-style playback UI
- `hfix58f_*`: timeline, seek UI
- `hfix58s_*`: subtitles
- `hfix59r3_*`: settings menu
- `hfix60_*`: chapters, favorites, cover/nfo sidecars
- `mivf_menu_*`: the DVD-style per-movie menu (root menu, Scene Selection, background,
  fades) — a separate naming convention from the `hfix*` browser/playback code, see
  [Movie Menus](../player/movie-menus.md) and [Menu Authoring](../authoring/menu-authoring.md)

### MoFlex

The MoFlex playback path (`source/moflex/`) handles `.moflex` (MobiClip 3D) files:

- **Demuxer:** `moflex_demux.c` — parses the MoFlex container and emits per-stream packets.
- **Video decoder:** `mobiclip.c` — FFmpeg-derived Mobiclip decoder.
- **Audio decoder:** `adpcm_moflex.c` — IMA-ADPCM audio.
- **Playback loop:** `moflex_playback.c` — orchestrates demux, decode, render.

### Seek Index

- At file open, the player checks for an embedded MIDX footer, then a `.idx` sidecar.
- If found, the index is loaded into memory for O(1) seek lookups.
- If not found and the file is small enough, a binary search builds a coarse index.
- Very large files without an index skip this scan to avoid freezing.

## Key Design Decisions

- **No dynamic memory in the hot path:** video frames are decoded into pre-allocated buffers.
- **Streaming from SD:** pages are read in the background to overlap I/O with decode.
- **Direct framebuffer rendering:** no GPU abstraction layer; draws directly into GSP framebuffers.
- **Single-threaded main loop:** uses `aptMainLoop()` with hidKeys polling and vblank sync.
- **Settings persistence:** written to SD only on settings close, not on every value change.
