#!/usr/bin/env python3
"""Generate placeholder icons for non-block items into res/item_atlas.png.

The atlas is 256x256 with 32 px tiles (8x8). Material items fill it row by
row in ItemId order, tile (i % 8, i // 8) (see src/item/item.cpp). Shapes and colours are fixed, so running
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


def wool():
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    puffs = [(10, 13, 7), (17, 10, 8), (23, 14, 7), (12, 20, 7), (20, 20, 8)]
    for x, y, r in puffs:  # outline pass, then fill pass
        d.ellipse([x - r - 1, y - r - 1, x + r + 1, y + r + 1], fill=(150, 150, 160, 255))
    for x, y, r in puffs:
        d.ellipse([x - r, y - r, x + r, y + r], fill=(245, 245, 250, 255))
    d.rectangle([13, 11, 15, 13], fill=(255, 255, 255, 255))
    return img


def berries():
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.polygon([(16, 4), (24, 7), (18, 11)], fill=(60, 160, 70, 255))  # leaf
    for x, y in [(11, 15), (20, 15), (15, 22)]:
        d.ellipse([x - 6, y - 6, x + 6, y + 6], fill=(110, 20, 60, 255))
        d.ellipse([x - 5, y - 5, x + 5, y + 5], fill=(200, 40, 90, 255))
        d.rectangle([x - 3, y - 3, x - 2, y - 2], fill=(255, 190, 210, 255))
    return img


def shell():
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.pieslice([4, 4, 28, 30], 200, 340, fill=(250, 215, 190, 255), outline=(170, 120, 100, 255))
    for x in (8, 12, 16, 20, 24):  # ridges fanning from the hinge
        d.line([(16, 17), (x, 7)], fill=(215, 160, 140, 255))
    d.rectangle([13, 16, 19, 20], fill=(230, 180, 160, 255), outline=(170, 120, 100, 255))
    return img


def petals():
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for x, y in [(16, 8), (24, 14), (21, 23), (11, 23), (8, 14)]:
        d.ellipse([x - 6, y - 6, x + 6, y + 6], fill=(235, 130, 175, 255), outline=(170, 70, 120, 255))
    d.ellipse([12, 12, 20, 20], fill=(255, 220, 90, 255))
    return img


# Animal products, after the shape-based materials, in ItemId order.
DRAWN = [wool, berries, shell, petals]


def main():
    atlas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    tiles = [tile_image(*spec) for spec in MATERIALS] + [draw() for draw in DRAWN]
    cols = SIZE // TILE
    for i, tile in enumerate(tiles):
        atlas.paste(tile, ((i % cols) * TILE, (i // cols) * TILE))
    atlas.save(OUT)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
