#!/usr/bin/env python3
"""Draw the building-block tiles into rows 2 and up of res/block_atlas.png.

The atlas is 512x512 with 32 px tiles (16x16). The tiles listed in TILES
below fill it in order from row 2, sixteen to a row (see cell());
inc/block/block_data.hpp points each block at its cell. Every tile is drawn from a fixed seed, so running this again
produces the same image.

Only empty cells are written. Pass --force to redraw cells that already hold
art (for example after tweaking a colour here), which overwrites any
hand-painted version. With --png-dir DIR, each tile is also saved as
DIR/<name>.png, which tools/png_to_xcf.py turns into the GIMP sources in
assets/blocks/.

Requires Pillow. Usage: python3 tools/gen_building_tiles.py [--force] [--png-dir DIR]
"""

import argparse
import random
from pathlib import Path

from PIL import Image, ImageDraw

TILE = 32
ROW = 2
ATLAS = Path(__file__).resolve().parent.parent / "res" / "block_atlas.png"


def clamp(v):
    return max(0, min(255, int(v)))


def shade(rgb, d):
    return tuple(clamp(c + d) for c in rgb)


def new_tile():
    return Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))


def pixels(img):
    px = img.load()
    assert px is not None
    return px


def planks(base, rng):
    """Four boards with grain, dark seams and staggered butt joints."""
    img = new_tile()
    px = pixels(img)
    for board in range(4):
        y0 = board * 8
        tone = shade(base, rng.randint(-10, 10))
        joint = rng.randint(6, 26)
        for y in range(y0, y0 + 8):
            streak = rng.randint(-6, 6)
            for x in range(TILE):
                d = streak + rng.randint(-4, 4)
                if rng.random() < 0.06:
                    d -= 14  # a knot or a fleck of darker grain
                c = shade(tone, d)
                if y == y0 + 7:
                    c = shade(base, -45)  # seam under the board
                elif y == y0:
                    c = shade(tone, 14)  # lit top edge
                if x == joint:
                    c = shade(base, -40)
                px[x, y] = c + (255,)
    return img


def bricks(brick, mortar, rng, w=16, h=8, bevel=False):
    """A running bond: rows of bricks, every other row shifted by half."""
    img = new_tile()
    px = pixels(img)
    for row in range(TILE // h):
        y0 = row * h
        shift = (w // 2) * (row % 2)
        for col in range(-1, TILE // w + 1):
            x0 = col * w + shift
            tone = shade(brick, rng.randint(-14, 14))
            for y in range(y0, y0 + h):
                for x in range(x0, x0 + w):
                    if not 0 <= x < TILE:
                        continue
                    edge_x = x == x0 + w - 1
                    edge_y = y == y0 + h - 1
                    if edge_x or edge_y:
                        c = shade(mortar, rng.randint(-6, 6))
                    else:
                        c = shade(tone, rng.randint(-8, 8))
                        if bevel and (x == x0 or y == y0):
                            c = shade(c, 22)
                        elif bevel and (x == x0 + w - 2 or y == y0 + h - 2):
                            c = shade(c, -22)
                    px[x, y] = c + (255,)
    return img


def glass(rng):
    """A pale frame round a clear pane, with two glints across it.

    The pane is alpha 0 and everything drawn is alpha 255, so the cutout
    test in block.vert keeps the frame and drops the pane cleanly."""
    img = new_tile()
    px = pixels(img)
    frame = (196, 226, 236)
    for y in range(TILE):
        for x in range(TILE):
            if x < 2 or y < 2 or x > TILE - 3 or y > TILE - 3:
                d = 24 if (x < 1 or y < 1) else rng.randint(-6, 6)
                px[x, y] = shade(frame, d) + (255,)
    for start, length in ((6, 7), (11, 4)):
        for i in range(length):
            x, y = start + i, start + 12 - i - (start // 2)
            if 2 <= x < TILE - 2 and 2 <= y < TILE - 2:
                px[x, y] = (240, 250, 255, 255)
    return img


def colour(base, rng):
    """A flat, bright block with soft noise and a bevelled edge."""
    img = new_tile()
    px = pixels(img)
    for y in range(TILE):
        for x in range(TILE):
            c = shade(base, rng.randint(-5, 5))
            if x == 0 or y == 0:
                c = shade(c, 28)
            elif x == TILE - 1 or y == TILE - 1:
                c = shade(c, -36)
            elif x == 1 or y == 1:
                c = shade(c, 12)
            elif x == TILE - 2 or y == TILE - 2:
                c = shade(c, -16)
            px[x, y] = c + (255,)
    return img


def crystal(rng):
    """Glowing shards growing out of dark rock."""
    img = new_tile()
    px = pixels(img)
    rock = (52, 50, 64)
    for y in range(TILE):
        for x in range(TILE):
            px[x, y] = shade(rock, rng.randint(-8, 8)) + (255,)
    draw = ImageDraw.Draw(img)
    # (base x, base y, height, half width, lean, colour): back to front.
    shards = [
        (7, 30, 17, 3, -3, (150, 110, 240)),
        (25, 31, 15, 3, 3, (150, 110, 240)),
        (19, 31, 22, 4, 1, (110, 225, 240)),
        (11, 31, 25, 4, -1, (120, 235, 250)),
        (27, 14, 9, 2, 2, (200, 150, 250)),
        (4, 12, 8, 2, -2, (110, 225, 240)),
    ]
    for bx, by, h, w, lean, colour in shards:
        tip = (bx + lean, by - h)
        left = [(bx - w, by), (bx - w + lean, by - h + w + 1), tip, (bx, by)]
        right = [(bx, by), tip, (bx + w + lean, by - h + w + 1), (bx + w, by)]
        draw.polygon(left, fill=shade(colour, -35) + (255,))
        draw.polygon(right, fill=colour + (255,))
        draw.line([(bx, by - 1), tip], fill=(235, 250, 255, 255))
    return img


def table_top(rng):
    """A worktop: oak boards inside a darker frame, with a square marked out."""
    img = planks((150, 116, 74), rng)
    px = pixels(img)
    frame = (92, 64, 36)
    for y in range(TILE):
        for x in range(TILE):
            if x < 3 or y < 3 or x > TILE - 4 or y > TILE - 4:
                px[x, y] = shade(frame, rng.randint(-8, 8) + (14 if x == 0 or y == 0 else 0)) + (255,)
    for i in range(9, 23):  # a marked-out square, scored into the wood
        for x, y in ((i, 9), (i, 22), (9, i), (22, i)):
            px[x, y] = (110, 80, 48, 255)
    return img


def table_side(rng):
    """Boards with legs at the corners and a hammer and saw on a rack."""
    img = planks((150, 116, 74), rng)
    px = pixels(img)
    frame = (92, 64, 36)
    for y in range(TILE):
        for x in range(TILE):
            if y < 4 or x < 4 or x > TILE - 5:
                px[x, y] = shade(frame, rng.randint(-8, 8)) + (255,)
    draw = ImageDraw.Draw(img)
    draw.rectangle([8, 9, 9, 24], fill=(110, 76, 44, 255))      # hammer handle
    draw.rectangle([5, 8, 13, 11], fill=(150, 150, 158, 255))   # hammer head
    draw.rectangle([5, 8, 13, 8], fill=(200, 200, 208, 255))
    draw.polygon([(17, 10), (26, 10), (26, 24), (17, 20)], fill=(180, 182, 190, 255))  # saw blade
    for y in range(12, 22, 2):
        draw.point((26, y), fill=(120, 122, 130, 255))
    draw.rectangle([15, 9, 18, 13], fill=(110, 76, 44, 255))    # saw grip
    return img


def lamp(rng):
    """Four glowing panes in a dark iron frame, brightest in the middle."""
    img = new_tile()
    px = pixels(img)
    iron = (58, 52, 50)
    for y in range(TILE):
        for x in range(TILE):
            bar = x < 3 or y < 3 or x > TILE - 4 or y > TILE - 4
            bar = bar or 15 <= x <= 16 or 15 <= y <= 16
            if bar:
                c = shade(iron, rng.randint(-6, 6))
                if x in (0, 15) or y in (0, 15):
                    c = shade(c, 18)
            else:
                dist = abs(x - 15.5) + abs(y - 15.5)
                glow = 255 - dist * 3.2
                c = (clamp(glow + 10), clamp(glow * 0.86), clamp(glow * 0.42 + 20))
                c = shade(c, rng.randint(-5, 5))
            px[x, y] = c + (255,)
    return img


# (file name, drawing) in row-2 order, matching block_data.hpp.
TILES = [
    ("oak_planks",    lambda r: planks((150, 116, 74), r)),
    ("pine_planks",   lambda r: planks((104, 72, 40), r)),
    ("palm_planks",   lambda r: planks((210, 180, 132), r)),
    ("cherry_planks", lambda r: planks((186, 112, 104), r)),
    ("bricks",        lambda r: bricks((168, 72, 54), (200, 192, 180), r)),
    ("stone_bricks",  lambda r: bricks((128, 130, 130), (82, 84, 86), r,
                                       h=16, bevel=True)),
    ("glass",         glass),
    ("white_block",   lambda r: colour((236, 236, 232), r)),
    ("red_block",     lambda r: colour((214, 46, 46), r)),
    ("orange_block",  lambda r: colour((240, 132, 30), r)),
    ("yellow_block",  lambda r: colour((246, 214, 46), r)),
    ("green_block",   lambda r: colour((84, 184, 60), r)),
    ("blue_block",    lambda r: colour((48, 104, 220), r)),
    ("purple_block",  lambda r: colour((142, 66, 196), r)),
    ("pink_block",    lambda r: colour((244, 140, 190), r)),
    ("lamp",          lamp),
    # Row 3
    ("glow_crystal",  crystal),
    ("crafting_table_top",  table_top),
    ("crafting_table_side", table_side),
]


def cell(index):
    """The atlas cell (col, row) of TILES[index]."""
    return index % 16, ROW + index // 16


def is_empty(atlas, col, row):
    box = atlas.crop((col * TILE, row * TILE, (col + 1) * TILE, (row + 1) * TILE))
    return box.getchannel("A").getbbox() is None


def main():
    ap = argparse.ArgumentParser(description="Draw the building-block tiles.")
    ap.add_argument("--force", action="store_true", help="redraw cells that hold art")
    ap.add_argument("--png-dir", type=Path, help="also save each tile here")
    args = ap.parse_args()

    atlas = Image.open(ATLAS).convert("RGBA")
    if args.png_dir:
        args.png_dir.mkdir(parents=True, exist_ok=True)
    for i, (name, draw) in enumerate(TILES):
        col, row = cell(i)
        tile = draw(random.Random(f"{name}:{i}"))
        if args.png_dir:
            tile.save(args.png_dir / f"{name}.png")
        if not args.force and not is_empty(atlas, col, row):
            print(f"skip {name}: cell ({col}, {row}) already has art")
            continue
        atlas.paste(tile, (col * TILE, row * TILE))
        print(f"drew {name} at ({col}, {row})")
    atlas.save(ATLAS)


if __name__ == "__main__":
    main()
