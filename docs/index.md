# MIVF

## Your movies, reimagined for Nintendo 3DS.

MIVF is an in-development homebrew player and creation toolkit that turns media you
provide into a personalized Nintendo 3DS movie experience — with a cinematic library,
custom artwork, Scene Selection, Resume, subtitles, and the personality of physical
media.

> **Current status:** The active player candidate has been build-verified, while
> documented physical-hardware validation and release stabilization are still underway.
> Tested hardware, known issues, setup requirements, and matching release artifacts will
> be documented for each published preview. Full detail: [Project Status](status.md).

[Watch the Showcase](showcase.md) · [Check Compatibility](compatibility/hardware-matrix.md) · [Getting Started](getting-started/installing.md)

## What's inside

<div class="grid" markdown>

- **Library** — a dual-screen browser: bottom-screen list, top-screen artwork and
  metadata preview, favorites, recents, resume badges. → [Library](player/library.md)
- **Movie Menus** — a DVD-style per-movie menu (Play / Resume / Scene Selection / Back)
  over animated custom artwork. → [Movie Menus](player/movie-menus.md)
- **Scene Selection** — a real chapter-thumbnail grid, generated at authoring time.
  → [Scene Selection](player/scene-selection.md)
- **Resume** — session-independent bookmarks with a pixel progress bar.
  → [Resume](player/resume.md)
- **Subtitles** — multi-track `.srt` support with adjustable delay and position.
  → [Subtitles](player/subtitles.md)
- **Personalization** — a real Theme Picker and Color Vision Picker, reached from
  Settings during playback. → [Personalization](player/personalization.md)

</div>

## Screenshot gallery

*Emulator captures (Azahar), used for interface inspection — not physical-hardware
performance evidence.* See each linked page above for the full-resolution capture in
context, or [Compatibility](compatibility/validation-method.md) for exactly what each one
does and doesn't confirm.

## Compatibility summary

| Layer | Status |
| :--- | :--- |
| Core playback foundation, MoFlex | **Hardware-verified** (last tagged release) |
| Library / DVD-menu / Scene Selection / Personalization | **Build-verified, Emulator-tested**; one preliminary physical-hardware run reported |
| Encoder motion-search experiments | **Experimental**, tracked separately from the player |

Full breakdown: [Validation Status](compatibility/validation-method.md). Model-specific
results: [Hardware Matrix](compatibility/hardware-matrix.md).

## Get started

- [Requirements & Installing](getting-started/installing.md)
- [Preparing your first `.mivf`](authoring/overview.md)
- [Troubleshooting](getting-started/troubleshooting.md)

## For developers

- [Architecture](technical/architecture.md)
- [Building MIVF Player](development/building-player.md)
- [Building MIVF Toolkit](development/building-toolkit.md)
- [Contributing](development/contributing.md)

## Legal

MIVF does not include movies or provide access to commercial media. Users are
responsible for supplying and using media, artwork, subtitles, and other assets in
accordance with applicable law and any relevant rights or permissions. MIVF is
unofficial homebrew software and is not affiliated with or endorsed by Nintendo. Full
detail: [Media and Legal](project/media-and-legal.md).
