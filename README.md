# MIVF

## Your movies, reimagined for Nintendo 3DS.

## What MIVF brings to Nintendo 3DS

MIVF is designed to make a movie feel **authored for the console**, not merely copied onto it. The Player and Toolkit work together so a source video can become a complete, personalized presentation.

### The viewing experience

- **Cinematic dual-screen Library** — Browse a personal collection with cover art, title details, favorites, recent-item tracking, and saved-position indicators across both screens.
- **DVD-style Movie Menus** — Open a title into an authored menu with Play, Resume, Scene Selection, and Back instead of dropping directly into a bare video player.
- **Scene Selection** — Navigate a paged chapter interface and launch a chosen scene directly, using chapter data prepared during authoring.
- **Reliable Resume** — Return to a meaningful saved position after leaving a movie, with visible Continue Watching and Resume states.
- **Readable subtitles** — Use selected subtitle editions with delay and positioning controls designed for the 3DS display.
- **Custom playback dashboard** — Combine a themed background, semantic colors, transport artwork, chapter context, and playback status for each project.
- **Personalization and accessibility** — Choose themes, transport styles, color-vision adjustments, readable contrast options, font scaling, and independent left/right audio attenuation.
- **Idle screensaver** — Let authored artwork take over during menu inactivity and return cleanly when input resumes.

### The creation experience

- **Guided stream selection** — Inspect detected video, audio, and subtitle streams and explicitly choose the intended edition instead of relying on file order.
- **Movie Information authoring** — Add title, release details, genre, director, synopsis, cast, edition notes, and project context in one place.
- **Artwork and theme tools** — Prepare covers, preview art, screensaver art, dashboard backgrounds, semantic colors, and focused/idle control artwork with runtime-aware validation.
- **Chapter Authoring Studio** — Build and review chapter structure for Scene Selection without hand-editing runtime files.
- **Dashboard Canvas** — Position transport controls with grid snapping, shared visual/touch geometry, undo/redo, reset actions, and validation.
- **Storage Planner** — Separate exact, measured, estimated, projected, and unknown storage values instead of presenting false precision.
- **Check Project and dry run** — Validate sources, assets, manifests, package plans, and output readiness before writing the runtime package.
- **Review and queue workflow** — Review the authoritative encoder command and stage persistent, resumable jobs for later execution.

### The technical foundation

- **Exact rational timing** — Preserve rates such as `24000/1001` with exact fraction math so long-form playback does not accumulate timing error from rounded frame rates.
- **Generation-safe audio clock** — Drive playback from sample-accurate audio progress while protecting buffer generations across seek and restart paths.
- **M2Y1 and M2Y2 video** — Use the project’s own video pipeline, including M2Y2 lossless recompression of an already encoded M2Y1 payload for smaller files without changing decoded output.
- **IA4M and PC16 audio** — Support compact ADPCM and PCM paths with exact packet sizing for validated frame-rate and sample-rate combinations.
- **Seek indexes and recovery** — Use indexed seeking, deterministic output, resumable encoder jobs, and validation-oriented tooling instead of treating conversion as a single opaque step.

> **Development status:** MIVF remains in development. Physical-hardware media in this README documents one capture session and must not be generalized to every Nintendo 3DS model or build. Azahar images document interface states only, not physical performance or compatibility.

<!-- MIVF_PUBLIC_MEDIA_START -->
## See the complete MIVF workflow

MIVF is currently in development. The photographs below document one physical-hardware capture session; the Azahar images document deterministic interface states only and are not physical-performance evidence.

[Setup guide](docs/getting-started/installing.md) · [Project status](docs/status.md) · [Known issues](docs/compatibility/known-issues.md) · [Hardware evidence](docs/HARDWARE_EVIDENCE.md)

### Physical-hardware experience

#### Physical-hardware hero

MIVF running on a physical Nintendo 3DS-family console.

<p align="center"><img src="docs/images/hardware/hardware_02_hero.jpg" alt="Physical-hardware hero" width="880"></p>

#### DVD-style Movie Menu

The authored Play, Resume, Scene Selection, and Back experience on physical hardware.

<p align="center"><img src="docs/images/hardware/hardware_04_movie_menu.jpg" alt="DVD-style Movie Menu" width="880"></p>

#### Scene Selection — Chapter 11

A real chapter-navigation state on physical hardware, with Chapter 11 selected.

<p align="center"><img src="docs/images/hardware/hardware_05_scene_selection_chapter11.jpg" alt="Scene Selection — Chapter 11" width="880"></p>

#### Chapter 11 playback

The selected chapter playing on physical hardware.

<p align="center"><img src="docs/images/hardware/hardware_06_playback_chapter11.jpg" alt="Chapter 11 playback" width="880"></p>

#### Custom playback dashboard

The finished authored dashboard and control artwork during Chapter 11 playback.

<p align="center"><img src="docs/images/hardware/hardware_07_custom_dashboard_chapter11.jpg" alt="Custom playback dashboard" width="880"></p>

### Motion on physical hardware

#### From Library to playback

A physical-hardware walkthrough from browsing into playback.

<p align="center"><img src="docs/media/hardware_clip_01_browser_to_player_chapter11.gif" alt="From Library to playback" width="800"></p>

#### Screensaver motion and wake

The custom screensaver in motion and the return to the player experience.

<p align="center"><img src="docs/media/hardware_clip_03_screensaver_activation_and_wake.gif" alt="Screensaver motion and wake" width="800"></p>

### MIVF Toolkit — from source to validated project

The desktop Toolkit guides stream selection, metadata authoring, artwork and theme creation, validation, review, and queueing.

#### Choose the source and output

Load source media, choose an output path, and keep the project workflow visible in one place.

<p align="center"><img src="docs/images/toolkit/toolkit_01_project_source_media_overview.png" alt="Choose the source and output" width="880"></p>

#### Select the video stream

Choose the intended video stream explicitly while retaining the legacy automatic option.

<p align="center"><img src="docs/images/toolkit/toolkit_02_project_video_stream_selection.png" alt="Select the video stream" width="880"></p>

#### Select the audio edition

Choose between the available Stereo AAC and Surround AC-3 tracks with language, codec, channel, and sample-rate metadata.

<p align="center"><img src="docs/images/toolkit/toolkit_03_project_audio_stream_selection.png" alt="Select the audio edition" width="880"></p>

#### Select text subtitles

Choose the detected English SubRip subtitle stream or intentionally author an edition without subtitles.

<p align="center"><img src="docs/images/toolkit/toolkit_04_project_subtitle_stream_selection.png" alt="Select text subtitles" width="880"></p>

#### Author movie information

Add title, release, genre, director, synopsis, cast, edition, and project-specific notes.

<p align="center"><img src="docs/images/toolkit/toolkit_10_movie_information_dialog.png" alt="Author movie information" width="880"></p>

#### Plan device storage honestly

Distinguish exact, measured, estimated, projected, and unknown storage values before creating the final file.

<p align="center"><img src="docs/images/toolkit/toolkit_11_device_storage_planner.png" alt="Plan device storage honestly" width="880"></p>

#### Build the presentation

Configure cover art, screensaver art, dashboard background, transport artwork, and semantic colors.

<p align="center"><img src="docs/images/toolkit/toolkit_12_artwork_and_theme_overview.png" alt="Build the presentation" width="880"></p>

#### Customize Rewind

Inspect the focused Rewind artwork, geometry, mask, adjustments, runtime size, and final one-bit mask.

<p align="center"><img src="docs/images/toolkit/toolkit_25_control_artwork_rewind_focused.png" alt="Customize Rewind" width="880"></p>

#### Customize Play/Pause

Inspect the focused Play/Pause artwork and inherited runtime recipe.

<p align="center"><img src="docs/images/toolkit/toolkit_26_control_artwork_play_pause_focused.png" alt="Customize Play/Pause" width="880"></p>

#### Customize Fast Forward

Inspect the focused Fast Forward artwork and inherited runtime recipe.

<p align="center"><img src="docs/images/toolkit/toolkit_27_control_artwork_fast_forward_focused.png" alt="Customize Fast Forward" width="880"></p>

#### Arrange the dashboard

Position controls on the Dashboard Canvas with grid snapping, shared visual/touch geometry, undo/redo, and validation.

<p align="center"><img src="docs/images/toolkit/toolkit_29_dashboard_canvas_selected_control.png" alt="Arrange the dashboard" width="880"></p>

#### Validate the project

Run a no-write validation pass and confirm the project is ready.

<p align="center"><img src="docs/images/toolkit/toolkit_13_check_project_dialog.png" alt="Validate the project" width="880"></p>

#### Review the authoritative command

Review selected streams, preset, generated encoder command, and ready state before creation.

<p align="center"><img src="docs/images/toolkit/toolkit_17_review_and_run_validation.png" alt="Review the authoritative command" width="880"></p>

#### Queue projects

Stage a persistent, resumable project job and review the queue controls before execution.

<p align="center"><img src="docs/images/toolkit/toolkit_18_queue_idle_pending.png" alt="Queue projects" width="880"></p>

### Azahar interface evidence

> **Emulator Capture — Azahar.** These images document user-interface and navigation states. They do not prove physical performance, SD-card behavior, lifecycle behavior, touch accuracy, persistence, or model compatibility.

#### Library

The dual-screen Library state used for deterministic interface documentation.

<p align="center">
  <img src="docs/images/azahar/azahar_library_top.png" alt="Library top screen in Azahar" width="48%">
  <img src="docs/images/azahar/azahar_library_bot.png" alt="Library bottom screen in Azahar" width="39%">
</p>

#### Scene Selection

The chapter-selection interface rendered in Azahar.

<p align="center">
  <img src="docs/images/azahar/azahar_scene_selection_top.png" alt="Scene Selection top screen in Azahar" width="48%">
  <img src="docs/images/azahar/azahar_scene_selection_bot.png" alt="Scene Selection bottom screen in Azahar" width="39%">
</p>

#### Playback

The playback state rendered in Azahar.

<p align="center">
  <img src="docs/images/azahar/azahar_playback_top.png" alt="Playback top screen in Azahar" width="48%">
  <img src="docs/images/azahar/azahar_playback_bot.png" alt="Playback bottom screen in Azahar" width="39%">
</p>

#### Accessible dashboard style

A representative alternate dashboard presentation rendered in Azahar.

<p align="center">
  <img src="docs/images/azahar/azahar_dashboard_accessible_top.png" alt="Accessible dashboard style top screen in Azahar" width="48%">
  <img src="docs/images/azahar/azahar_dashboard_accessible_bot.png" alt="Accessible dashboard style bottom screen in Azahar" width="39%">
</p>

#### Screensaver

The screensaver state rendered in Azahar.

<p align="center">
  <img src="docs/images/azahar/azahar_screensaver_top.png" alt="Screensaver top screen in Azahar" width="48%">
  <img src="docs/images/azahar/azahar_screensaver_bot.png" alt="Screensaver bottom screen in Azahar" width="39%">
</p>

### Technical results

The README keeps technical charts below the product story. Results are source- and machine-specific; see the benchmark methodology and audited CSV files for scope and limitations.

#### Motion-search time versus output size

<p align="center"><img src="docs/assets/benchmarks/motion_search_time_vs_size.png" alt="Motion-search encode time versus output size" width="760"></p>

#### M2Y1 versus M2Y2 quality and size

<p align="center"><img src="docs/assets/benchmarks/m2y1_m2y2_quality_size.png" alt="M2Y1 versus M2Y2 quality and size" width="760"></p>

Full methodology: [Benchmark methodology](docs/technical/benchmark-methodology.md) · [Performance tuning](docs/technical/performance-tuning.md)

<!-- MIVF_PUBLIC_MEDIA_END -->


MIVF is an in-development homebrew player and creation toolkit that turns media you
provide into a personalized Nintendo 3DS movie experience — with a cinematic library,
custom artwork, Scene Selection, Resume, subtitles, and the personality of physical
media.

> **Current status:** The active player candidate has been build-verified, while
> documented physical-hardware validation and release stabilization are still underway.
> Tested hardware, known issues, setup requirements, and matching release artifacts will
> be documented for each published preview.

[Watch the Showcase](docs/showcase.md) · [Check Compatibility](docs/compatibility/hardware-matrix.md) · [Read the Setup Guide](docs/getting-started/installing.md)

**Recently added**: watch-state tracking with Continue Watching/Recently Added, Library
sort/filter/search, Touch Lock, screensaver customization, and a much larger desktop
Toolkit (guided project creation, an encode queue, a theme wizard, chapter authoring, a
theme browser). All build-verified and automated-test-verified; none of it is
physical-hardware-verified yet — see [Changelog](docs/project/changelog.md) for the full
list and [Known Issues](docs/compatibility/known-issues.md) for exact scope limits.

**MIVF Player** (the 3DS application) · **MIVF Toolkit** (the desktop encoder/tools) ·
**`.mivf`** (the media container format) — see [Terminology](#terminology) below.


## Compatibility summary

| Layer | Status |
| :--- | :--- |
| Core playback foundation (audio clock, timing, decode, seek), MoFlex | **Hardware-verified**, last tagged release |
| Library, DVD-style menu, Scene Selection, Personalization | **Build-verified, Emulator-tested**; one **preliminary physical-hardware run reported**, not yet a documented model-specific validation |
| Encoder motion-search experiments (`diamond`/`fast`/`hybrid`/`hierarchical`) | **Experimental**, tracked separately from the player |

No claim of universal 3DS compatibility or Old-3DS-family support is made until that's
specifically confirmed. Full detail: [Project Status](docs/status.md) ·
[Hardware Matrix](docs/compatibility/hardware-matrix.md) ·
[Known Issues](docs/compatibility/known-issues.md).

## Quick Start

1. Download `mivf_player_3ds.3dsx` (Homebrew Launcher) or `mivf_player_3ds.cia`
   (HOME menu, via a title manager like FBI) from the [Releases page](https://github.com/Oldhimaster1/MIVF/releases).
2. Place `.mivf` files on your SD card in `sdmc:/mivf/` (also scans
   `sdmc:/3ds/mivf_player_3ds/` and the SD root).
3. Encode your own video:

   ```bash
   python encode_mivf.py input.mkv output.mivf --m2y2
   ```

   Requires Python 3 and `ffmpeg` on `PATH`.

Full guide: [Getting Started](docs/getting-started/installing.md). Full encoding guide,
CLI reference, sidecar formats, and authoring workflow: [MkDocs site](docs/index.md).

## Known Issues (summary)

- Documented, model-specific physical-hardware validation of the Library/DVD-menu/
  Personalization work is not yet complete — see [Project Status](docs/status.md).
- Very high-quality M2Y2 encodes (near-lossless QP, exhaustive `full` motion search)
  have been observed to decode slower than real time during dense content, producing
  growing audio/video drift over extended playback. `hybrid` motion search is the
  current mitigation under evaluation.
- The DVD-style menu only recognizes four actions (`play`/`resume`/`chapters`/`back`) —
  no Setup, Movie Info, or Bonus Features page yet.

Full list: [Known Issues](docs/compatibility/known-issues.md).

## Legal

MIVF does not include movies or provide access to commercial media. Users are
responsible for supplying and using media, artwork, subtitles, and other assets in
accordance with applicable law and any relevant rights or permissions. MIVF is
unofficial homebrew software and is not affiliated with or endorsed by Nintendo. Full
detail: [Media and Legal](docs/project/media-and-legal.md).

## Terminology

- **MIVF** — the overall project and experience
- **MIVF Player** — the Nintendo 3DS application
- **MIVF Toolkit** — the desktop encoding/authoring tools
- **`.mivf` container** — the media format itself
- **MIVF project** — a user-created movie presentation (video + its sidecars)

## Documentation

This README is a front door. Full documentation — encoding guide, CLI reference,
sidecar/asset specification, architecture, benchmarks, controls, and more — lives in the
[MkDocs site](docs/index.md) (`docs/` folder; run `mkdocs serve` locally to browse it).

## License

MIVF Player is released under the **MIT License** — see [LICENSE](LICENSE). MIT's actual
requirement is narrow: the copyright notice and license text must be preserved in copies
or substantial portions. [NOTICE.md](NOTICE.md) additionally *requests* courtesy
attribution when substantial portions are reused elsewhere; see
[License docs](docs/project/license.md) for the distinction between that request and
MIT's actual mandatory terms. Third-party components (FFmpeg-derived MoFlex
decoder, LGPL) retain their own licensing — see [CREDITS.md](CREDITS.md) and
[Third-Party Notices](docs/project/third-party-notices.md).

---

![License](https://img.shields.io/github/license/Oldhimaster1/MIVF?color=blue&style=flat-square)
![GitHub commit activity](https://img.shields.io/github/commit-activity/m/Oldhimaster1/MIVF?color=darkgreen&style=flat-square)