# Building MIVF Player

Building MIVF Player from source.

## Prerequisites

- **devkitPro** with the `3ds-dev` group installed:
  - devkitARM (ARM11 cross-compiler)
  - libctru (3DS userspace library)
- **makerom** and **bannertool** (for `.cia` builds only):
  - [makerom](https://github.com/3DSGuy/Project_CTR/releases)
  - [bannertool](https://github.com/Epicpkmn11/bannertool/releases)
  - Place both in `devkitPro/tools/bin/` or anywhere on PATH.

### Windows Setup

1. Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the 3DS development group.
2. Use the MSYS2 shell that comes with devkitPro for building.
3. The devkit environment is automatically configured in the MSYS2 shell.

Build command from MSYS2:

```bash
cd path/to/MIVF
make
```

## Build Targets

### .3dsx (Homebrew Launcher)

```bash
make
```

Produces `mivf_player_3ds.3dsx` with embedded SMDH icon/metadata.

To skip the icon:

```bash
make NO_SMDH=1
```

### .cia (Installable Title)

```bash
make cia
```

Requires `makerom` and `bannertool` on PATH. Produces `mivf_player_3ds.cia`.

### Clean

```bash
make clean
```

Removes build artifacts.

## Repository Layout

| Path | Contents |
| :--- | :--- |
| `source/main.c` | Application entry point, UI, browser, settings, playback orchestration |
| `source/mivf_stream.c/.h` | Page-based streaming reader from SD card |
| `source/mivf_io_ring.c` | I/O ring buffer for background reading |
| `source/mivf_settings.c/.h` | Settings persistence, bookmarks, app data |
| `source/mivf_subtitles.c/.h` | `.srt` subtitle parser |
| `source/moflex/decoder/` | MoFlex demuxer and Mobiclip decoder (FFmpeg-derived) |
| `source/moflex/playback/` | MoFlex playback loop, Y2R video conversion |
| `source/moflex/ffmpeg_support/` | Bundled FFmpeg support files (VLC, Golomb, etc.) |
| `source/moflex/ui/` | MoFlex in-player UI graphics |
| `tools/` | Native encoder, M2Y2 transcoder, and helper binaries |
| `meta/` | Icon, banner image/audio, makerom RSF |
| `encode_mivf.py` | Python encoder front-end (ffmpeg → .mivf) |
| `docs/` | Project documentation |

## Test Flow

1. Build: `make clean && make`
2. Copy `mivf_player_3ds.3dsx` to `sdmc:/3ds/`
3. Put test `.mivf` files in `sdmc:/mivf/`
4. Launch via Homebrew Launcher or Azahar emulator

See [Building the Toolkit](building-toolkit.md) for the desktop encoder/tools build, and
[Testing](testing.md) for the test suite.
