# Building MIVF Toolkit

Building the desktop-side encoder and native helper tools. For building the 3DS player
itself, see [Building MIVF Player](building-player.md).

## Build the encoder EXE on Windows (script)

The repository includes a PowerShell helper to package the Python frontend as an EXE.

```powershell
powershell -ExecutionPolicy Bypass -File .\build_encode_mivf_exe.ps1
copy /Y dist\encode_mivf.exe .\encode_mivf.exe
```

## Manual PyInstaller build (Windows)

```powershell
pyinstaller --clean --onefile ^
  --add-binary "miv2y_moflex_tier.exe;." ^
  --add-binary "tools\m2y2_transcode.exe;tools" ^
  --add-binary "ffmpeg.exe;." ^
  encode_mivf.py
```

## Build native tools with gcc (examples)

```bash
gcc -O2 -o tools/m2y2_transcode.exe tools/m2y2_transcode.c
gcc -O2 -o tools/miv2y_moflex_tier tools/miv2y_moflex_tier.c
gcc -O2 -o tools/m2y2_verify tools/m2y2_verify.c
```

See [Tools](../technical/tools.md) for what each of these binaries does.

## Notes

- For reproducible builds, consider using a Docker image or a CI job that installs the
  required compilers and produces artifacts.
- Prefer publishing compiled artifacts in GitHub Releases rather than committing large
  binaries to the repository — see [Release Process](release-process.md).
