# Controls Reference

This page is also available in-app as a scrollable help screen — press **X** in the
Library, or open Settings during playback and select **CONTROLS**. The in-app screen is
the same content shown here.

**B does not mean the same thing everywhere.** Each context below is independent —
verify the table for the screen you're actually in rather than assuming a universal
"back" button.

## Library (file browser)

| Button | Action |
| :--- | :--- |
| D-Pad ↑/↓ | Move selection up/down |
| A | Open selected file (plays directly, or opens its DVD-style menu if it has one) |
| Y | Toggle favorite on selected file |
| SELECT | Toggle showing all folders (including SD root) |
| X | Open the in-app controls help screen |
| B | Exit the app |
| START | Exit the app (same as B here) |

Scrolling through files: preview thumbnails are debounced — the preview loads after you
stop on a file for ~200 ms. This keeps scrolling smooth on Old 3DS hardware.

## DVD-Style Menu — Root View

Opens automatically instead of direct playback when the selected movie has a
`.menu.ini` sidecar. See [Movie Menus](movie-menus.md) and
[Menu Authoring](../authoring/menu-authoring.md).

| Button | Action |
| :--- | :--- |
| D-Pad ↑/↓ | Move between enabled buttons (disabled/unrecognized-action buttons are skipped) |
| A | Activate the selected button (Play, Resume, Scene Selection, or Back) |
| B | Leave the menu, back to the Library |
| START | Same as B here |

Touch input is not used on this screen — D-pad + A/B is the supported input for the
root menu.

## DVD-Style Menu — Scene Selection

| Button | Action |
| :--- | :--- |
| D-Pad ↑/↓ | Move one chapter at a time through the chapter list (there is no D-pad left/right — moving up/down visits both grid columns in turn, since the grid is filled 2-per-row in chapter order) |
| L / R | Page by a full page (6 chapters: 2 columns × 3 rows) |
| A | Play the selected chapter |
| B | Back to the **root menu** (not out of the DVD menu entirely) |
| START | Same as B here |

## Resume / Start-Over Modal

Shown when starting a video that has a saved bookmark, outside of the DVD-style menu
(the DVD menu has its own separate Resume button instead — see above).

| Button | Action |
| :--- | :--- |
| D-Pad (any direction) | Switch the highlighted choice between RESUME and START OVER |
| A | Activate whichever choice is currently highlighted |
| B | **Start over immediately** — bypasses the highlighted choice entirely |
| START | Same as B here — **neither B nor START means "cancel"**, and both force start-over regardless of what's highlighted |

## Playback

| Button | Action |
| :--- | :--- |
| A | Play / pause |
| ← / → | Seek backward / forward (~5 seconds) |
| Touch + drag on timeline | Scrub to any position |
| X | Cycle playback speed (0.5×, 0.75×, 1.0×, 1.25×, 1.5×, 2.0×) |
| B | A/B scene loop: press once to set A, twice to set B (loops A→B), third time to clear |
| Y | Cycle subtitle track (off → track 1 → track 2 → …) |
| L + D-Pad ↑/↓ | Increase / decrease audio volume (persisted across sessions, 0–300%) |
| L + D-Pad → | Toggle forced stereo/upmix |
| L + D-Pad ← | Toggle audio limiter |
| R + D-Pad ↑/↓ | Increase / decrease screen brightness |
| R + D-Pad ←/→ | Jump to previous / next chapter |
| SELECT | Open settings overlay |
| START | Stop playback (returns to the movie's DVD menu if it was launched from one, otherwise to the Library) |

## Settings Menu

Open with SELECT during playback.

| Button | Action |
| :--- | :--- |
| D-Pad ↑/↓ | Move between settings |
| A / D-Pad ←/→ | Adjust selected setting |
| B / SELECT | Close settings and save |

Settings are saved when you close the menu, not on every value change. This prevents
SD-card stalls during rapid adjustment.

### Settings Items

- Resume bookmarks (on/off)
- Playback speed
- Auto-advance (play next file in folder)
- Sleep timer (off / 15 / 30 / 45 / 60 / 90 / 120 min)
- Aspect ratio (FIT / STRETCH / NATIVE)
- Screen brightness (1–5)
- Auto dim (on/off), dim timeout, dim brightness level
- Theme (opens the real Theme Picker — see [Personalization](../player/personalization.md))
- Font scale
- Force stereo 3D
- Subtitles (on/off), subtitle track, subtitle delay (±ms), subtitle position (low / middle / high)
- Chapters display (on/off)
- Favorites (on/off)
- Lid sleep allowed (on/off)
- Color Vision (opens the real Color Vision Picker — see [Personalization](../player/personalization.md))
- Debug overlay (on/off)
- Controls (opens the in-app help screen; press A on this row)

## In-App Controls Help Screen

Open with X in the Library, or via the CONTROLS row in Settings during playback.

| Button | Action |
| :--- | :--- |
| D-Pad ↑/↓ | Scroll |
| B / START / X | Close |

## Touch Controls

During playback, touch the bottom screen timeline area to scrub. The player shows a
time bubble while dragging. The DVD-style menu (root and Scene Selection) does not
currently support touch input — D-pad and buttons only.
