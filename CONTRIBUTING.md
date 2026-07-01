# Contributing

Thanks for helping improve MIVF. Keep contributions small and focused — include tests for behavioral changes when possible.

Requirements

- Python 3.8+ (used by encode_mivf.py and tests)
- FFmpeg on PATH (or place ffmpeg.exe next to the encoder)
- GCC/Clang (or MSVC) for native tools
- Optional: devkitPro + devkitARM and libctru to build the 3DS player

Run Python tests

1. Create and activate a virtual environment:
   - Linux/macOS: python -m venv .venv && source .venv/bin/activate
   - Windows (PowerShell): python -m venv .venv; .\.venv\Scripts\Activate.ps1
2. Install test runner and run tests:
   pip install -U pip pytest
   pytest -q tests/

Build the encoder EXE on Windows

1. Install Python 3.8+ and ensure FFmpeg is available.
2. Install PyInstaller: pip install pyinstaller
3. From the repo root, build a single-file EXE:
   pyinstaller --onefile --name encode_mivf encode_mivf.py
4. The EXE will be in dist/encode_mivf.exe. Test with: dist/encode_mivf.exe --help

Build native tools

- Linux / macOS (gcc/clang):
  gcc -O2 -o tools/m2y2_transcode.exe tools/m2y2_transcode.c
  gcc -O2 -o tools/miv2y_moflex_tier tools/miv2y_moflex_tier.c
  gcc -O2 -o tools/m2y2_verify tools/m2y2_verify.c

- Windows (MSYS2/MinGW or Visual Studio): use equivalent build commands or an MSVC project.

- 3DS player: install devkitPro/devkitARM + libctru and run:
  make

Do NOT commit

- Movie files (e.g. .mp4, .mkv) or other large media
- Generated .mivf outputs or encoded video files
- build/, dist/, __pycache__, or other build artifacts
- Logs, temporary working directories, and editor swap/backup files
- Temporary patch files (*.patch, *.rej, *.orig)

If you need to share binaries for testing, upload them to GitHub Releases rather than committing them to the repository.

Releases

- Publish prebuilt binaries (EXEs, native tools, .3dsx/.cia) as GitHub Release assets.
- Include SHA256 checksums (and signatures when possible) alongside release artifacts and a short note describing how they were built.

Thanks — file an issue or PR if you need help reproducing builds or adding CI.
