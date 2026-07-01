# Player Usage

This section documents controls, settings, and sidecar files.

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

Use D‑Pad ↑/↓ to move, A / ← / → to change, and B or SELECT to close and save. Settings persist to `sdmc:/mivf_settings.ini`. Bookmarks and favorites live next to it on the SD card.

Sidecar files

Place these next to `yourvideo.mivf`:

- `yourvideo.srt`, `yourvideo.1.srt` — Subtitle tracks (cycle with Y)
- `yourvideo.chapters` — Chapter marks: `SECONDS Label`, `H:MM:SS Label`, or just `SECONDS`
- `yourvideo.cover` — Poster (raw RGB565, browser‑preview sized)
- `yourvideo.nfo` — Synopsis text shown in the browser
