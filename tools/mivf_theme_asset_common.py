"""Shared image-processing core for MIVF asset conversion.

Extracted from mivf_make_cover.py / mivf_make_preview_cover.py /
mivf_make_dashboard_bg.py / mivf_make_control_asset.py, which previously
each carried their own copy of the same letterbox-fit and RGB565-encode
logic. Phase C.1: the GUI's live preview (mivf-gui/src/mivf_gui/
asset_pipeline.py) imports this SAME module so the preview reflects the
literal bytes the real conversion tools would produce -- not a second,
independently-written approximation that could silently drift from what
actually gets written to an SD card.

No 3DS/GUI dependency here -- plain Pillow only, importable from either a
CLI script or a Qt app.
"""
from __future__ import annotations

import struct
from PIL import Image, ImageOps

MVCA_MAGIC = b"MVCA"
MVCA_VERSION = 1
DEFAULT_ALPHA_THRESHOLD = 128


def fit_to_canvas(
    image: Image.Image,
    w: int,
    h: int,
    bg: tuple[int, int, int] = (0, 0, 0),
    margin: int = 0,
    opaque: bool = True,
) -> Image.Image:
    """Letterbox/pillarbox-fit `image` onto a `w`x`h` RGBA canvas, centered,
    with `margin` pixels of safe border. If `opaque`, the canvas is filled
    with `bg` first (used for covers/backgrounds); if not, the canvas stays
    transparent outside the fitted image (used for control underlays, where
    "no content" should mean "no mask bit set", not "background color")."""
    image = image.convert("RGBA")
    max_w = max(1, w - margin * 2)
    max_h = max(1, h - margin * 2)
    fitted = ImageOps.contain(image, (max_w, max_h), method=Image.Resampling.LANCZOS)

    fill = tuple(bg) + (255,) if opaque else (0, 0, 0, 0)
    canvas = Image.new("RGBA", (w, h), fill)
    x = (w - fitted.width) // 2
    y = (h - fitted.height) // 2
    canvas.alpha_composite(fitted, (x, y))
    return canvas


def rgb565le_bytes(img: Image.Image, bgr: bool = False) -> bytes:
    """RGB (no alpha) -> raw headerless RGB565 little-endian bytes, the
    format every existing MIVF sidecar image (.cover, .menu_bg.cover,
    .screensaver.cover) already uses."""
    img = img.convert("RGB")
    raw = bytearray()
    for r, g, b in img.getdata():
        if bgr:
            r, b = b, r
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        raw.append(v & 0xFF)
        raw.append((v >> 8) & 0xFF)
    return bytes(raw)


def quantize_rgb565_preview(img: Image.Image) -> Image.Image:
    """Round-trips an RGB image through real RGB565 quantization and back to
    RGB888, so an on-screen preview shows the actual color banding the 3DS
    framebuffer will show -- not the source image's full 24-bit color."""
    img = img.convert("RGB")
    out = Image.new("RGB", img.size)
    src = img.load()
    dst = out.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = src[x, y]
            r5 = (r >> 3) << 3
            g6 = (g >> 2) << 2
            b5 = (b >> 3) << 3
            dst[x, y] = (r5, g6, b5)
    return out


def encode_mask_1bpp(alpha_channel_getter, w: int, h: int, threshold: int = DEFAULT_ALPHA_THRESHOLD) -> bytes:
    """Row-major, MSB-first, 1 = opaque/draw -- matches
    source/mivf_customization.c's mivf_cust_blit_asset() bit convention
    exactly (0x80 >> (bit_idx & 7))."""
    mask = bytearray((w * h + 7) // 8)
    idx = 0
    for y in range(h):
        for x in range(w):
            a = alpha_channel_getter(x, y)
            if a >= threshold:
                mask[idx >> 3] |= (0x80 >> (idx & 7))
            idx += 1
    return bytes(mask)


def encode_control_asset(canvas_rgba: Image.Image, threshold: int = DEFAULT_ALPHA_THRESHOLD) -> tuple[bytes, bytes]:
    """RGBA image -> (RGB565LE pixel bytes, 1bpp mask bytes), the exact
    payload written after an MVCA header by mivf_make_control_asset.py."""
    w, h = canvas_rgba.size
    px = canvas_rgba.load()
    pixels = bytearray()
    for y in range(h):
        for x in range(w):
            r, g, b, _a = px[x, y]
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            pixels.append(v & 0xFF)
            pixels.append((v >> 8) & 0xFF)
    mask = encode_mask_1bpp(lambda x, y: px[x, y][3], w, h, threshold)
    return bytes(pixels), mask


def mvca_header(w: int, h: int) -> bytes:
    return MVCA_MAGIC + struct.pack("<IHH", MVCA_VERSION, w, h)


def masked_preview_rgba(canvas_rgba: Image.Image, threshold: int = DEFAULT_ALPHA_THRESHOLD) -> Image.Image:
    """What the real encode_control_asset() mask will actually keep/drop,
    rendered back as a real RGBA image (hard-edge, no soft alpha) -- used by
    the GUI so a user can literally see the thresholded mask, not just be
    told a number."""
    w, h = canvas_rgba.size
    src = canvas_rgba.load()
    out = Image.new("RGBA", (w, h))
    dst = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = src[x, y]
            dst[x, y] = (r, g, b, 255 if a >= threshold else 0)
    return out
