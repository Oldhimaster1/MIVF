#!/usr/bin/env python3
"""Generate the icon, banner image, and banner audio used for SMDH/CIA packaging.

Outputs (written next to this script, in meta/):
  - icon48.png   48x48 RGBA   -> SMDH icon (smdhtool) / CIA icon
  - banner.png   256x128 RGBA -> CIA banner image (bannertool)
  - banner.wav   short jingle  -> CIA banner audio (bannertool)

Re-run after editing this file:  python meta/make_assets.py
Requires Pillow (pip install pillow).
"""

import math
import os
import struct
import wave

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))

ACCENT = (120, 200, 255, 255)
TEXT = (240, 248, 255, 255)
SUBTEXT = (150, 190, 230, 255)


def load_font(size):
    for path in (r"C:\Windows\Fonts\arialbd.ttf", r"C:\Windows\Fonts\arial.ttf"):
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


def vertical_gradient(draw, w, h, top, bottom):
    for y in range(h):
        t = y / max(1, h - 1)
        r = int(top[0] + (bottom[0] - top[0]) * t)
        g = int(top[1] + (bottom[1] - top[1]) * t)
        b = int(top[2] + (bottom[2] - top[2]) * t)
        draw.line([(0, y), (w, y)], fill=(r, g, b, 255))


def rounded_mask(w, h, radius):
    mask = Image.new("L", (w, h), 0)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, fill=255)
    return mask


def make_icon():
    w = h = 48
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    vertical_gradient(d, w, h, (16, 32, 56), (24, 70, 140))
    # Play triangle.
    d.polygon([(17, 12), (17, 32), (35, 22)], fill=ACCENT)
    # Wordmark.
    f = load_font(11)
    d.text((24, 40), "MIVF", font=f, fill=TEXT, anchor="mm")
    # Round the corners so it reads as an app tile.
    img.putalpha(rounded_mask(w, h, 7))
    img.save(os.path.join(HERE, "icon48.png"))


def make_banner():
    w, h = 256, 128
    img = Image.new("RGBA", (w, h), (0, 0, 0, 255))
    d = ImageDraw.Draw(img)
    vertical_gradient(d, w, h, (10, 20, 40), (18, 50, 110))
    d.polygon([(34, 40), (34, 90), (80, 65)], fill=ACCENT)
    d.text((100, 52), "MIVF", font=load_font(34), fill=TEXT, anchor="lm")
    d.text((101, 82), "3DS VIDEO PLAYER", font=load_font(13), fill=SUBTEXT, anchor="lm")
    img.save(os.path.join(HERE, "banner.png"))


def make_banner_audio():
    sr = 32000
    dur = 1.1
    n = int(sr * dur)
    tones = ((330.0, 0.45), (440.0, 0.28), (660.0, 0.14))
    frames = bytearray()
    for i in range(n):
        t = i / sr
        env = min(1.0, t / 0.04, (dur - t) / 0.25)
        if env < 0.0:
            env = 0.0
        s = sum(a * math.sin(2.0 * math.pi * f * t) for f, a in tones)
        v = int(max(-1.0, min(1.0, s * env)) * 30000)
        frames += struct.pack("<hh", v, v)
    with wave.open(os.path.join(HERE, "banner.wav"), "wb") as wv:
        wv.setnchannels(2)
        wv.setsampwidth(2)
        wv.setframerate(sr)
        wv.writeframes(bytes(frames))


def main():
    make_icon()
    make_banner()
    make_banner_audio()
    print("Wrote icon48.png, banner.png, banner.wav to", HERE)


if __name__ == "__main__":
    main()
