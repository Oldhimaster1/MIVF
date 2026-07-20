# Chapters

Covers preparing `.chapters` and `.chapthumbs` sidecars for Scene Selection. Exact file
formats are the authoritative reference at
[Files & Sidecars](files-and-sidecars.md#chapters).

## `.chapters` — chapter markers

Plain text, one entry per line: `SECONDS Label`, `H:MM:SS Label`, or just `SECONDS`
(label optional). See [Files & Sidecars](files-and-sidecars.md#chapters) for the exact
format and comment syntax.

## `.chapthumbs` — Scene Selection thumbnails

```bash
python tools/mivf_build_chapter_thumbs.py input.mkv output.chapters output.chapthumbs
```

Generates chapter thumbnails for Scene Selection directly from the original source
video, using an existing `.chapters` sidecar for the timestamps. The player reads a flat
pixel array at runtime — it never decodes the compressed `.mivf` to produce a thumbnail,
which is why authoring happens against the original source, not the encoded output. If
the resulting `.chapthumbs` count doesn't match the `.chapters` sidecar loaded at menu
time, the player falls back to a plain text chapter list instead of guessing — see
[Scene Selection](../player/scene-selection.md).
