#!/usr/bin/env python3
"""Generate placeholder icons for non-block items into res/item_atlas.png.

The atlas is 256x256 with 32 px tiles (8x8). Material items occupy row 0 in
ItemId order (see src/item/item.cpp). Shapes and colours are fixed, so running
this again produces the same image. Replace tiles with real art at any time;
nothing in the code depends on what they look like.

Requires Pillow. Usage: python3 tools/gen_item_placeholders.py
"""

from pathlib import Path

from PIL import Image, ImageDraw

TILE = 32
SIZE = 256
OUT = Path(__file__).resolve().parent.parent / "res" / "item_atlas.png"

# Outline points on a 32x32 tile.
GEM = [(16, 3), (27, 12), (16, 29), (5, 12)]
LUMP = [(7, 12), (12, 6), (21, 6), (27, 12), (26, 23), (18, 27), (9, 25), (5, 18)]
NUGGET = [(6, 16), (10, 9), (18, 7), (25, 11), (27, 19), (21, 25), (11, 25)]

# (shape, fill, outline, highlight) per material, in ItemId order.
MATERIALS = [
    (LUMP,   (40, 40, 44),    (12, 12, 14),  (80, 80, 88)),     # Coal
    (GEM,    (110, 230, 240), (30, 120, 140), (220, 255, 255)), # Diamond
    (GEM,    (60, 200, 100),  (20, 100, 50),  (170, 255, 190)), # Emerald
    (GEM,    (220, 40, 60),   (110, 10, 25),  (255, 160, 170)), # Ruby
    (NUGGET, (200, 170, 140), (110, 90, 70),  (240, 220, 200)), # Raw Iron
    (NUGGET, (240, 200, 50),  (140, 100, 10), (255, 240, 150)), # Raw Gold
    (NUGGET, (210, 110, 60),  (120, 55, 25),  (250, 180, 130)), # Raw Copper
]


def tile_image(shape, fill, outline, highlight):
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.polygon(shape, fill=fill + (255,), outline=outline + (255,))
    # A small highlight near the top-left so the shape reads as solid.
    xs = [p[0] for p in shape]
    ys = [p[1] for p in shape]
    hx = min(xs) + (max(xs) - min(xs)) // 3
    hy = min(ys) + (max(ys) - min(ys)) // 3
    draw.rectangle([hx, hy, hx + 2, hy + 2], fill=highlight + (255,))
    return img


def main():
    atlas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    for i, spec in enumerate(MATERIALS):
        atlas.paste(tile_image(*spec), (i * TILE, 0))
    atlas.save(OUT)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
