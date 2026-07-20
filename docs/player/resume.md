# Resume

**Status:** Build-verified; confirmed running correctly in the Azahar emulator as part of
the Showcase Revision 3 sequence. Not yet part of a documented, model-specific
physical-hardware validation pass — see [Project Status](../status.md).

A title with a saved bookmark shows a `RESUME` status label in the Library list, and — for
the currently selected entry — the top-screen preview shows a pixel progress bar for its
saved position. Bookmarks are session-independent: they're read from a per-video bookmark
file under `appdata/`, not kept only in memory — see
[Files & Sidecars](../authoring/files-and-sidecars.md#app-data-layout).

On selecting a title with a bookmark, a Resume/Start-Over modal appears. In that modal,
**both B and START mean "start over"** — neither means "cancel" here. See
[Controls](controls.md) for the full context-by-context reference.
