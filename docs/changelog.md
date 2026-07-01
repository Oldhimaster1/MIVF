# Changelog

## Unreleased

- Documented the stable streaming chunk encoder pipeline.
- Added full documentation site (docs/).

## Observed

- Streaming chunk encoder uses FFmpeg -> ThreadPool -> native helper -> segment merge -> IA4M mux -> optional M2Y2.

