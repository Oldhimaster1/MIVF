"""Basic-mode encode presets.

Implements mivf_customization_gui_20260716/ENCODER_GUI_INFORMATION_ARCHITECTURE.md
"Basic-mode presets, mapped to real flags only" table. Every flag name here
is checked against the real encode_mivf.py build_parser() (encode_mivf.py:2621-2687)
-- see CURRENT_ENCODER_OPTION_CLASSIFICATION.md for the PUBLIC STABLE/ADVANCED/
EXPERIMENTAL tiering this table respects. No preset sets an EXPERIMENTAL flag
(e.g. any --motion-search value other than "full") or claims unverified
hardware status -- "3ds-fast" is presented as a real flag name with a plain
description, never as "Old 3DS optimized" or similar.
"""
from __future__ import annotations

PRESET_NAMES = ["fast_test", "balanced", "high_quality", "lower_performance_hardware", "custom"]

PRESET_LABELS = {
    "fast_test": "Fast Test",
    "balanced": "Balanced",
    "high_quality": "High Quality",
    "lower_performance_hardware": "Lower-Performance Hardware",
    "custom": "Custom",
}

PRESET_DESCRIPTIONS = {
    "fast_test": "Quick correctness check, not a quality target.",
    "balanced": "The encoder's own defaults, already tuned.",
    "high_quality": "M2Y2 range coding, full-detail transform coefficients.",
    "lower_performance_hardware": "Uses the encoder's own 3ds-fast profile (faster encode, smaller packets).",
    "custom": "No flags pre-set; use Advanced mode.",
}


def preset_flags(preset: str) -> dict[str, object]:
    """Returns {flag_name: value} using real encode_mivf.py flag names
    (without the leading '--', underscores instead of hyphens, matching
    argparse's dest convention) -- backend.py turns this into a real argv."""
    if preset == "fast_test":
        return {"width": 200, "height": 120, "qp": 36}
    if preset == "balanced":
        return {}  # encoder's own defaults; no flags emitted at all
    if preset == "high_quality":
        return {"m2y2": True, "keep": 16}
    if preset == "lower_performance_hardware":
        return {"profile": "3ds-fast"}
    if preset == "custom":
        return {}
    raise ValueError(f"unknown preset {preset!r}")
