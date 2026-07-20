# Troubleshooting

Common issues and how to resolve them.

## Encoder Issues

### "ffmpeg not found"

The encoder requires `ffmpeg` on your system PATH.

- **Windows:** Download ffmpeg from [ffmpeg.org](https://ffmpeg.org/download.html), extract it, and add the `bin/` folder to your PATH.
- **Linux:** `sudo apt install ffmpeg` or equivalent.
- **macOS:** `brew install ffmpeg`.

### "No such file or directory" during encoding

Check that your input file path is correct. Use absolute paths if needed:

```bash
python encode_mivf.py "C:/videos/input.mp4" "C:/videos/output.mivf" --m2y2
```

### Encoded file won't play / "MIVF invalid"

- Make sure you're using the `.mivf` output file, not an intermediate file.
- Try re-encoding without `--m2y2` to rule out codec-specific issues.
- Check that `ffmpeg` can read your source file: `ffmpeg -i input.mp4`

### Helper binary not found

The encoder uses native helper binaries for encoding and M2Y2 transcoding. These are in the `tools/` directory. Make sure they are built or downloaded, and placed where the encoder can find them (same directory as `encode_mivf.py` or in `tools/`).

### High RAM usage

Lower `--chunk-frames`, reduce `--jobs`, or run on a machine with more RAM.

### Packet-size report shows very large packets

Large packets cause decode stalls on hardware. Apply more aggressive tuning:

```bash
python encode_mivf.py input.mp4 output.mivf --m2y2 --profile 3ds-fast --keep 4 --qp 38
```

### Slow M2Y2 pass

`--m2y2` adds a CPU-intensive range-coding pass that is lossless. If encode time is critical, omit `--m2y2` (use default M2Y1).

## Player Issues

### Browser feels slow on Old 3DS

This was addressed in recent updates:
- Preview loading is debounced (loads only after cursor stops for ~200 ms)
- Settings are saved on close, not on every value change

If the browser still feels slow, try fewer files in the scanned folders, or use a faster SD card.

### Seek doesn't work / is very slow

- Check that a seek index exists: either a `.idx` sidecar file or the embedded footer (encoder generates both by default).
- For very large files without an index, the player skips the expensive scan — seeking will be approximate.
- Re-encode with index generation: `python encode_mivf.py input.mp4 output.mivf --m2y2`

### No subtitles showing

- Verify your `.srt` file has the exact same base name as the `.mivf` file (e.g., `movie.mivf` + `movie.srt`).
- Cycle subtitle tracks with **Y** during playback. Track 0 is "off."
- Make sure subtitles are enabled in Settings (SELECT → Subtitles → ON).
- Check that your `.srt` file is UTF-8 encoded.

### Audio not working

- Check that the file has an audio track (the encoder includes audio by default).
- Make sure volume isn't muted (L + D-Pad ↑/↓).
- On some setups, NDSP may not initialize. The player falls back to video-only mode.

### Settings not saving

- Settings are saved when you close the settings menu (B or SELECT), not on every change.
- After adjusting settings, press B or SELECT to close and save.
- Check `sdmc:/3ds/mivf_player_3ds/appdata/settings.ini` exists after closing.

### MoFlex file won't open

- Only unencrypted `.moflex` files are supported.
- Some `.moflex` files from certain sources may use different container layouts.
- Try the file in Azahar emulator first to rule out SD card issues.

### App freezes on large movie

- Large files without a seek index used to cause a synchronous scan freeze. This was fixed — the player now skips the scan and uses approximate seeking.
- Re-encode with index generation for best results.

## Build Issues

### "3ds.h: No such file or directory"

You need devkitPro with the `3ds-dev` group installed. The devkit environment variables (`DEVKITARM`, `DEVKITPRO`) must be set. Build from the MSYS2 shell that comes with devkitPro.

### "makerom not found"

`makerom` is only needed for `make cia`. Download it from [Project_CTR releases](https://github.com/3DSGuy/Project_CTR/releases) and place it in `devkitPro/tools/bin/`.

### "bannertool not found"

Same as makerom — download from [bannertool releases](https://github.com/Epicpkmn11/bannertool/releases).

## Azahar Emulator

- Point the emulator's SD card path to a folder containing a `mivf/` subfolder with your `.mivf` files.
- The emulator's SD card I/O is faster than real hardware; performance issues may not reproduce.
- Test on real hardware before concluding performance is acceptable.
