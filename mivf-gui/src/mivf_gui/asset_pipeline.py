"""Real image-conversion pipeline backing the GUI's preview.

Phase C.1 root-cause fix: the original PremierePreviewWidget never loaded
or displayed the user's selected image at all -- it only drew a boolean
"has an underlay: yes/no" glow circle. This module actually decodes,
fits/crops, and RGB565+mask-encodes source images through the SAME shared
core (tools/mivf_theme_asset_common.py) the real desktop conversion tools
use, so what the GUI shows is the literal bytes a real .mivfasset would
contain -- not a second, independently-approximated guess.

No PySide6 import at the encode layer -- only process_asset()'s QPixmap
conversion touches Qt, so the core logic stays testable without a display.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path

_TOOLS_DIR = Path(__file__).resolve().parents[3] / "tools"
if str(_TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(_TOOLS_DIR))

from mivf_theme_asset_common import (  # noqa: E402
    fit_to_canvas,
    rgb565le_bytes,
    quantize_rgb565_preview,
    encode_control_asset,
    masked_preview_rgba,
    DEFAULT_ALPHA_THRESHOLD,
)
from PIL import Image  # noqa: E402


class FitMode(str, Enum):
    CONTAIN = "contain"
    COVER = "cover"
    STRETCH = "stretch"
    CENTER_CROP = "center_crop"


class AssetState(str, Enum):
    EMPTY = "empty"          # no path given -- built-in fallback, not an error
    READY = "ready"
    WARNING = "warning"      # loaded and converted, but something is worth flagging
    ERROR = "error"          # could not be decoded/converted
    MISSING = "missing"      # path given but file does not exist


@dataclass
class AssetResult:
    state: AssetState
    message: str
    # PIL images, not yet Qt -- process_asset_qt() below adds pixmaps.
    source_image: Image.Image | None = None
    prepared_image: Image.Image | None = None   # after fit/crop, before RGB565 encode
    runtime_image: Image.Image | None = None    # RGB565-quantized + real mask alpha baked in
    mask_image: Image.Image | None = None       # hard-edge thresholded mask only
    source_w: int = 0
    source_h: int = 0
    source_format: str = ""
    source_bytes: int = 0
    has_alpha: bool = False
    runtime_w: int = 0
    runtime_h: int = 0
    runtime_bytes: int = 0
    mask_byte_count: int = 0
    visible_px: int = 0
    transparent_px: int = 0
    raw_pixels: bytes = field(default=b"", repr=False)
    raw_mask: bytes = field(default=b"", repr=False)


_cache: dict[tuple, AssetResult] = {}


def clear_cache() -> None:
    _cache.clear()


def _apply_fit(image: Image.Image, w: int, h: int, fit_mode: FitMode, opaque: bool, margin: int = 0) -> Image.Image:
    rgba = image.convert("RGBA")
    if fit_mode == FitMode.STRETCH:
        return rgba.resize((w, h), Image.Resampling.LANCZOS)
    if fit_mode in (FitMode.COVER, FitMode.CENTER_CROP):
        scale = max(w / rgba.width, h / rgba.height)
        new_size = (max(1, round(rgba.width * scale)), max(1, round(rgba.height * scale)))
        scaled = rgba.resize(new_size, Image.Resampling.LANCZOS)
        left = (scaled.width - w) // 2
        top = (scaled.height - h) // 2
        return scaled.crop((left, top, left + w, top + h))
    # CONTAIN (default). margin matches the real tool's own default for
    # this asset type -- 0 for control assets/dashboard bg (unchanged from
    # before), 2 for .cover (mivf_make_cover.py's own --margin default).
    # This was a real fidelity bug: byte COUNT matched at margin=0, but the
    # actual pixel content didn't match mivf_make_cover.py's real output
    # until this was added -- caught by a direct SHA-256 comparison, not
    # assumed correct from a matching size alone.
    return fit_to_canvas(image, w, h, bg=(0, 0, 0), margin=margin, opaque=opaque)


def process_asset(
    path_str: str | None,
    w: int,
    h: int,
    has_mask: bool,
    fit_mode: FitMode = FitMode.CONTAIN,
    threshold: int = DEFAULT_ALPHA_THRESHOLD,
    header_bytes: int = 12,
    format_label: str = ".mivfasset",
    margin: int = 0,
) -> AssetResult:
    """Pure-PIL pipeline (no Qt). Cached by (path, mtime, dims, mask policy,
    fit mode, threshold, header size) -- a file that hasn't changed on disk
    is never re-decoded, and changing any parameter always produces a fresh
    result.

    header_bytes/format_label exist because not every MIVF runtime image
    format has the same envelope: FF/PP/Rewind/Back and the dashboard
    background are all "MVCA" assets with a real 12-byte header (mask or
    not); but .cover/.preview.cover/.screensaver.cover are headerless raw
    RGB565 dumps with NO header at all. Defaulting header_bytes=12 keeps
    every existing call site (all of which are real MVCA assets) exactly as
    it was; only a genuinely headerless format needs to pass header_bytes=0
    -- this was a real bug caught while building cover-artwork parity:
    the original code always added 12, which silently over-reported a
    .cover file's real size by 12 bytes if reused as-is for that format."""
    if not path_str:
        return AssetResult(state=AssetState.EMPTY, message="No image selected — using built-in appearance.")

    path = Path(path_str)
    if not path.exists():
        return AssetResult(state=AssetState.MISSING, message=f"File not found: {path}")

    try:
        mtime = path.stat().st_mtime
        source_bytes = path.stat().st_size
    except OSError as e:
        return AssetResult(state=AssetState.ERROR, message=f"Could not stat file: {e}")

    key = (str(path), mtime, w, h, has_mask, fit_mode, threshold, header_bytes, margin)
    cached = _cache.get(key)
    if cached is not None:
        return cached

    try:
        source_img = Image.open(path)
        source_img.load()
    except Exception as e:  # noqa: BLE001 -- any decode failure is a real, reportable error
        result = AssetResult(state=AssetState.ERROR, message=f"This image could not be decoded: {e}")
        _cache[key] = result
        return result

    source_format = source_img.format or path.suffix.lstrip(".").upper() or "unknown"
    has_alpha = source_img.mode in ("RGBA", "LA", "PA") or "transparency" in source_img.info

    warnings: list[str] = []
    prepared = _apply_fit(source_img, w, h, fit_mode, opaque=not has_mask, margin=margin)

    if has_mask:
        if not has_alpha:
            warnings.append(
                f"The selected {source_format} has no transparency. "
                "The runtime asset will be fully opaque; choose a transparent PNG for a soft-edged button face image."
            )
        pixels, mask = encode_control_asset(prepared, threshold=threshold)
        visible = sum(bin(b).count("1") for b in mask)
        total = w * h
        transparent = total - visible
        if visible == 0:
            warnings.append(f"Fully transparent mask: this artwork will not be visible at runtime.")
        elif transparent == 0 and has_alpha:
            warnings.append("Fully opaque mask: artwork will cover the full control rectangle beneath later layers.")

        quantized_rgb = quantize_rgb565_preview(prepared.convert("RGB"))
        real_alpha = masked_preview_rgba(prepared, threshold=threshold).split()[3]
        runtime_image = Image.merge("RGBA", (*quantized_rgb.split(), real_alpha))
        mask_image = masked_preview_rgba(prepared, threshold=threshold)
        runtime_bytes = header_bytes + len(pixels) + len(mask)
        mask_byte_count = len(mask)
    else:
        pixels = rgb565le_bytes(prepared.convert("RGB"))
        mask = b""
        visible = w * h
        transparent = 0
        runtime_image = quantize_rgb565_preview(prepared.convert("RGB")).convert("RGBA")
        mask_image = None
        runtime_bytes = header_bytes + len(pixels)
        mask_byte_count = 0

    state = AssetState.WARNING if warnings else AssetState.READY
    message = " ".join(warnings) if warnings else f"Ready: {w}x{h}, {runtime_bytes} bytes (matches real {format_label} output)"

    result = AssetResult(
        state=state,
        message=message,
        source_image=source_img.convert("RGBA"),
        prepared_image=prepared,
        runtime_image=runtime_image,
        mask_image=mask_image,
        source_w=source_img.width,
        source_h=source_img.height,
        source_format=source_format,
        source_bytes=source_bytes,
        has_alpha=has_alpha,
        runtime_w=w,
        runtime_h=h,
        runtime_bytes=runtime_bytes,
        mask_byte_count=mask_byte_count,
        visible_px=visible,
        transparent_px=transparent,
        raw_pixels=pixels,
        raw_mask=mask,
    )
    _cache[key] = result
    return result


def pil_to_qpixmap(image: Image.Image):
    """Isolated Qt touchpoint -- imports PySide6 lazily so process_asset()
    above stays importable/testable with plain PIL, no display required."""
    from PySide6.QtGui import QImage, QPixmap

    rgba = image.convert("RGBA")
    data = rgba.tobytes("raw", "RGBA")
    qimg = QImage(data, rgba.width, rgba.height, QImage.Format.Format_RGBA8888)
    return QPixmap.fromImage(qimg.copy())
