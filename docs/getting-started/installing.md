# Installing MIVF Player

## Quick Install

1. Download the latest release from the [Releases page](https://github.com/Oldhimaster1/MIVF/releases).
2. Choose your install method below.
3. Put `.mivf` video files in `sdmc:/mivf/`.

## Method 1: HOME Menu Icon (.cia)

- Download `mivf_player_3ds.cia`.
- Install it using a title manager like **FBI**.
- The MIVF Player icon will appear on your HOME menu.

## Method 2: Homebrew Launcher (.3dsx)

- Download `mivf_player_3ds.3dsx`.
- Place it in `sdmc:/3ds/`.
- Launch via the Homebrew Launcher.

## Where to Put Videos

The player scans these folders (in order):

1. `sdmc:/mivf/` (recommended)
2. `sdmc:/3ds/mivf_player_3ds/`
3. `sdmc:/` (SD root)

Place your `.mivf` (and optional `.moflex`) files in any of these locations.

## App Data

Settings, bookmarks, favorites, recents, logs, and cache are stored under:

```
sdmc:/3ds/mivf_player_3ds/appdata/
```

If you previously used an older version of MIVF, legacy files at the SD root (`sdmc:/mivf_settings.ini`, `sdmc:/mivf_favorites`, etc.) are still read and migrated automatically.

## Sidecar Files

Optional sidecar files go next to your `.mivf` file. See [Files & Sidecars](../authoring/files-and-sidecars.md) for details.

## Azahar Emulator

MIVF works on the Azahar 3DS emulator. Point the emulator's SD card path to a folder containing `mivf/` with your `.mivf` files.
