# Creating Subtitles

Place a standard `.srt` file next to your video: `yourvideo.srt` for the main track, and
`yourvideo.1.srt`, `yourvideo.2.srt`, … for additional tracks. No conversion tool is
required — the player reads `.srt` directly.

A cue with more than one line renders as two separately centered lines; a third or later
line folds into the second line, since the renderer only supports two lines.

See [Subtitles (player)](../player/subtitles.md) for playback-side behavior (track
cycling, delay, position) and [Files & Sidecars](files-and-sidecars.md#subtitles) for the
exact naming rules.
