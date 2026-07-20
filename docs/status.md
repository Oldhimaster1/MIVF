# Project Status

MIVF is currently in development. The active player candidate has been build-verified,
while documented physical-hardware validation and release stabilization are still
underway. Tested hardware, known issues, setup requirements, and matching release
artifacts will be documented for each published preview.

## Physical-hardware progress

An initial physical-hardware run of Showcase Revision 3 was reported successful, with
strong observed performance on the tested console. Exact console, build, stage results,
and limitations are being documented before broader compatibility claims are made.

This is a **preliminary physical-hardware run reported**, not a **documented
physical-hardware validation** — see [Validation Status](compatibility/validation-method.md)
for the full status vocabulary. Concretely, still outstanding before this can be
upgraded to a documented validation:

- [ ] Exact console model
- [ ] Exact tested artifact and its SHA-256
- [ ] Whether the entire Showcase sequence completed
- [ ] Stage-by-stage results (Library, DVD menu, Scene Selection, Playback, Dashboard,
      Personalization, Resume, Screensaver, Title)
- [ ] Whether Resume worked
- [ ] Whether the real screensaver activated and woke correctly
- [ ] Whether Settings/Theme/Color Vision changes were correctly restored afterward
- [ ] Whether cancellation (B+START+SELECT) was tested
- [ ] Whether the run's log was preserved
- [ ] Any visual issues or other known limitations observed

Until these are recorded:

- No claim of universal 3DS compatibility.
- No claim of Old-3DS-family support (not yet confirmed which console family was tested).
- No claim of "lag-free," "stable," or "fully hardware-validated."
- The public compatibility matrix stays model-specific and evidence-gated — see
  [Hardware Matrix](compatibility/hardware-matrix.md).

## What is and isn't validated right now

| Layer | Status |
| :--- | :--- |
| Core playback foundation (NDSP audio clock, rational-FPS timing, decode, seek) | **Hardware-verified**, as of the last tagged release; unchanged by current work |
| MoFlex playback | **Hardware-verified**, as of the last tagged release |
| Library, DVD-style menu, Scene Selection, Personalization | **Build-verified, Emulator-tested (Azahar)**; one **preliminary physical-hardware run reported** (Showcase Revision 3, details above pending) |
| Screensaver | **Build-verified, partially emulator-tested** |
| Encoder motion-search experiments (`diamond`/`fast`/`hybrid`/`hierarchical`) | **Experimental** — a separate PC-side tooling track, not part of the player validation above |

Full per-feature breakdown: [Validation Status](compatibility/validation-method.md).
Release readiness checklist: [Release Process](development/release-process.md).
