# MIVF AI Context

## Project Summary
MIVF is a Nintendo 3DS video player and encoder project.

Runtime code is concentrated in source/main.c with support code in:
- source/mivf_stream.c and source/mivf_stream.h
- source/mivf_io_ring.c
- source/mivf_settings.c and source/mivf_settings.h
- encode_mivf.py
- tools/miv2y_moflex_tier.c

## Maintainer Workflow
1. Inspect current code and git state.
2. Identify active runtime path.
3. Back up before patching.
4. Patch one small coherent feature.
5. Build.
6. Report exact changes.
7. Do not commit automatically.

## Do Not Touch Unless Explicitly Required
- audio startup, audio.ready, NDSP
- IA4M and PC16 runtime decode
- MoFlex playback
- M2Y/M2Y2 decode and packet parsing
- mivf_io_ring.c
- mivf_stream.c and mivf_stream.h
- encoder behavior
- seek-index logic

## Known Good Commits
- 07e1725: Skip sync seek index for large uncached movies
- dc07659: Generate seek index sidecar during encoding
- a16d8e5: Add embedded seek index footer reader
- e614091: Embed seek index footer during encoding
- 3a64a8c: Stop streaming before embedded seek index metadata
- d23c327: Polish active fluent and browser UI visuals
- 1d234e7: Polish playback footer and alert severity visuals

## Active UI Paths
Playback bottom UI:
- hfix51c_present_finish
- hfix51c_draw_bottom_ui_throttled
- hfix51c_draw_bottom_ui
- hfix58d_draw_bottom_fluent_ui

Browser UI:
- hfix58_file_browser_select
- hfix58_browser_redraw
- hfix58_draw_browser
- hfix58_draw_browser_preview

Settings UI:
- hfix59r3_handle_settings_menu
- hfix59r3_draw_settings_overlay

Alert/status UI:
- hfix58_alert_set
- hfix58_alert_clear
- hfix58_draw_alert

Inactive UI functions to avoid:
- hfix58b_draw_bottom_glass_ui
- hfix57_draw_transport_dock

## Build Commands
Player build:
C:\msys64\usr\bin\bash.exe -lc 'cd /c/dev/MIVF || cd /c/dev/mivf; source /etc/profile.d/devkit-env.sh 2>/dev/null || true; make clean; make'

Encoder syntax check:
python -m py_compile encode_mivf.py

## Safety Grep
git diff -- source/main.c source/mivf_settings.c source/mivf_settings.h source/mivf_stream.c source/mivf_stream.h | grep -nE "audio_configure|audio.ready|IA4M|PC16|ndsp|NDSP|moflex_set_audio_enabled|audio_queue|DSP_FlushDataCache|waveBuf|mivf_io_ring"

Expected: no output unless the task explicitly touches that subsystem.

## Runtime Smoke Checklist
1. Launch app.
2. Browser opens.
3. MIVF opens.
4. Audio starts.
5. Pause/resume works.
6. Seek works if index exists.
7. Large no-index movie does not freeze.
8. Settings opens.
9. Subtitles show if matching srt exists.
10. MoFlex dispatch works if available.
11. Return to browser works.
