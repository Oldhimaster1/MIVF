# MoFlex Status

## Current State

MIVF includes a built-in MoFlex (MobiClip 3D video) playback backend under `source/moflex/`. This is compiled into the player and is **enabled by default**.

- `.moflex` files are recognized in the file browser and show a "MOFLEX" text badge (this
  is a plain text label, not an icon/color system, and can be overridden by the
  Favorite/Resume/Recent badges — see [player-ui.md](player-ui.md)).
- Opening a `.moflex` file dispatches to `moflex_play()`, which handles demux, video decode, audio decode (via NDSP channel 0), and 3D rendering.
- MoFlex playback returns control to the browser when the file ends or the user presses B/START.
- Resume bookmarks are supported for `.moflex` files.

## What Works

- MobiClip video decode (via adapted FFmpeg `mobiclip.c`)
- IMA-ADPCM audio decode
- Stereo 3D output on the top screen
- Play/pause, seek, and exit controls
- Resume position tracking

## What Is Not Integrated

An external bundle (`moflex_for_mivf`) was reviewed in July 2026 and **not wholesale integrated**. That bundle contained:

- A full alternative playback stack with threading, GPU blit workers, and citro2d/citro3d dependencies
- CIA-embedded MoFlex container parsing
- ARM motion-compensation assembly
- Broader NDSP/audio ownership changes

These changes were too broad and risky for a single integration. They touched multiple compiled files, required new libraries and build-system changes, and rewrote core playback paths.

See [docs/MOFLEX_FOR_MIVF_REVIEW.md](MOFLEX_FOR_MIVF_REVIEW.md) for the full review.

## Future Plans

Potential staged integration (no timeline):

1. Review CIA-embedded MoFlex helper code in isolation
2. Evaluate demux improvements individually
3. Consider playback path changes only with isolated testing

## Known Limitations

- MoFlex playback performance depends on content complexity and hardware (New 3DS recommended).
- Only unencrypted `.moflex` files are supported.
- MoFlex playback uses NDSP channel 0, which is released when playback ends.
