# Player UI: Library & DVD-Style Menu

This page describes the dual-screen Library browser and the per-movie DVD-style menu in
detail. Every feature below is individually status-rated — see
[validation-status.md](validation-status.md) for what the ratings mean. As a whole, this
UI layer is **build-verified**, and the Library browser, DVD-style root menu, and Scene
Selection have since been confirmed running correctly in the **Azahar emulator**. Other
parts of this layer (the menu-return fix, the screensaver, the Ken Burns animation's
actual motion, session-scoped cursor memory) remain build-verified only, not yet
emulator-confirmed. **None of it has had a physical-hardware regression pass yet** —
emulator confirmation is not hardware confirmation; the last hardware-verified release
predates all of this.

## Library (dual-screen browser)

- **Layout:** bottom screen shows a scrollable list, seven rows visible at a time; top
  screen shows a preview (artwork, synopsis, resolution/frame rate) for the highlighted
  entry.
- **Preview loading:** debounced by ~200 ms so scrolling quickly doesn't trigger a preview
  load on every row you pass over.
- **Preview artwork resolution order:** (1) reuse the already-displayed 176×100 image if
  it already matches this file, (2) check the 4-slot in-RAM LRU cache by path, (3) load
  a `.preview.cover` sidecar (176×100) if present, (4) load a `.cover` sidecar (88×50) if
  present and build a softened 176×100 in-memory display copy from it (box-filter
  averaging of 2×2 pixel blocks, not nearest-neighbor duplication — this copy is
  session-only RAM, never written back as a `.preview.cover` file), (5) decode the file's
  first successfully-decoded video frame as a last resort, (6) a generic placeholder
  card if nothing above produced artwork. `.preview.cover` itself is meant to be a
  separately authored sidecar — the box-filter upscale only exists as a fallback for
  files that only have the legacy 88×50 `.cover`. See
  [FILES_AND_SIDECARS.md](FILES_AND_SIDECARS.md#cover-poster-and-preview) for the exact
  order and formats.
- **In-RAM preview cache:** 4 slots, LRU eviction (least-recently-used, not
  first-in-first-out) — the cache tracks a tick counter and evicts the oldest-touched
  slot. Session-only; nothing here is written to the SD card.
- **Favorites / Recents:** both are lists of paths (Favorites capped at 128 entries,
  Recents capped at 16, most-recently-used order) that promote an entry to the top of
  the list and add a badge. There is **no separate "QUICK ACCESS" section label** —
  despite being a natural assumption, promoted entries stay in the same list, just
  reordered and badged.
- **Sorting:** numeric-aware natural sort — `episode_9` correctly sorts before
  `episode_10`, not after it, because digit runs are compared by length then value
  rather than character-by-character.
- **Format badge:** a plain text label, `"MIVF"` or `"MOFLEX"` — not an icon or color
  system. It's silently overridden by the FAVORITE, RESUME, or RECENT badge when more
  than one applies to the same entry (in that priority order). Treat this as a partial
  implementation of "distinguish MIVF from MoFlex at a glance," not a finished visual
  system.
- **Resume indicator:** a title with a saved bookmark gets a `RESUME` status label in
  the list itself; when it's the currently selected entry, the top-screen preview panel
  additionally shows a real pixel progress bar for its saved position — the progress bar
  lives in the preview, not under the list row.
- **Synopsis:** loaded from `.nfo`, capped at 19+19 characters (two lines) — long
  synopses are truncated, not wrapped further.
- **Metadata footer:** mostly cosmetic string-reformatting (e.g. the summary line format
  changed from `"%s %ux%u @ %u/%u"` to `"%s  %uX%u  FPS %u/%u"`). One real, honest gap:
  file size is computed internally (`file_size_kb`) but is **never actually rendered
  anywhere in the UI** — a calculated value with no display path yet, not a finished
  feature.
- **Show-all-directories toggle, Settings, Help:** unchanged, no regressions found.


![MIVF dual-screen Library showing selected-media metadata and preview on the top screen and a seven-row media list on the bottom screen.](assets/screenshots/library_phase8.png)

The capture above exercises the generic `MIVF MEDIA` fallback card, not a separately
authored `.preview.cover` sidecar; the validation status for sidecar loading remains
qualified accordingly.

## DVD-Style Menu

A movie with a `.menu.ini` sidecar opens into a DVD-style menu instead of playing
immediately. See [menu-authoring.md](menu-authoring.md) for how to author one.

### Root menu

- **Screen layout:** the buttons themselves render on the **top screen**, laid directly
  over the authored artwork/background — not on the bottom screen. The bottom screen
  shows a contextual "Disc Dashboard" panel (chapter count, format info, and a
  description of the currently highlighted button) instead of the button list itself.
- Recognizes exactly four actions: `play`, `resume`, `chapters`, `back`. Anything else
  in the sidecar is parsed but **silently disabled** (not a crash, not an error message —
  the button just doesn't do anything). There is no "Setup" action or page.
- D-pad moves between enabled buttons only (disabled buttons are skipped); A dispatches
  the selected action; B or START leaves the menu, back to the Library.
- Menu sound effects play on move/select/back.
- The menu remembers which button (and, if applicable, which Scene Selection page and
  chapter) was last highlighted **for that movie, for the current app session only** — a
  plain in-memory variable, not written to disk. Reopening the same movie's menu later in
  the same session restores your place; a fresh app launch does not.


![MIVF Cinematic Glass disc menu with Play Movie selected over custom artwork and a contextual dashboard on the bottom screen.](assets/screenshots/dvd_menu_root.png)

### Scene Selection

- A chapter-thumbnail grid: **2 columns × 3 visible rows = 6 thumbnails per page**
  (confirmed against source and against a real capture: 20 chapters paginate as "PAGE 1
  OF 4," which only checks out at 6/page — `ceil(20/6)=4`, not `ceil(20/12)=2`; the
  in-app text on the text-fallback path also literally reads "SIX SCENES PER PAGE"). A
  prior pass of this documentation said 2×6/12-per-page — that was wrong, caused by
  misreading `MIVF_MENU_CHAPTERS_VISIBLE_ROWS` (which actually bounds the *total slot
  count* per page, not the row count) as if it were a row count with a fixed 2 columns.
- Thumbnails come from a `.chapthumbs` sidecar (96×54 RGB565 each), generated once at
  authoring time from the *original* source video — the player reads a flat pixel array,
  it never decodes the compressed `.mivf` to produce a thumbnail. If the sidecar is
  missing, wrong-sized, or its chapter count doesn't match the `.chapters` sidecar
  loaded at menu time, it falls back to a plain text chapter list instead of guessing.
- D-pad **up/down only** moves one chapter at a time through the flat chapter list (no
  left/right — since the grid fills 2 chapters per row in order, stepping up/down
  visits both columns alternately); L/R page by a full 6-chapter page; A plays the
  selected chapter; B or START goes back to the **root menu**, not out of the DVD menu
  entirely.


![MIVF Scene Selection showing a two-column by three-row chapter-thumbnail grid with Chapter 3 selected and chapter context displayed on the top screen.](assets/screenshots/scene_selection.png)

### Return-to-menu after playback

If a movie was launched from its DVD menu, ending or exiting playback returns to that
same menu (with the standard fade), not the flat file browser — a disc menu owns its own
post-playback navigation. This was a real gap in an earlier design (playback launched
from a menu used to fall through to the file browser); it's fixed in the current working
tree, currently build-verified but not yet re-confirmed on hardware.

### Menu background & Ken Burns animation

- Background image: `.menu_bg.cover` sidecar (400×240 RGB565), or an embedded
  `menu_bg.cover` asset from a MIVF Asset Bundle if no valid sidecar is present — this is
  currently the *only* sidecar type the player will also read from an embedded bundle.
- The background pans/zooms continuously (Ken Burns-style). This is integer/fixed-point
  math throughout (Q16 fixed-point smoothstep easing) — there is no floating-point in
  this path. The current pass lengthened the pan/zoom cycles, reduced the maximum zoom
  percentage, and added smoothstep easing (with the pan and zoom axes phase-offset from
  each other) specifically to reduce visible whole-pixel stepping versus the previous
  version.
- Rendering is throttled: the background animation only advances once every 4 render
  loop iterations, not every frame.

### Cached-frame rendering

The menu keeps two full RGB565 framebuffer caches (top and bottom screen) and only
redraws when something actually changed — a fade in progress, a page or selection
change, or the next Ken Burns animation step. On a cache hit, the frame is produced with
a single memory copy instead of redrawing background, text, and blends from scratch.
This is a genuine dirty-rectangle-style cache, not just a description of intended
behavior.

### Idle screensaver

- Scoped to the **root menu view only** — Scene Selection keeps its own paging state and
  does not trigger the screensaver.
- Any key press dismisses it and resets the idle timer.
- Optional custom image via a `.screensaver.cover` sidecar (96×54 RGB565, exactly 10,368
  bytes) — sidecar-only, no MASB embedding support currently exists for this asset type.


![MIVF DVD-menu screensaver showing custom artwork over a dark theatre background with a wake hint on the bottom screen.](assets/screenshots/screensaver.png)

This one-off showcase capture confirms custom image loading and one rendered state. It
does not exercise the normal idle timeout, continuous bounce/collision behavior, or the
full input-dismissal path.

## Controls

See [CONTROLS.md](CONTROLS.md) for the full, context-by-context control reference. The
single most important thing to know: **B does not mean the same thing everywhere** —
quit the app (Library), leave the menu (DVD root), go back one level (Scene Selection),
or cycle the A/B loop marker (active playback). Treat each context's control table as
independent; don't assume B is a universal "back."
