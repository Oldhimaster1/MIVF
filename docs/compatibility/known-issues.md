# Known Issues

- The Library, DVD-style root menu, Scene Selection, and Personalization flow have been
  confirmed running correctly in the Azahar emulator, but **documented, model-specific
  physical-hardware validation is not yet complete** — see [Project Status](../status.md).
  One preliminary physical-hardware Showcase run was reported successful; treat that as
  encouraging, not as a finished regression pass.
- Very high-quality M2Y2 encodes (near-lossless QP, exhaustive `full` motion search) have
  been observed, via direct measurement, to decode slower than real time during
  dense/motion-heavy content, producing growing audio/video drift over extended
  playback. The initial seek/sync point itself was measured frame-exact; the drift
  builds progressively afterward, consistent with a decode-throughput ceiling rather
  than a sync bug. `hybrid` motion search is the current mitigation under evaluation —
  see [Performance Tuning](../technical/performance-tuning.md).
- The MIVF-vs-MoFlex badge in the Library is a plain text label, not an icon/color
  system, and shares one status-tag slot per row with Favorite/Resume/Recent/Continue
  Watching/Recently Added/Watched/In Progress/series episode ("SxxEyy")/duplicate-episode
  tags — only the highest-priority one shows when more than one applies to the same file.
  Priority order (highest first): Continue Watching, Favorite, Resume (selected row only),
  Recent, Recently Added, Watched, In Progress, duplicate-episode warning, episode number.
  Build-verified; not physical-hardware-verified.
- Computed file size (`file_size_kb`) in the browser preview metadata is never actually
  rendered anywhere in the UI — it's calculated but currently invisible to the user.
- Only `menu_bg.cover` can be embedded in a `.mivf` as a MIVF Asset Bundle; chapter
  thumbnails and the screensaver image are sidecar-only.
- The DVD-style menu only recognizes four actions (`play`/`resume`/`chapters`/`back`) —
  there is no Setup, Movie Info, or Bonus Features page yet.
- E0's resume/refuse-on-mismatch logic is implemented and code-reviewed but has not been
  exercised end-to-end this cycle (i.e., deliberately killing and resuming a job).
- `diamond`/`fast`/`hybrid`/`hierarchical` motion-search modes and `--warm-start-chunks`
  are validated on a small number of local test clips on one machine — not a guarantee
  for every source. These are encoder-side experiments, tracked separately from the
  player candidate — see [Project Status](../status.md).
- The repository's one automated test that calls a `read_mivf_first_page_offset`
  function currently fails, because that function doesn't exist in `encode_mivf.py` —
  a known, pre-existing gap in the test suite, not something newly introduced.
- Series/Season/Episode grouping (Library sort mode) recognizes only the `SxxEyy`
  filename convention, matched directly against the filename with no folder traversal —
  the Library scanner is intentionally flat/non-recursive, so `Show/Season 1/`-style
  folder structure is not read. The alternate `NxM` notation is deliberately not
  recognized (it collides with resolution tags like `1920x1080`). An unrecognized
  filename is left ungrouped, never guessed.
- The Toolkit's persistent multi-project encode queue, Project Home, Chapter Authoring
  Studio, and Local Theme Browser are new, build-verified and automated-test-verified
  (pytest + offscreen smoke), but have not been used on a large real-world library.
- The Toolkit GUI (`mivf-gui/`) is not yet part of the documentation site's navigation —
  its workflow is covered by its own in-repo test suite and docstrings, not a dedicated
  guide page yet.
- `pytest` is installed in the Toolkit's own virtual environment (`mivf-gui/.venv`) and is
  the test runner actually used for `mivf-gui/tests/`; the top-level `tests/` directory
  (covering `encode_mivf.py` itself) also runs under it. `python -m unittest discover -s
  tests` remains a working fallback if `pytest` isn't installed in a given environment.

See [Reporting Results](reporting-results.md) to add a hardware test result of your own.
