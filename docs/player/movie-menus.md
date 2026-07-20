# Movie Menus (DVD-Style Menu)

**Status:** Build-verified; the DVD-style root menu has been confirmed running correctly
in the Azahar emulator. Not yet part of a documented, model-specific physical-hardware
validation pass — see [Project Status](../status.md). The menu-return fix, the Ken Burns
animation's actual motion, and session-scoped cursor memory remain build-verified only,
not yet separately emulator-confirmed in isolation.

A movie with a `.menu.ini` sidecar opens into a DVD-style menu instead of playing
immediately. See [Menu Authoring](../authoring/menu-authoring.md) for how to author one.

## Root menu

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

![MIVF Cinematic Glass disc menu with Play Movie selected over custom artwork and a contextual dashboard on the bottom screen.](../assets/screenshots/dvd_menu_root.png)

*Emulator capture (Azahar), used for interface inspection — not physical-hardware
performance evidence.*

## Return-to-menu after playback

If a movie was launched from its DVD menu, ending or exiting playback returns to that
same menu (with the standard fade), not the flat file browser — a disc menu owns its own
post-playback navigation. This was a real gap in an earlier design (playback launched
from a menu used to fall through to the file browser); it's fixed in the current working
tree, build-verified but not yet separately hardware-confirmed.

## Menu background & Ken Burns animation

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

## Cached-frame rendering

The menu keeps two full RGB565 framebuffer caches (top and bottom screen) and only
redraws when something actually changed — a fade in progress, a page or selection
change, or the next Ken Burns animation step. On a cache hit, the frame is produced with
a single memory copy instead of redrawing background, text, and blends from scratch.
This is a genuine dirty-rectangle-style cache, not just a description of intended
behavior.

See also [Scene Selection](scene-selection.md) and [Screensaver](screensaver.md), which
are both reached from this menu.
