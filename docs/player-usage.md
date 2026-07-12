# Player Usage

This section documents controls, settings, and sidecar files. It's a condensed summary —
[CONTROLS.md](CONTROLS.md) has the full context-by-context control reference (including
the DVD-style menu and Resume modal, where button meaning changes by screen), and
[FILES_AND_SIDECARS.md](FILES_AND_SIDECARS.md) has the full sidecar/asset reference.

File Browser Controls

- D-Pad ↑ / ↓ — move selection
- A — Play
- Y — Toggle favorite
- B / START — Exit

Playback Controls

- A — Play / Pause
- ← / → — Seek −/+ (~5 s)
- Touch + drag — Scrub timeline
- X — Cycle playback speed (0.5×–2.0×)
- B — A/B loop: set A → set B → clear
- Y — Cycle subtitle track
- L + D‑Pad — Audio (volume / stereo)
- R + ↑/↓ — Screen brightness
- R + ←/→ — Previous / next chapter
- SELECT — Open settings
- START — Stop and return to the browser

Settings Menu

Use D‑Pad ↑/↓ to move, A / ← / → to change, and B or SELECT to close and save. Settings persist to `sdmc:/3ds/mivf_player_3ds/appdata/settings.ini` (legacy root-level `sdmc:/mivf_settings.ini` is still read for migration only). Bookmarks and favorites live alongside it under the same `appdata/` folder — see [FILES_AND_SIDECARS.md](FILES_AND_SIDECARS.md#app-data-layout).

Sidecar files

Place these next to `yourvideo.mivf`:

- `yourvideo.srt`, `yourvideo.1.srt` — Subtitle tracks (cycle with Y)
- `yourvideo.chapters` — Chapter marks: `SECONDS Label`, `H:MM:SS Label`, or just `SECONDS`
- `yourvideo.cover` — Poster (raw RGB565, browser‑preview sized)
- `yourvideo.nfo` — Synopsis text shown in the browser
