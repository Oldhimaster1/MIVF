# Changelog

## Unreleased

- Added an in-app controls/keybinds help screen (press X in the browser, or select CONTROLS in Settings during playback).
- Documented the stable streaming chunk encoder pipeline.
- Added full documentation site (docs/).

## Observed

- Streaming chunk encoder uses FFmpeg -> ThreadPool -> native helper -> segment merge -> IA4M mux -> optional M2Y2.

