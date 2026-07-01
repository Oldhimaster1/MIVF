# Building

This page documents how to build the 3DS player, the encoder EXE, and native tools.

Build the 3DS player (.3dsx)

- Install devkitPro and the `3ds-dev` group (devkitARM + libctru).
- Ensure `DEVKITPRO` / `DEVKITARM` are set and toolchain is on PATH.
- From the repository root:

```bash
make
```

This produces `mivf_player_3ds.3dsx` with the embedded SMDH icon. To skip the icon: `make NO_SMDH=1`.

Build an installable .cia

- The `.cia` target requires two third-party tools: `makerom` and `bannertool`.
- Place both executables into `devkitPro/tools/bin` or on PATH, then:

```bash
make cia
```

Build the encoder EXE on Windows (script)

The repository includes a PowerShell helper to package the Python frontend as an EXE.

```powershell
powershell -ExecutionPolicy Bypass -File .\build_encode_mivf_exe.ps1
copy /Y dist\encode_mivf.exe .\encode_mivf.exe
```

Manual PyInstaller build (Windows)

```powershell
pyinstaller --clean --onefile ^
  --add-binary "miv2y_moflex_tier.exe;." ^
  --add-binary "tools\m2y2_transcode.exe;tools" ^
  --add-binary "ffmpeg.exe;." ^
  encode_mivf.py
```

Build native tools with gcc (examples)

```bash
gcc -O2 -o tools/m2y2_transcode.exe tools/m2y2_transcode.c
gcc -O2 -o tools/miv2y_moflex_tier tools/miv2y_moflex_tier.c
gcc -O2 -o tools/m2y2_verify tools/m2y2_verify.c
```

Notes

- For reproducible builds, consider using a Docker image or a CI job that installs the required compilers and produces artifacts.
- Prefer publishing compiled artifacts in GitHub Releases rather than committing large binaries to the repository.
