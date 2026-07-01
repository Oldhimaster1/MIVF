MIVF Player for Nintendo 3DS

A homebrew video player for the Nintendo 3DS. MIVF is a custom, page‑based video container with its own software codecs, a native C player that runs on the console, and a standalone encoder that turns ordinary `.mp4` or `.mkv` files into `.mivf`.

It is built for the real hardware: the codecs are tuned for the ARM11, the player streams pages from the SD card with a background reader, and the whole UI (file browser, transport, settings) is drawn straight into the RGB565 framebuffers.

**Status:** Works on real hardware and on the Azahar emulator. Tested primarily at 400×240, 30 fps.

## Features

* **Custom codecs:** M2Y1 (raw token YUV420) and M2Y2 (the same picture, entropy‑coded with a division‑free binary range coder, ~24% smaller and still a locked 30 fps on hardware). Also reads RAWV, M2Y0, and PC16/IA4M audio.
* **File browser:** Includes thumbnails, posters, synopsis, and favorites.
* **Auto‑resume bookmarks:** Pick up exactly where you left off.
* **Playlists / auto‑advance:** Automatically play the next file in the folder.
* **Aspect‑ratio modes:** FIT (letterbox), STRETCH (fill), NATIVE (1:1).
* **A/B scene looper:** Mark two points and loop the section.
* **Playback speed:** 0.5× to 2.0×, audio stays in sync.
* **Sleep timer + clamshell pause/park:** Closes cleanly and resumes on wake.
* **Customization:** Subtitles (`.srt`, multiple tracks), chapters, themes, adjustable brightness, font scale, and a persistent settings menu.
* **Touch transport:** Drag the timeline to scrub.

---

## Controls

### File Browser
| Button | Action |
| :--- | :--- |
| **D‑Pad ↑/↓** | Move selection |
| **A** | Play |
| **Y** | Toggle favorite |
| **B / START** | Exit |

### Playback
| Button | Action |
| :--- | :--- |
| **A** | Play / pause |
| **← / →** | Seek −/+ (~5 s) |
| **Touch + drag** | Scrub timeline |
| **X** | Cycle playback speed (0.5×–2.0×) |
| **B** | A/B loop: set A → set B → clear |
| **Y** | Cycle subtitle track |
| **L + D‑Pad** | Audio (volume / stereo) |
| **R + ↑/↓** | Screen brightness |
| **R + ←/→** | Previous / next chapter |
| **SELECT** | Open settings |
| **START** | Stop and return to the browser |

### Settings menu (SELECT)
Use **D‑Pad ↑/↓** to move, **A / ← / →** to change, and **B or SELECT** to close and save. 

Items include: Resume bookmarks, Auto dim, Dim timeout/brightness, Force stereo, Debug overlay, Subtitle tracks, Chapters, Favorites, Theme skin, Font scale, Sleep on lid close, Screen brightness, Aspect ratio, Playback speed, Auto‑advance, and Sleep timer.

> **Note:** Settings, bookmarks, favorites, logs, cache, and benchmark data now live under `sdmc:/3ds/mivf_player_3ds/appdata/`. Legacy root-level settings/bookmark/favorites files are still read for migration.

---

## Installing

The easiest way to install MIVF Player is to grab the latest release from the **[Releases Page](../../releases)**.

1. **For a HOME-menu icon:** Download `mivf_player_3ds.cia` and install it using a title manager like FBI.
2. **For the Homebrew Launcher:** Download `mivf_player_3ds.3dsx`, place it in `sdmc:/3ds/`, and launch it via the Homebrew Launcher.

Put your compiled `.mivf` video files in `sdmc:/mivf/` (the player also scans `sdmc:/3ds/mivf_player_3ds/` and the SD root).

### Optional sidecar files
Place these next to `yourvideo.mivf`:

| File | Purpose |
| :--- | :--- |
| `yourvideo.srt`, `yourvideo.1.srt` | Subtitle tracks (cycle with Y) |
| `yourvideo.chapters` | Chapter marks: `SECONDS Label`, `H:MM:SS Label`, or just `SECONDS` |
| `yourvideo.cover` | Poster (raw RGB565, browser‑preview sized) |
| `yourvideo.nfo` | Synopsis text shown in the browser |

---

## Encoding videos to .mivf

You can download the standalone PC encoder from the **[Releases Page](../../releases)** to easily convert `.mp4` files into `.mivf` formats without needing to install Python. 

The `encode_mivf` tool wraps `ffmpeg` and the native encoder to process a single file or a whole folder.

> **Requirement:** `ffmpeg` must be available on your system (either bundled next to the executable or added to your system's PATH).

**Windows Example:**
```cmd
:: Smaller, entropy-coded M2Y2 (Recommended):
encode_mivf.exe input.mp4 output.mivf --m2y2

:: Faster, raw token M2Y1:
encode_mivf.exe input.mp4 output.mivf

::Best Balanced
encode_mivf.exe input.mp4 output.mivf --m2y2 --fps 24 --audio-rate 48000 --jobs 6 --seek-preroll 2 --keep 4 --mv-range 1 --qp 38 --lambda 34

```

**Linux Example:**

```bash
./encode_mivf input.mp4 output.mivf --m2y2

```

*(Developers can also run the Python front-end directly via `python encode_mivf.py input.mp4 output.mivf --m2y2`).*

---

## Developer Build Instructions

### Building the player (.3dsx)

You need devkitPro with the `3ds-dev` group (devkitARM + libctru).

```bash
# Make sure DEVKITPRO / DEVKITARM are set and the toolchain is on PATH, then:
make

```

This produces `mivf_player_3ds.3dsx` with the embedded SMDH icon/metadata. To skip the icon, run `make NO_SMDH=1`.

### Building an installable .cia

The `.cia` needs two third‑party tools that are not part of devkitPro:

* [makerom](https://github.com/3DSGuy/Project_CTR/releases)
* [bannertool](https://github.com/Epicpkmn11/bannertool/releases)

Drop both executables into `devkitPro/tools/bin` (or anywhere on your PATH), then:

```bash
make cia

```

This builds the banner, reuses the SMDH icon, and packages everything per `meta/app.rsf` into `mivf_player_3ds.cia`.

### Repository layout

* **`source/`** - Native 3DS player (C). `main.c` is the app; `mivf_*.c/.h` are modules. `mivf_rc.h` is the shared M2Y2 range coder.
* **`tools/`** - Native encoder + M2Y2 transcoder/verifier and helpers.
* **`meta/`** - Icon, banner, banner audio, makerom RSF, asset generator.
* **`Makefile`** - Builds the `.3dsx` (and `make cia` for the installable title).
* **`encode_mivf.py`** - Python front-end for ffmpeg -> `.mivf` encoding.

---

## Acknowledgements

Built with devkitPro / devkitARM and libctru. CIA packaging uses makerom and bannertool.

## License

Released under the MIT License.

