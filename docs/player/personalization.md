# Personalization

**Status:** Build-verified. Confirmed running correctly in the Azahar emulator via the
Showcase Revision 3 sequence, which drives these exact screens with real input rather than
setting the underlying values directly. Not yet part of a documented, model-specific
physical-hardware validation pass — see [Project Status](../status.md).

Personalization is reached from the in-playback Settings menu (**SELECT** to open — see
[Playback](playback.md)), not from the Library or the DVD-style menu.

## Theme Picker

Settings → **THEME** row → **A** opens the picker.

- **L / R** — rotate hue
- **D-Pad Left/Right** — adjust saturation
- **D-Pad Up/Down** — adjust brightness/value
- **X** — reset to the default hue/saturation/value
- **Y** — cycle to the next of 4 built-in color presets
- **A** — apply and save (persists a custom theme)
- **B / START** — cancel, restoring the previous theme

## Color Vision Picker

Settings → **COLOR VISION** row → **A** opens the picker.

- **D-Pad Left, or L** — previous color-vision mode
- **D-Pad Right, R, or Y** — next color-vision mode
- **X** — jump to the standard (no correction) mode
- **A** — apply and save
- **B / START** — cancel, restoring the previous mode

Both pickers live-preview their changes on screen before you confirm. Closing Settings
(**B** or **SELECT**) after either picker also persists whatever is currently applied.
