# Artwork

Covers preparing the raw-RGB565 image sidecars used by the Library and DVD-style menu.
Exact formats and load order are the authoritative reference at
[Files & Sidecars](files-and-sidecars.md#cover-poster-and-preview).

| Sidecar | Size | Used by |
| :--- | :--- | :--- |
| `yourvideo.cover` | 88×50 RGB565 | Library list poster |
| `yourvideo.preview.cover` | 176×100 RGB565 | Library preview panel |
| `yourvideo.menu_bg.cover` | 400×240 RGB565 | DVD-style menu background |
| `yourvideo.screensaver.cover` | 96×54 RGB565 (10,368 bytes exactly) | Idle screensaver |

## Screensaver image

```bash
python tools/mivf_make_screensaver_cover.py cover_art.png output.screensaver.cover
```

Converts and sizes a source image for the player's idle screensaver.

## Cover / preview / menu background

There is currently no single dedicated conversion script documented for `.cover`,
`.preview.cover`, or `.menu_bg.cover` beyond producing a headerless raw RGB565 file at
the exact dimensions above. This page is a placeholder for that tooling — see
[Files & Sidecars](files-and-sidecars.md) for the exact byte layout each format needs in
the meantime.
