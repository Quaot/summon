"""Draws summon.ico: a ringing service bell, engraved in ink on green, in the style of PdfCraft and Crammy."""

import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

BIG = 1024
SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]

GREEN = (30, 88, 54)
CREAM = (239, 233, 220)
INK = (11, 11, 12)

SHADOW = (26, 30)
BASE = (176, 676, 848, 758)
DOME_CENTRE = (512, 676)
DOME_RADII = (296, 276)
STEM = (492, 300, 532, 408)
BUTTON = (450, 258, 574, 318)

TILT = 11                # degrees the bell leans back, counterclockwise
SPEED_LINES = [(560, 40, 150), (640, 10, 140), (720, 60, 170)]   # y, left end, right end, beside the bell
FRAME = 760              # the drawing's longer side on the 1024 tile
FRAME_SMALL = 860        # larger at 16 to 24 pixels, where every pixel counts


def blank():
    return Image.new("L", (BIG, BIG), 0)


def hatch(clip, spacing, width, slope=1):
    """Parallel diagonal lines inside clip, like the shading on an engraving."""
    lines = blank()
    pen = ImageDraw.Draw(lines)
    for start in range(-BIG, 2 * BIG, spacing):
        pen.line((start, 0, start + slope * BIG, BIG), fill=255, width=width)
    return ImageChops.multiply(lines, clip)


def ink(image, mask, colour=INK):
    image.paste(Image.new("RGBA", (BIG, BIG), colour), (0, 0), mask)


def dome_outline(radii=DOME_RADII, centre=DOME_CENTRE):
    """The top half of an ellipse, closed along the base line."""
    cx, cy = centre
    rx, ry = radii
    return [(cx + rx * math.cos(math.radians(a)), cy - ry * math.sin(math.radians(a))) for a in range(0, 181, 3)]


def rectangle_outline(box):
    left, top, right, bottom = box
    return [(left, top), (right, top), (right, bottom), (left, bottom)]


def polygon_mask(points, offset=(0, 0)):
    mask = blank()
    ImageDraw.Draw(mask).polygon([(x + offset[0], y + offset[1]) for x, y in points], fill=255)
    return mask


def ellipse_mask(box, offset=(0, 0)):
    mask = blank()
    left, top, right, bottom = box
    ImageDraw.Draw(mask).ellipse((left + offset[0], top + offset[1], right + offset[0], bottom + offset[1]), fill=255)
    return mask


def draw(detailed):
    """The bell slides in from the left and leans back as it stops, still ringing. The whole
    drawing is measured and centred on the tile, so the margins stay even."""
    scene = draw_bell(detailed).rotate(TILT, resample=Image.BICUBIC, center=(512, 718))
    pen = ImageDraw.Draw(scene)
    width = 22 if detailed else 48
    for y, left, right in SPEED_LINES if detailed else SPEED_LINES[1:2]:
        pen.line((left, y, right, y), fill=INK, width=width)
        for x in (left, right):
            pen.ellipse((x - width // 2, y - width // 2, x + width // 2, y + width // 2), fill=INK)

    scene = scene.crop(scene.getbbox())
    fit = FRAME if detailed else FRAME_SMALL
    scale = fit / max(scene.size)
    scene = scene.resize((round(scene.width * scale), round(scene.height * scale)), Image.LANCZOS)

    image = Image.new("RGBA", (BIG, BIG), (0, 0, 0, 0))
    tile = blank()
    ImageDraw.Draw(tile).rounded_rectangle((28, 28, 996, 996), radius=150, fill=255)
    ink(image, tile, GREEN)
    image.alpha_composite(scene, ((BIG - scene.width) // 2, (BIG - scene.height) // 2))
    return image


def draw_bell(detailed):
    image = Image.new("RGBA", (BIG, BIG), (0, 0, 0, 0))
    outline = 18 if detailed else 40
    dome = dome_outline()
    base = rectangle_outline(BASE)
    stem = rectangle_outline(STEM)
    pen = ImageDraw.Draw(image)

    if detailed:
        shadow = ImageChops.lighter(polygon_mask(dome, SHADOW), polygon_mask(base, SHADOW))
        shadow = ImageChops.lighter(shadow, ellipse_mask(BUTTON, SHADOW))
        ink(image, hatch(shadow, 22, 7))

    # Stem and button sit behind the dome's outline, so they go first.
    ink(image, polygon_mask(stem), CREAM)
    pen.line(stem + [stem[0]], fill=INK, width=outline, joint="curve")
    ink(image, ellipse_mask(BUTTON), CREAM)
    pen.ellipse(BUTTON, outline=INK, width=outline)

    ink(image, polygon_mask(dome), CREAM)
    if detailed:
        # Shade the right flank of the dome: the dome minus a copy of itself shifted left.
        inner = dome_outline((DOME_RADII[0] - 40, DOME_RADII[1] - 26), (DOME_CENTRE[0] - 52, DOME_CENTRE[1]))
        crescent = ImageChops.subtract(polygon_mask(dome), polygon_mask(inner))
        ink(image, hatch(crescent, 18, 6))
    pen.line(dome, fill=INK, width=outline, joint="curve")

    ink(image, polygon_mask(base), CREAM)
    pen.line(base + [base[0]], fill=INK, width=outline, joint="curve")

    if detailed:
        # A shine on the dome, and sound rings either side of the button.
        pen.arc((300, 456, 520, 676), start=200, end=245, fill=INK, width=16)
        cx, cy = 512, 288
        for radius, sweep in ((112, 24), (168, 22), (226, 18)):
            box = (cx - radius, cy - radius, cx + radius, cy + radius)
            pen.arc(box, start=180 - sweep, end=180 + sweep, fill=INK, width=18)
            pen.arc(box, start=-sweep, end=sweep, fill=INK, width=18)

    return image


def main():
    here = Path(__file__).parent
    detailed = draw(True)
    simple = draw(False)
    frames = [(simple if size <= 24 else detailed).resize((size, size), Image.LANCZOS) for size in SIZES]
    frames[-1].save(here.parent / "summon.ico", sizes=[(s, s) for s in SIZES], append_images=frames[:-1])
    preview = Image.new("RGBA", (256 + 64 + 48 + 32 + 24 + 16 + 60, 256), (240, 240, 240, 255))
    x = 0
    for size in (256, 64, 48, 32, 24, 16):
        frame = (simple if size <= 24 else detailed).resize((size, size), Image.LANCZOS)
        preview.paste(frame, (x, 0), frame)
        x += size + 10
    preview.save(here / "preview.png")


if __name__ == "__main__":
    main()
