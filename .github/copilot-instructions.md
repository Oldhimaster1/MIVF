# Copilot Instructions for MIVF

This repository is fragile. Use a maintainer workflow.

Required workflow for every change:
1. Inspect current branch and diff.
2. Identify active runtime path.
3. Create backups.
4. Patch the smallest coherent feature.
5. Avoid unrelated subsystems.
6. Build or syntax-check.
7. Report exact changes.
8. Do not commit automatically.

Do-not-touch subsystems unless explicitly required:
- audio startup, audio.ready, NDSP
- IA4M/PC16 runtime decode
- MoFlex playback
- video decode
- mivf_io_ring.c
- mivf_stream.c and mivf_stream.h
- encoder logic
- seek-index logic

Active UI paths:
- hfix51c_present_finish
- hfix51c_draw_bottom_ui_throttled
- hfix51c_draw_bottom_ui
- hfix58d_draw_bottom_fluent_ui
- hfix58_draw_browser
- hfix58_draw_browser_preview
- hfix59r3_draw_settings_overlay
- hfix59r3_handle_settings_menu
- hfix58_alert_set
- hfix58_draw_alert

Inactive legacy UI functions to avoid:
- hfix58b_draw_bottom_glass_ui
- hfix57_draw_transport_dock
