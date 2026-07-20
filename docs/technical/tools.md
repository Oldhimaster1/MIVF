# Tools

This project uses several native tools and external binaries.

miv2y_moflex_tier

- Native encoder helper that compresses raw YUV frames into MIVF container pages. Called by the Python frontend during chunk encoding.

m2y2_transcode

- Native transcoder that range-codes M2Y1 video packets into M2Y2 for smaller size. It verifies output correctness against the input and exits nonzero on mismatches.

m2y2_verify

- A helper that validates M2Y2 content (verification tool).

FFmpeg

- FFmpeg is required on the PATH or placed next to the bundled encoder executable. The encoder uses ffmpeg to produce rawvideo and raw PCM audio for muxing.

Where to get binaries

- Prefer downloading prebuilt PC encoder binaries and helpers from GitHub Releases rather than including them in the repository history.

Building

- See `docs/building.md` for compile commands and packaging guidance.
