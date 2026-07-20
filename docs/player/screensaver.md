# Screensaver

**Status:** Build-verified, partially emulator-tested (Azahar) — custom artwork loading
and one rendered screensaver state have been observed. Normal idle-timeout activation,
continuous bounce/collision behavior, and the full input-dismissal path remain
unverified. Not yet part of a documented, model-specific physical-hardware validation
pass — see [Project Status](../status.md).

- Scoped to the **root menu view only** — Scene Selection keeps its own paging state and
  does not trigger the screensaver.
- Any key press dismisses it and resets the idle timer.
- Optional custom image via a `.screensaver.cover` sidecar (96×54 RGB565, exactly 10,368
  bytes) — sidecar-only, no MASB embedding support currently exists for this asset type.

![MIVF DVD-menu screensaver showing custom artwork over a dark theatre background with a wake hint on the bottom screen.](../assets/screenshots/screensaver.png)

*Emulator capture (Azahar), used for interface inspection — not physical-hardware
performance evidence.* This one-off showcase capture confirms custom image loading and
one rendered state. It does not exercise the normal idle timeout, continuous
bounce/collision behavior, or the full input-dismissal path.
