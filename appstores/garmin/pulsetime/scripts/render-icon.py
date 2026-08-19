#!/usr/bin/env python3
"""Render the PulseTime launcher icon for the Venu 2S (61x61 per the device
compiler.json launcherIcon entry): a dark disc with three white pulse bars of
increasing height, echoing the hour / quarter / minute pulse fields.

    python3 scripts/render-icon.py            # writes resources/drawables/launcher_icon.png
"""
from pathlib import Path
from PIL import Image, ImageDraw

SIZE = 61
SCALE = 8  # render large, then downsample for smooth edges
HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "resources" / "drawables" / "launcher_icon.png"


def render() -> Image.Image:
    s = SIZE * SCALE
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    # Dark disc (house background #040912) with a subtle cyan rim.
    draw.ellipse((0, 0, s - 1, s - 1), fill=(4, 9, 18, 255))
    rim = int(1.5 * SCALE)
    draw.ellipse((rim, rim, s - 1 - rim, s - 1 - rim), outline=(92, 217, 255, 255), width=SCALE)
    # Three pulse bars: short, medium, tall — centred, rounded ends.
    bar_w = int(7 * SCALE)
    gap = int(5 * SCALE)
    heights = [14, 22, 30]
    total_w = 3 * bar_w + 2 * gap
    x = (s - total_w) // 2
    cy = s // 2
    for h in heights:
        hh = int(h * SCALE)
        draw.rounded_rectangle((x, cy - hh // 2, x + bar_w, cy + hh // 2), radius=bar_w // 2, fill=(255, 255, 255, 255))
        x += bar_w + gap
    return img.resize((SIZE, SIZE), Image.LANCZOS)


if __name__ == "__main__":
    OUT.parent.mkdir(parents=True, exist_ok=True)
    render().save(OUT, optimize=True)
    print(f"wrote {OUT} ({SIZE}x{SIZE})")
