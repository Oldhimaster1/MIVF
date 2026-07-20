# Roadmap

- Complete documented, model-specific physical-hardware validation of the Library,
  DVD-menu, Scene Selection, and Personalization work (currently emulator-confirmed,
  plus one preliminary physical-hardware run reported) — see [Project Status](../status.md).
- Resolve the high-quality-encode real-time decode margin: evaluate `hybrid` motion
  search as a mitigation, continue encoder throughput work (motion search, chunking)
  without compromising determinism, rational-timing accuracy, or M2Y2 losslessness.
- **Phase 9 (planned, not implemented):** a redesigned bottom-screen playback dashboard —
  unobstructed top video area except for temporary overlays; cached/dirty-rendered
  panels for title, state, chapter, a rational-aware timeline with chapter/A-B markers,
  transport, volume, subtitles, speed, aspect, codec status, battery, and clock; a
  timeline that updates at roughly 5–10 Hz and a clock at 1 Hz, with immediate input
  feedback and animations only on real events. This explicitly does **not** touch NDSP,
  the audio clock, rational-FPS timing, decoding, seeking, or scheduling — those stay
  frozen.
- Expand Settings, Help, toasts, and subtitle handling.
- Add Setup / Movie Info / Bonus Features menu pages to the DVD-style menu.
- Expand which sidecar assets can be embedded via MASB (chapter thumbnails, screensaver
  image currently cannot be).
- Complete hardware regression testing and release packaging for everything currently
  build-verified only.
