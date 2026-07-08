# Credits

## Project

**MIVF Player for Nintendo 3DS**
Created and maintained by Micah Lagger.

A homebrew video player for the Nintendo 3DS with a custom page-based video
container, software codecs, streaming playback, seek-index infrastructure,
and a PC-side encoder.

https://github.com/Oldhimaster1/MIVF

## Built With

- [devkitPro](https://devkitpro.org/) / devkitARM + libctru — 3DS homebrew toolchain
- [makerom](https://github.com/3DSGuy/Project_CTR) — CIA packaging
- [bannertool](https://github.com/Epicpkmn11/bannertool) — CIA banner generation

## Third-Party Components

- **FFmpeg** (LGPL) — MoFlex/Mobiclip demuxer and decoder adapted from
  `libavformat/moflex.c` and `libavcodec/mobiclip.c`. Bundled support files
  under `source/moflex/ffmpeg_support/` retain their original LGPL licensing.

## Acknowledgements

Special thanks to the 3DS homebrew community, the FFmpeg project, and all
contributors who have tested, reported issues, or suggested improvements.
