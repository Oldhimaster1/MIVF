# Scene Selection

**Status:** Build-verified; confirmed running correctly in the Azahar emulator, including
real chapter-launch navigation exercised via the Showcase sequence. Not yet part of a
documented, model-specific physical-hardware validation pass — see
[Project Status](../status.md).

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

![MIVF Scene Selection showing a two-column by three-row chapter-thumbnail grid with Chapter 3 selected and chapter context displayed on the top screen.](../assets/screenshots/scene_selection.png)

*Emulator capture (Azahar), used for interface inspection — not physical-hardware
performance evidence.*

See [Chapters](../authoring/chapters.md) for how to author `.chapters` and `.chapthumbs`.
