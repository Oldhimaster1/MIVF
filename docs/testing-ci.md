# Testing & CI

Local checks

- Basic Python compile check:

```bash
python -m py_compile encode_mivf.py
```

- Run Python unit tests (unittest/pytest):

```bash
python -m unittest discover -s tests -p "test_*.py"
# or if using pytest
pytest -q
```

Smoke test (example)

You can run a short smoke test with a ~1-minute test file (replace with your local test fixture):

```bat
encode_mivf.exe car_1min_fast.mkv car_1min_test.mivf --m2y2 --no-deploy --fps 24 --audio-rate 48000 --jobs 6 --chunk-frames 240 --keep 4 --mv-range 1 --qp 38 --lambda 34
```

- For a 1-minute (60s) 24 fps stream, expect frame counts near 60*24 = 1440 (implementation may produce 1441–1443 due to rounding/pts handling for some sources).

CI recommendations

- Run linters (ruff/flake8 or ruff), run `pytest`, and build native tools on a Linux runner.
- Optionally add a job that packages the Python frontend with PyInstaller and stores the artifact as a release or job artifact.
- Add a docs build job to ensure Markdown generation remains valid.
