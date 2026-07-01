# Releases

Use GitHub Releases to distribute compiled artifacts.

Recommended assets per release

- `mivf_player_3ds.3dsx` — 3DS Homebrew payload
- `mivf_player_3ds.cia` — Installable title
- `encode_mivf_windows_x64.exe` — Packaged encoder for Windows
- native tools (e.g., `m2y2_transcode.exe`, `miv2y_moflex_tier.exe`)

Recommendations

- Do not keep large binaries in the Git history. Attach them to Releases instead.
- Provide SHA256 checksums for every release asset and attach a short build note describing the environment and commands used to produce the artifact.
