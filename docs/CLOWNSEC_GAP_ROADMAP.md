# Clownsec Gap Roadmap

This document compares the current MIVF workspace against the Clownsec-style feature set described in the pasted notes. It is intentionally conservative: it focuses on product gaps, implementation order, and risk, not on copying any external code.

## Executive Summary

MIVF and Clownsec are solving related but different problems.

- **MIVF** is a focused Nintendo 3DS video player + encoder with a strong native playback core, page-based `.mivf` container, seek-index support, sidecars, and a browser/settings UI.
- **Clownsec** is a broader media-suite application: player + catalog browser + downloader + upload server + file manager + metadata/artwork workflow + CIA-embedded MoFlex support.

The largest gaps are **not** playback codecs. MIVF already has core playback, browser, settings, subtitles, chapters, resume, favorites, recents, and seek-index infrastructure. The gaps are mainly in **library UX**, **metadata/artwork management**, **network/media-suite features**, and **CIA-embedded MoFlex support**.

The safest next feature is a **browser-only hidden system folder toggle**. It is visual/UI-only, low risk, and does not require touching audio, NDSP, stream I/O, or playback internals.

## Scoring Rubric

Each feature below is ranked using:

- **User impact**: how much the feature changes day-to-day use
- **Implementation difficulty**: low / medium / high
- **Playback risk**: low / medium / high
- **Isolation potential**: whether it can be added without touching audio/NDSP/MoFlex playback
- **CIA dependency**: whether it should come before or after any CIA MoFlex support work

## Full Gap Matrix

### 1. Browser / Library UX

| Feature | Clownsec behavior | Current MIVF status | Gap | Risk | Difficulty | Likely files | Must-not-touch systems | Suggested first safe slice | Test checklist |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Hidden system folder toggle | Hides 3DS/DCIM/Nintendo 3DS folders by default, reveal with Y | No equivalent browser hide/reveal control. MIVF scans a few fixed folders via `hfix58_scan_default_dirs()` in `source/main.c` | Missing | Low | Low | `source/main.c`, `docs/CONTROLS.md`, `docs/INSTALLING.md` | Add a browser filter toggle that only changes list visibility | Browser opens, toggle works, hidden folders reveal on demand, no file opens regressively |
| Better local metadata panel | Poster, year, runtime, genres, size, description, 2D/3D badge | MIVF browser preview shows thumbnail/poster, `.nfo` synopsis, and MoFlex badge, but not a catalog-style metadata panel | Partial | Low | Medium | `source/main.c`, `docs/FILES_AND_SIDECARS.md`, `docs/ARCHITECTURE.md` | No playback core changes | Add one extra info field (for example file size or duration) to existing preview | Browser preview still loads, text fits, no redraw regressions |
| Poster image cache | Cache poster art from metadata folder | MIVF supports `.cover` raw RGB565 posters and auto-decodes thumbnails, but not a general artwork cache | Partial | Low | Medium | `source/main.c`, `source/mivf_settings.c` | Do not touch decoder/playback | Add cache keying/documentation for existing `.cover`/thumbnail behavior | Preview still loads, cached poster reused, no SD-card stalls |
| Movie info panel | Top-screen info panel on highlight | MIVF preview already has title/summary/detail/extra text in the browser preview area | Partial | Low | Low | `source/main.c` | No audio/NDSP/playback changes | Polish existing preview fields, do not add new loading paths | Browser highlight updates text correctly, preview remains responsive |
| File manager basics | Move/delete/create folders, movies-only toggle | Not present | Missing | High | High | Likely new app modules, `source/main.c` UI, possibly `source/mivf_settings.c` | Avoid stream/codec/playback changes | Do not start here; write a design note only | If ever implemented, must preserve browser/playback behavior and SD safety |

### 2. Metadata / Artwork

| Feature | Clownsec behavior | Current MIVF status | Gap | Risk | Difficulty | Likely files | Must-not-touch systems | Suggested first safe slice | Test checklist |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Central `moviedata` folder | Metadata keyed by filename in a hidden SD folder | MIVF uses sidecars next to the `.mivf` plus appdata for persistent state; see `source/mivf_settings.h` and [docs/FILES_AND_SIDECARS.md](FILES_AND_SIDECARS.md) | Missing as a centralized library model | Medium | Medium | `source/main.c`, `source/mivf_settings.h/c`, `docs/FILES_AND_SIDECARS.md` | Do not change seek/index/player core | Design-only doc first, then optional lookup from new central folder | Metadata lookup must never block browser scrolling |
| `.nfo` import/export | Hand-authored or fetched metadata files | MIVF already supports `.nfo` sidecars for browser synopsis | Partial | Low | Low | `source/main.c`, `docs/FILES_AND_SIDECARS.md` | No playback changes | Add optional import path later if needed | Existing `.nfo` still displays, no regressions |
| JPG/PNG poster decode/cache | Any-size poster decode, scaled and cached | MIVF currently uses raw RGB565 `.cover` posters; auto thumbnails are used otherwise | Missing for JPG/PNG | Medium | Medium | `source/main.c`, maybe `tools/` if helpers are needed | Do not touch video decode or audio | Feasibility report or prototype only | Poster decode should not stall browser list rendering |
| Filename matching | Tolerant title/year matching for local metadata | MIVF uses direct filename/sidecar matching, not a title-matching catalog search | Missing | Medium | Medium | `source/main.c`, `source/mivf_settings.c` | Do not change playback code | Start with a local-only normalization helper if needed | Matching must be deterministic and non-destructive |

### 3. Network / Media Suite

| Feature | Clownsec behavior | Current MIVF status | Gap | Risk | Difficulty | Likely files | Must-not-touch systems | Suggested first safe slice | Test checklist |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Web upload server | Send files to console from browser over Wi-Fi | Not present | Missing | High | High | New network modules, `Makefile`, `source/main.c` UI | Avoid playback and stream logic | Do not start until a separate experimental branch exists | Upload should not interfere with normal browser or playback |
| Downloader | Fetch movies/seasons/direct URLs | Not present | Missing | High | High | New network modules, archive extraction helpers, UI | Do not touch encoder/playback | Report-only design first | Must preserve SD safety and file integrity |
| Catalog browser | Browse remote catalog JSON with posters/details | Not present | Missing | High | High | New app modules, metadata cache, UI | Avoid MoFlex/playback changes | Design-only first | Catalog browsing must not break local browser |
| Direct URL add | Paste URL, add to queue/download | Not present | Missing | High | Medium | New network module, UI flow | Do not touch decoder or encoder | Not a first feature | Must validate URL safety and destination handling |

### 4. MoFlex-Specific Gaps

| Feature | Clownsec behavior | Current MIVF status | Gap | Risk | Difficulty | Likely files | Must-not-touch systems | Suggested first safe slice | Test checklist |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| CIA embedded MoFlex | Plays unencrypted `.moflex` inside `.cia` in place | Not integrated in current MIVF dispatch path; see [docs/MOFLEX_FOR_MIVF_REVIEW.md](MOFLEX_FOR_MIVF_REVIEW.md) | Missing | High | High | New helper code, `source/main.c` dispatch, maybe demux APIs | Do not touch default playback path yet | CIA helper design only, non-called helper first | Must not break current `.moflex` playback |
| Multi-movie picker | Lists embedded movies with real titles | Not present | Missing | High | High | CIA helper code, browser/picker UI | Avoid playback path replacement | Design only | Picker must preserve file open behavior |
| 2D/3D auto-detect | Detects frame-interleaved 3D vs flat 2D per file | MIVF MoFlex playback does not advertise a separate CIA-style auto-detect workflow in current docs | Partial / unclear | Medium | Medium | `source/moflex/`, `source/main.c` | Do not alter audio ownership or thread model | Audit only; do not add until proven necessary | 2D and 3D files should still play correctly |
| Audio-master pacing review | Audio clocks playback; emulator audio caveats documented | MIVF already has synced playback and NDSP usage, but not this exact emulator-centric workflow | Partial | Medium | Medium | `source/moflex/playback/moflex_playback.c`, docs only | No NDSP changes unless explicit | Document-only unless proven issue arises | Existing audio sync must not regress |
| PC verification harness | PC-side bit-exact verification | Not present as a documented end-user feature in MIVF docs | Missing | Low | Medium | `tools/`, docs only | No runtime code changes | Documentation or offline verification note only | Must not affect player binaries |

### 5. Playback UX

| Feature | Clownsec behavior | Current MIVF status | Gap | Risk | Difficulty | Likely files | Must-not-touch systems | Suggested first safe slice | Test checklist |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Software volume boost | Up to 400% volume | MIVF has playback volume controls, but not a documented boost workflow | Partial / missing | Medium | Medium | `source/main.c`, `source/mivf_settings.c` | No audio startup changes | Documented workflow or small UI extension only | Volume control must remain safe and non-clipping where possible |
| Enhanced hold-to-scrub | Hold to scrub with responsive landing | MIVF already has touch scrubbing and timeline behavior, but the exact hold-to-scrub semantics are not the same | Partial | Low | Medium | `source/main.c` | Do not touch seek index logic | Polish existing scrub UX only | Scrub should land accurately and redraw smoothly |
| Immediate seek landing frame | Show landing frame immediately after seek | MIVF already has seek UI and embedded/sidecar index support; the behavior is broadly present | Partial | Low | Low | `source/main.c`, seek docs only | Avoid `mivf_stream.c` | Documentation/polish only | Seek should continue working with current indexes |
| Better unsupported-file messaging | Clear messaging for unsupported media | MIVF has alerts and browser feedback, but not a Clownsec-style media-suite response layer | Partial | Low | Low | `source/main.c` | No playback core changes | UI text improvement only | Error messages should not block playback |

## Recommended Priority Order

Ranked by a mix of user impact, implementation difficulty, and playback risk.

### Tier 1: Low-risk, high-value app UX

1. Hidden system folder toggle
2. Better local metadata panel
3. Movie info panel polish
4. Better unsupported-file messaging
5. Enhanced hold-to-scrub polish

Why these first:
- They are browser/UI-only or close to it.
- They do not require audio, NDSP, stream I/O, encoder, or MoFlex playback changes.
- They improve perceived polish immediately.

### Tier 2: Metadata/artwork improvements

6. Central `moviedata` folder design
7. `.nfo` import/export flow
8. JPG/PNG poster cache feasibility
9. Filename matching helper

Why these next:
- They add library polish without becoming a network stack.
- They can be staged around existing sidecars and preview code.
- They still avoid touching playback internals.

### Tier 3: MoFlex-specific experiments

10. CIA helper design only
11. CIA-embedded MoFlex parser helper (non-called)
12. Windowed demux API design
13. Multi-movie picker UI
14. Optional CIA MoFlex playback path

Why these later:
- They directly touch the riskiest area: container parsing and playback dispatch.
- They should be isolated from the default `.moflex` path until proven safe.

### Tier 4: Network/media-suite features

15. Wi-Fi upload server
16. Direct URL downloader
17. Catalog parser
18. Catalog browser
19. Catalog metadata fetch

Why these last:
- They are the highest scope, the most code, and the easiest to destabilize.
- They are useful, but they are not the right first expansion step for this repo.

## First 5 Tiny Implementation Slices

If you want a gradual roadmap that stays low-risk, I would do these in order:

1. **Hidden system folder toggle** in the browser
   - Browser-only filter; no file format or playback changes.

2. **Better info panel polish**
   - Add one small field to the existing preview area (for example file size or codec summary).

3. **Central metadata folder design note**
   - Add docs and a non-invasive lookup plan before any code.

4. **Local `.nfo` folder fallback**
   - Optional lookup path that complements existing sidecars.

5. **JPG/PNG poster feasibility report**
   - Evaluate whether the current browser preview path can accept decoded images without hurting scrolling.

## Features to Defer

These should be deferred until the simpler UX/metadata work is done:

- Web upload server
- Downloader
- Catalog browser
- Direct URL import
- Full file manager
- Multi-movie CIA picker
- CIA-embedded MoFlex playback path
- Any NDSP/audio redesign
- Any threading/GPU playback rewrite

## Features That Belong on a Separate Experimental Branch

These should not land in the main line until isolated tests exist:

- CIA-embedded MoFlex helper integration
- Any demux API expansion that changes how playback opens files
- Any MoFlex container parser that depends on new file-window semantics
- Any network/service subsystem (upload server, downloader)
- Any large file-manager subsystem

## Suggested First Safe Slice

**Hidden system folder toggle in the browser**

Why this is the best first real feature:
- It is visible and useful immediately.
- It stays entirely in the browser/UI layer.
- It does not require audio, decode, stream, or encoder changes.
- It is easy to test and easy to revert.

## Test Checklist for the First Safe Slice

- Browser still opens normally.
- Hidden folders remain hidden by default.
- Reveal toggle works and persists as expected.
- File selection still opens the right media file.
- No changes to playback, audio, or seek behavior.
- No regressions in preview loading or scrolling.

## Evidence Summary from Current MIVF

Current workspace evidence supporting the “MIVF = player + encoder + seek/index infrastructure” framing:

- Browser and playback controls exist in `source/main.c`.
- Persistent app data paths are defined in `source/mivf_settings.h`.
- Sidecar and seek-index behavior are already documented in [docs/SEEK_INDEX.md](SEEK_INDEX.md) and [docs/FILES_AND_SIDECARS.md](FILES_AND_SIDECARS.md).
- Built-in MoFlex playback is enabled by default in [docs/MOFLEX_STATUS.md](MOFLEX_STATUS.md).
- The `moflex_for_mivf` bundle was explicitly reviewed and rejected for wholesale integration in [docs/MOFLEX_FOR_MIVF_REVIEW.md](MOFLEX_FOR_MIVF_REVIEW.md).

## Final Recommendation

Do not try to make MIVF into a full media-suite clone in one go.

Build it as a series of small, safe expansions:

1. Browser UX improvements
2. Metadata/artwork improvements
3. CIA helper experiments
4. MoFlex experiments in isolation
5. Network/media-suite features only after the base experience is stable
