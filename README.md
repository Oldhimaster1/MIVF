# MIVF Player for Nintendo 3DS

A homebrew video player for the Nintendo 3DS. MIVF is a custom, page‑based video container with its own software codecs, a native C player that runs on the console, and a standalone encoder that converts common video formats into the MIVF container.

This repository contains:
- `source/` — native 3DS player (C)
- `tools/` — native encoder helpers and verifiers
- `encode_mivf.py` — Python front-end encoder
- `meta/` — SMDH icon, banner and packaging assets
- `Makefile` — builds the 3DS payload and CIA

Documentation

Full documentation is available in the `docs/` folder. To preview locally:

```bash
pip install mkdocs mkdocs-material
mkdocs serve
```

Recommended stable encode example

```bat
encode_mivf.exe input.mkv output_balanced.mivf --m2y2 --no-deploy --fps 24 --audio-rate 48000 --jobs 6 --chunk-frames 240 --keep 8 --mv-range 2 --qp 36 --lambda 26
```

Releases

Release artifacts (prebuilt encoder EXEs, native tools, and 3DS payloads) should be distributed via GitHub Releases. Avoid committing large binaries into repository history.
