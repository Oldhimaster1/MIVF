# Quickstart

This quickstart gets you an installed player and shows how to make a minimal `.mivf` file for the Nintendo 3DS.

Install the player

- For the Homebrew Launcher: download the release `mivf_player_3ds.3dsx` and place it on the SD card in `sdmc:/3ds/`.
- For a HOME-menu icon: download `mivf_player_3ds.cia` and install it using a title manager such as FBI.

Where to put videos

Place compiled `.mivf` videos on the SD card in any of these locations:

- `sdmc:/mivf/`
- `sdmc:/3ds/mivf_player_3ds/`
- SD card root (the player scans the root as well)

Minimal encode example

Use the provided encoder to convert existing video files into `.mivf`. A minimal example (Windows exe or script):

encode_mivf.exe input.mp4 output.mivf --no-deploy

This will produce `output.mivf` in the current directory. For better results, see the Encoding page for recommended settings.
