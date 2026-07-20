# Exact Timing

MIVF's audio muxer requires an **exact integer number of audio samples per video
frame** — it does not support fractional or rounded packet sizes. This works out cleanly
for common combinations:

| Frame rate | Audio rate | Samples/frame | Notes |
| :--- | :--- | :--- | :--- |
| `24000/1001` (~23.976 fps) | 48000 Hz | 2002 | Exact; validated production rate |
| `24/1` | 48000 Hz | 2000 | Exact |
| `30/1` | 48000 Hz | 1600 | Exact |
| `24000/1001` | 44100 Hz | — | **Not supported** — does not divide evenly; the encoder raises an error rather than silently rounding |

Frame rates are parsed with Python's exact `Fraction` type, never a float approximation,
and validated to fit the container's rate fields before encoding starts. The validated
`24000/1001` + 48 kHz IA4M configuration completed the documented long-duration
synchronization tests without observed progressive drift. If you're targeting a
different rational rate, pick an audio rate that divides evenly into it (48000 Hz covers
the common film rates above) — the encoder will refuse to proceed otherwise rather than
produce audio that slowly drifts out of sync. This is a measured result for the
combinations actually tested, not a claim that every rate/audio-rate pair has been
tried — see [Encoding Overview](../authoring/overview.md) for the full explanation.
