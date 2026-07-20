# Release Process

## Distribution Assets

Use GitHub Releases to distribute compiled artifacts. Do not keep large binaries in the
Git history — attach them to Releases instead, with a SHA-256 checksum and a short build
note describing the environment and commands used to produce each artifact.

Recommended assets per release:

- `mivf_player_3ds.3dsx` — 3DS Homebrew payload
- `mivf_player_3ds.cia` — Installable title
- `encode_mivf_windows_x64.exe` — Packaged encoder for Windows
- Native tools (e.g., `m2y2_transcode.exe`, `miv2y_moflex_tier.exe`)

## Release Checklist

### Before Release

- [ ] `make clean && make` succeeds with no new warnings
- [ ] `make cia` succeeds (if distributing `.cia`)
- [ ] Test on real New 3DS hardware:
  - [ ] Browser opens and scrolls smoothly
  - [ ] `.mivf` file plays with audio
  - [ ] `.moflex` file plays (if applicable)
  - [ ] Seek works (if index present)
  - [ ] Large file without index opens without freeze
  - [ ] Settings open, change, close, and persist
  - [ ] Subtitles display correctly
  - [ ] Resume bookmark works
  - [ ] Auto-advance works
  - [ ] Sleep/wake works
  - [ ] Return to browser works
- [ ] Test on Old 3DS hardware (if possible):
  - [ ] Playback with `--profile 3ds-fast` encoded file
  - [ ] Browser scrolling is responsive
- [ ] Test on Azahar emulator:
  - [ ] Basic playback smoke test
- [ ] Review documentation for stale claims, including hardware-status wording:
  - [ ] `README.md`
  - [ ] `docs/status.md`
  - [ ] `docs/compatibility/*.md`
- [ ] Do not use "hardware-tested preview" as a release classification until the
      release candidate itself — not merely a Showcase build — has completed and
      documented the required physical-hardware validation. See
      [Project Status](../status.md).
- [ ] Update version string if present in source or metadata

### Packaging

- [ ] Copy `mivf_player_3ds.3dsx` to release artifacts
- [ ] Copy `mivf_player_3ds.cia` to release artifacts (if built)
- [ ] Optionally bundle `encode_mivf.py` and encoder binaries for PC
  - [ ] **Rebuild `miv2y_moflex_tier.exe` from the current `tools/miv2y_moflex_tier.c`**
        before bundling — it is a local/generated artifact, not tracked in source
        control, so a stale bundled binary can silently lag behind
        `encode_mivf.py`'s CLI (e.g. accepting a `--motion-search` mode the bundled
        helper doesn't actually implement)

### GitHub Release

1. Tag the commit: `git tag vX.Y.Z`
2. Push the tag: `git push origin vX.Y.Z`
3. Create a release on GitHub with:
   - Version number
   - Summary of changes since last release
   - Attach `.3dsx` and `.cia` binaries
   - Link to updated documentation

### Post-Release

- [ ] Verify the release downloads work
- [ ] Update any external links or forum posts
- [ ] Merge release branch to main if using a release branch workflow
