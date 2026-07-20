# Menu & Scene Selection Authoring

How to give a movie a DVD-style menu: a `.menu.ini` sidecar, a `.menu_bg.cover`
background image, and (optionally) a `.chapthumbs` file for Scene Selection thumbnails.
All formats below were read directly from the player's parser
(`source/main.c`, `mivf_menu_load_for_movie` / `mivf_menu_load_chapter_thumbs` /
`mivf_menu_load_background`) — see [Movie Menus](../player/movie-menus.md) for how these
are used at runtime and [Files & Sidecars](files-and-sidecars.md) for the full sidecar
reference.

## 1. `.menu.ini`

Plain-text INI, parsed as a deliberately small, safe subset — unrecognized keys
(`background=`, `style=`, `up=`/`down=`, etc.) are silently ignored rather than
supported. The background image is **never** a path from the ini — it's always the
fixed `.menu_bg.cover` sidecar (or an embedded MASB asset) — so there is no
path-traversal surface here.

```ini
[MIVF_MENU]
title=Example Movie

[BUTTON play]
label=Play
x=40
y=120
w=160
h=24
action=play

[BUTTON resume]
label=Resume
x=40
y=150
w=160
h=24
action=resume

[BUTTON chapters]
label=Scene Selection
x=40
y=180
w=160
h=24
action=chapters

[BUTTON back]
label=Back to Library
x=40
y=210
w=160
h=24
action=back
```

- `[MIVF_MENU]` → `title=` sets the menu title text.
- `[BUTTON <id>]` → up to **8 buttons** (`id` is just a label for your own reference, up
  to 16 characters; it isn't otherwise meaningful to the player).
  - `label=` — button text, up to 32 characters.
  - `x=`, `y=`, `w=`, `h=` — on-screen position/size in top-screen pixels. Invalid or
    unset `w`/`h` default to 120×22; all four are clamped so the button stays fully
    within the 400×240 top screen.
  - `action=` — **must** be exactly one of `play`, `resume`, `chapters`, `back`. Anything
    else (typos included) is parsed without error but leaves the button permanently
    disabled — it will never be selectable, and won't appear in the D-pad navigation
    order. There is no `setup` action or any other action.

## 2. `.menu_bg.cover` (animated background)

Raw RGB565, headerless, **exactly 400×240 pixels**. The player pans/zooms this image
automatically (Ken Burns-style, integer/fixed-point) — there's nothing to author for the
animation itself, just supply one static image.

If you'd rather not ship a separate sidecar file, the same asset can be embedded
directly into the `.mivf` via `tools/mivf_embed_assets.py` (see §4) — currently the only
sidecar type the player will also read back from an embedded bundle. A sidecar, if
present, always takes priority over the embedded copy.

## 3. `.chapthumbs` (Scene Selection thumbnails)

Generate this from the **original source video** (not the encoded `.mivf`) plus the
movie's existing `.chapters` sidecar:

```bash
python tools/mivf_build_chapter_thumbs.py input.mkv output.chapters output.chapthumbs
```

This grabs one frame per chapter timestamp directly from the source via `ffmpeg -ss`,
downscales it to 96×54, and packs all of them into one binary sidecar
(magic `MCTH`, version, count, width, height, then `count × 96 × 54` RGB565LE pixels —
one thumbnail per chapter, in chapter order).

The player's reader is strict: if the thumbnail count doesn't match the number of
chapters loaded from the same movie's `.chapters` sidecar at menu time, it discards the
whole file and falls back to a plain text chapter list instead of guessing. **Re-run
this tool any time `.chapters` changes** (chapters added, removed, or reordered) — the
two files must stay in sync.

`--ffmpeg PATH` overrides the ffmpeg executable if it isn't on `PATH`.

## 4. Embedding assets into the `.mivf` (optional)

```bash
python tools/mivf_embed_assets.py input.mivf output.mivf \
    --menu-bg input.menu_bg.cover \
    --menu input.menu.ini --chapters input.chapters --nfo input.nfo --idx input.idx
```

Always writes a **new** file — `input.mivf` is never modified in place (a rewrite that's
interrupted partway through a multi-gigabyte file has no good recovery path, so this
tool doesn't attempt one). If `input.mivf` already ends in an embedded asset bundle, that
bundle is stripped and replaced with exactly what you pass on this run — bundles are
replaced, not merged across runs.

Every flag shown is accepted and packed into the bundle, but **only `menu-bg` is
currently read back by the player** (`mivf_menu_load_background`); the other asset types
are stored for a future player update, not because the tool or format needs to change
again later. Treat `--chapters`/`--nfo`/`--idx`/`--menu` embedding as forward-looking
storage, not a currently-effective override.

To inspect what's embedded in a file:

```bash
python tools/mivf_list_assets.py output.mivf
```

Exits with a message (not an error) if the file has no bundle — that's the normal state
for a plain sidecar-only `.mivf`.

## 5. Screensaver image (optional)

The DVD menu's idle screensaver (root menu view only) can show a custom image instead of
its default:

```bash
python tools/mivf_make_screensaver_cover.py cover_art.png output.screensaver.cover
```

Produces a raw RGB565 file at exactly 96×54 pixels (10,368 bytes) — the exact size and
format the player's loader expects. Useful flags: `--bg R,G,B` (letterbox/pillarbox
color, default black), `--margin N` (safe margin around the fitted image), `--bgr565`
(swap R/B if your loader expects BGR565), `--preview PATH` (also write a PNG preview so
you can check it before copying to the SD card), `--force` (overwrite an existing
output). This asset is sidecar-only — there is currently no MASB embedding support for
it.


![MIVF DVD-menu screensaver showing custom artwork over a dark theatre background with a wake hint on the bottom screen.](../assets/screenshots/screensaver.png)

*Emulator capture (Azahar), used for interface inspection — not physical-hardware
performance evidence.* The image proves that a custom `.screensaver.cover` can be loaded
and rendered. It is not a test of the normal idle trigger or long-running bounce
behavior.

## Putting it together

For `movie.mivf`, a full DVD-style menu setup is:

```
movie.mivf
movie.menu.ini
movie.menu_bg.cover
movie.chapters
movie.chapthumbs          (optional — falls back to a text chapter list without it)
movie.screensaver.cover   (optional — falls back to the default screensaver without it)
```
