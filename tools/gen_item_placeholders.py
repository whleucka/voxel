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


def sticks():
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for (x0, y0, x1, y1) in [(5, 24, 17, 5), (15, 28, 27, 10)]:  # two twigs, apart
        d.line([(x0, y0), (x1, y1)], fill=(70, 48, 26, 255), width=4)
        d.line([(x0, y0), (x1, y1)], fill=(160, 116, 70, 255), width=2)
        d.point((x1, y1), fill=(210, 176, 124, 255))
    d.line([(11, 15), (15, 15)], fill=(70, 48, 26, 255), width=2)  # a side twig
    return img


def ingot(fill, dark, light):
    def draw():
        img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        # A bar seen from above at an angle: a top face and a darker front.
        d.polygon([(6, 14), (12, 9), (27, 9), (21, 14)], fill=light + (255,), outline=dark + (255,))
        d.polygon([(6, 14), (21, 14), (21, 22), (6, 22)], fill=fill + (255,), outline=dark + (255,))
        d.polygon([(21, 14), (27, 9), (27, 17), (21, 22)], fill=dark + (255,), outline=dark + (255,))
        d.line([(8, 16), (14, 16)], fill=light + (255,))
        return img
    return draw


def charcoal():
    return tile_image(LUMP, (58, 44, 36), (20, 14, 10), (110, 90, 74))


# Tools: a wooden handle and a head in the tier's colours (fill, dark edge).
TIERS = [
    ((166, 122, 74), (96, 66, 36)),     # wood
    ((136, 136, 142), (72, 72, 78)),    # stone
    ((214, 120, 64), (118, 58, 26)),    # copper
    ((216, 216, 222), (112, 112, 120)), # iron
    ((222, 44, 66), (110, 12, 28)),     # ruby
    ((60, 204, 104), (20, 100, 50)),    # emerald
    ((112, 232, 242), (30, 120, 140)),  # diamond
]
HANDLE, HANDLE_DARK = (150, 106, 62), (74, 50, 26)


def handle(d, a, b):
    d.line([a, b], fill=HANDLE_DARK + (255,), width=4)
    d.line([a, b], fill=HANDLE + (255,), width=2)


def pickaxe(fill, dark):
    def draw():
        img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        handle(d, (7, 26), (21, 12))
        d.arc([5, 3, 29, 23], 200, 340, fill=dark + (255,), width=6)  # the head's curve
        d.arc([6, 4, 28, 22], 205, 335, fill=fill + (255,), width=3)
        return img
    return draw


def axe(fill, dark):
    def draw():
        img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        handle(d, (8, 27), (21, 7))
        blade = [(17, 5), (27, 7), (28, 16), (20, 17)]
        d.polygon(blade, fill=fill + (255,), outline=dark + (255,))
        d.line([(27, 8), (27, 15)], fill=(255, 255, 255, 120))
        return img
    return draw


def shovel(fill, dark):
    def draw():
        img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        handle(d, (6, 26), (18, 14))
        d.line([(4, 24), (9, 29)], fill=HANDLE_DARK + (255,), width=3)  # grip
        spade = [(17, 11), (23, 5), (29, 8), (27, 15), (21, 17)]
        d.polygon(spade, fill=fill + (255,), outline=dark + (255,))
        return img
    return draw


def sword(fill, dark):
    def draw():
        img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        d.line([(11, 21), (27, 5)], fill=dark + (255,), width=5)   # blade
        d.line([(11, 21), (27, 5)], fill=fill + (255,), width=3)
        d.line([(13, 19), (25, 7)], fill=(255, 255, 255, 140))
        d.line([(7, 17), (15, 25)], fill=HANDLE_DARK + (255,), width=3)  # guard
        handle(d, (5, 27), (10, 22))
        return img
    return draw


TOOLS = [make(*t) for make in (pickaxe, axe, shovel, sword) for t in TIERS]


def fishing_rod():
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    handle(d, (4, 28), (24, 5))
    d.line([(4, 28), (8, 24)], fill=(60, 60, 64, 255), width=4)  # grip
    d.line([(24, 5), (27, 20)], fill=(230, 230, 236, 255))      # the line
    d.ellipse([24, 19, 30, 25], fill=(220, 40, 40, 255))        # bobber
    d.rectangle([24, 22, 30, 25], fill=(245, 245, 245, 255))
    return img


# Fish: a body, a tail and an eye, in the species' colours (body, dark, fin).
# Cooked fish are the same shape, browned, with grill marks.
FISH = [
    ((126, 150, 120), (64, 80, 62), (170, 186, 150)),  # Pebblefish
    ((244, 140, 40), (150, 70, 10), (255, 214, 60)),   # Sunfin
    ((36, 60, 130), (14, 24, 64), (90, 240, 240)),     # Glowfin
]
COOKED = ((176, 112, 58), (96, 56, 24), (214, 160, 96))


def fish(body, dark, fin, cooked=False):
    def draw():
        img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        b, k, f = COOKED if cooked else (body, dark, fin)
        d.polygon([(22, 16), (30, 9), (30, 23)], fill=f + (255,), outline=k + (255,))  # tail
        d.ellipse([3, 9, 25, 23], fill=b + (255,), outline=k + (255,))
        d.polygon([(11, 10), (16, 5), (19, 10)], fill=f + (255,))  # top fin
        if cooked:
            for x in (10, 15, 20):
                d.line([(x, 11), (x - 3, 21)], fill=k + (255,), width=2)
        else:
            for x, y in [(13, 18), (17, 14), (20, 18)]:  # spots
                d.point((x, y), fill=f + (255,))
        d.rectangle([7, 14, 8, 15], fill=(10, 10, 10, 255))
        return img
    return draw


def soggy_boot():
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    boot = [(9, 4), (19, 4), (19, 18), (28, 21), (28, 27), (6, 27), (7, 18)]
    d.polygon(boot, fill=(104, 84, 56, 255), outline=(52, 40, 24, 255))
    d.line([(6, 25), (28, 25)], fill=(52, 40, 24, 255), width=2)  # sole
    d.rectangle([9, 4, 19, 7], fill=(80, 62, 40, 255))
    for x, y in [(23, 10), (26, 6), (24, 3)]:  # drips
        d.ellipse([x - 1, y - 1, x + 1, y + 2], fill=(90, 160, 230, 255))
    return img


def treasure_key():
    img = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    gold, dark, light = (232, 186, 56, 255), (140, 100, 20, 255), (255, 236, 150, 255)
    d.ellipse([3, 3, 15, 15], fill=gold, outline=dark)        # the bow
    d.ellipse([7, 7, 11, 11], fill=(0, 0, 0, 0), outline=dark)
    d.line([(13, 13), (27, 27)], fill=dark, width=5)          # the shaft
    d.line([(13, 13), (27, 27)], fill=gold, width=3)
    d.line([(22, 22), (19, 25)], fill=dark, width=4)          # teeth
    d.line([(25, 25), (22, 28)], fill=dark, width=4)
    d.point((6, 6), fill=light)
    d.point((7, 5), fill=light)
    return img


FISHING = [fishing_rod] + [fish(*c, cooked=cooked) for c in FISH for cooked in (False, True)] + [soggy_boot]

# Animal products, crafted and smelted items, after the shape-based
# materials, in ItemId order.
DRAWN = [wool, berries, shell, petals, sticks,
         ingot((200, 200, 206), (110, 110, 118), (236, 236, 242)),   # iron
         ingot((214, 120, 64), (120, 60, 28), (246, 170, 120)),     # copper
         ingot((240, 200, 60), (150, 110, 20), (255, 236, 140)),    # gold
         charcoal] + TOOLS + FISHING + [treasure_key]


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
