#!/usr/bin/env python3
"""Render the PulseTime icons for the Venu 2S. Both sizes come from the device's
compiler.json: the 61x61 launcher icon and the 39x39 complication icon.

Both draw the same mark - three pulse bars of increasing height, echoing the
hour / quarter / minute pulse fields. The launcher icon sets the mark on the
house dark disc with a cyan rim and is written as a PNG. The complication icon
must be an SVG (the resource compiler rejects a bitmap there) and drops the
disc: a complication is composited onto whatever the watch face is already
drawing, so it needs a transparent ground and one high-contrast glyph.

    python3 scripts/render-icon.py    # writes both files into resources/drawables
"""
from pathlib import Path

from PIL import Image, ImageDraw

LAUNCHER_SIZE = 61      # compiler.json launcherIcon for venu2s
COMPLICATION_SIZE = 39  # compiler.json complicationIcon for venu2s
SCALE = 8  # render large, then downsample for smooth edges
HERE = Path(__file__).resolve().parent
DRAWABLES = HERE.parent / "resources" / "drawables"

# Mark geometry as fractions of the icon edge, so both sizes stay in proportion
# (at 61 px these reproduce the original 7 / 5 / 14 / 22 / 30 px figures exactly).
BAR_W = 7 / 61
GAP = 5 / 61
HEIGHTS = (14 / 61, 22 / 61, 30 / 61)
RIM_INSET = 1.5 / 61
# The complication mark carries the whole icon alone, so it is drawn larger.
COMPLICATION_BOOST = 1.45


def render_launcher() -> Image.Image:
    s = LAUNCHER_SIZE * SCALE
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    # Dark disc (house background #040912) with a subtle cyan rim.
    draw.ellipse((0, 0, s - 1, s - 1), fill=(4, 9, 18, 255))
    rim = int(RIM_INSET * s)
    draw.ellipse((rim, rim, s - 1 - rim, s - 1 - rim), outline=(92, 217, 255, 255), width=SCALE)
    # Three pulse bars: short, medium, tall - centred, rounded ends.
    bar_w = int(BAR_W * s)
    gap = int(GAP * s)
    total_w = 3 * bar_w + 2 * gap
    x = (s - total_w) // 2
    cy = s // 2
    for fraction in HEIGHTS:
        hh = int(fraction * s)
        draw.rounded_rectangle((x, cy - hh // 2, x + bar_w, cy + hh // 2), radius=bar_w // 2,
                               fill=(255, 255, 255, 255))
        x += bar_w + gap
    return img.resize((LAUNCHER_SIZE, LAUNCHER_SIZE), Image.LANCZOS)


def render_complication() -> str:
    s = float(COMPLICATION_SIZE)
    bar_w = BAR_W * COMPLICATION_BOOST * s
    gap = GAP * COMPLICATION_BOOST * s
    x = (s - (3 * bar_w + 2 * gap)) / 2
    cy = s / 2
    bars = []
    for fraction in HEIGHTS:
        hh = fraction * COMPLICATION_BOOST * s
        bars.append(
            f'  <rect x="{x:.2f}" y="{cy - hh / 2:.2f}" width="{bar_w:.2f}" height="{hh:.2f}"'
            f' rx="{bar_w / 2:.2f}" ry="{bar_w / 2:.2f}" fill="#FFFFFF"/>'
        )
        x += bar_w + gap
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{COMPLICATION_SIZE}"'
        f' height="{COMPLICATION_SIZE}" viewBox="0 0 {COMPLICATION_SIZE} {COMPLICATION_SIZE}">\n'
        + "\n".join(bars)
        + "\n</svg>\n"
    )


if __name__ == "__main__":
    DRAWABLES.mkdir(parents=True, exist_ok=True)
    launcher = DRAWABLES / "launcher_icon.png"
    render_launcher().save(launcher, optimize=True)
    print(f"wrote {launcher} ({LAUNCHER_SIZE}x{LAUNCHER_SIZE})")
    complication = DRAWABLES / "complication_icon.svg"
    complication.write_text(render_complication())
    print(f"wrote {complication} ({COMPLICATION_SIZE}x{COMPLICATION_SIZE})")
