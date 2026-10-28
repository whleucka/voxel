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


def furnace_body(rng):
    """Rough stone blocks: the furnace's top and the frame round its mouth."""
    img = bricks((122, 122, 126), (78, 78, 82), rng, w=8, h=8, bevel=True)
    return img


def furnace_top(rng):
    img = furnace_body(rng)
    px = pixels(img)
    for y in range(10, 22):  # a grate over the fire
        for x in range(10, 22):
            px[x, y] = (40, 38, 40, 255) if (x + y) % 3 else (70, 66, 64, 255)
    return img


def furnace_side(rng, lit):
    img = furnace_body(rng)
    px = pixels(img)
    # The mouth: an arch in the lower half, dark inside, embers when lit.
    for y in range(14, 28):
        for x in range(7, 25):
            dx = (x - 15.5) / 9.0
            if y < 20 and dx * dx + ((y - 20) / 6.0) ** 2 > 1.0:
                continue
            edge = y == 14 or x in (7, 24) or y == 27
            if edge:
                px[x, y] = (60, 58, 60, 255)
            elif not lit:
                px[x, y] = shade((24, 22, 24), rng.randint(-4, 4)) + (255,)
            else:
                heat = (y - 14) / 13.0  # hotter at the bottom
                flick = rng.randint(-20, 20)
                px[x, y] = (clamp(230 + flick * 0.3), clamp(90 + 140 * heat + flick),
                            clamp(20 + 40 * heat), 255)
    return img


def chest_wood(base, frame, rng):
    """Boards inside a darker frame: the body of a chest."""
    img = planks(base, rng)
    px = pixels(img)
    for y in range(TILE):
        for x in range(TILE):
            if x < 2 or y < 2 or x > TILE - 3 or y > TILE - 3:
                px[x, y] = shade(frame, rng.randint(-6, 6) + (12 if x == 0 or y == 0 else 0)) + (255,)
    return img


def chest_top(rng):
    return chest_wood((150, 112, 66), (86, 58, 30), rng)


def chest_side(rng):
    img = chest_wood((150, 112, 66), (86, 58, 30), rng)
    draw = ImageDraw.Draw(img)
    draw.rectangle([2, 11, 29, 12], fill=(70, 46, 22, 255))       # the lid's edge
    draw.rectangle([13, 9, 18, 16], fill=(168, 168, 176, 255))    # latch
    draw.rectangle([13, 9, 18, 9], fill=(214, 214, 222, 255))
    draw.rectangle([15, 13, 16, 14], fill=(60, 60, 66, 255))
    return img


GOLD, GOLD_DARK, GOLD_LIGHT = (222, 176, 52), (140, 100, 20), (255, 228, 120)


def gold_bands(img, horizontal):
    """Gold straps across a treasure chest."""
    px = pixels(img)
    for i in range(TILE):
        for b in (5, 26):
            for w in range(3):
                x, y = (i, b + w) if horizontal else (b + w, i)
                px[x, y] = (GOLD_LIGHT if w == 0 else GOLD if w == 1 else GOLD_DARK) + (255,)


def treasure_top(rng):
    img = chest_wood((84, 50, 30), (44, 26, 14), rng)
    gold_bands(img, horizontal=False)
    return img


def treasure_side(rng):
    img = chest_wood((84, 50, 30), (44, 26, 14), rng)
    gold_bands(img, horizontal=False)
    draw = ImageDraw.Draw(img)
    draw.rectangle([2, 11, 29, 12], fill=(36, 20, 10, 255))       # the lid's edge
    draw.rectangle([11, 9, 20, 19], fill=GOLD + (255,), outline=GOLD_DARK + (255,))  # lock plate
    draw.rectangle([12, 10, 19, 10], fill=GOLD_LIGHT + (255,))
    draw.ellipse([14, 12, 17, 15], fill=(30, 18, 8, 255))         # keyhole
    draw.rectangle([15, 15, 16, 17], fill=(30, 18, 8, 255))
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


DOOR_WOOD = (150, 116, 74)  # oak planks


def door_face(rng, top):
    """Upright boards in a frame: a window up top, a handle lower down.

    The window is alpha 0 (like glass, the cutout test drops it), so you can
    see out of a closed door."""
    img = new_tile()
    px = pixels(img)
    for x in range(TILE):
        board = x // 8
        tone = shade(DOOR_WOOD, (board % 2) * 8 - 4)
        for y in range(TILE):
            c = shade(tone, rng.randint(-5, 5))
            if x % 8 == 0 and 2 < x < TILE - 3:
                c = shade(DOOR_WOOD, -38)  # seam between boards
            if x < 3 or x > TILE - 4 or (y < 3 and top) or (y > TILE - 4 and not top):
                c = shade(DOOR_WOOD, -22 + rng.randint(-4, 4))  # the frame
            px[x, y] = c + (255,)
    if top:
        for y in range(7, 23):
            for x in range(7, 25):
                edge = x in (7, 24) or y in (7, 22)
                mullion = x in (15, 16) or y == 15
                if edge or mullion:
                    px[x, y] = shade(DOOR_WOOD, -50) + (255,)
                else:
                    px[x, y] = (0, 0, 0, 0)
    else:
        # A brass handle on a dark plate, on the side away from the hinge.
        for y in range(2, 9):
            for x in range(22, 27):
                px[x, y] = shade(DOOR_WOOD, -55) + (255,)
        for y in range(4, 7):
            for x in range(19, 25):
                px[x, y] = (226, 182, 70, 255)
        px[20, 4] = (255, 236, 150, 255)
    return img


def door_edge(rng):
    """The panel's thin edges: end grain, darker than its faces."""
    img = new_tile()
    px = pixels(img)
    for y in range(TILE):
        for x in range(TILE):
            px[x, y] = shade(DOOR_WOOD, -16 + rng.randint(-6, 6)) + (255,)
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
    ("furnace_top",         furnace_top),
    ("furnace_side",        lambda r: furnace_side(r, False)),
    ("furnace_lit_side",    lambda r: furnace_side(r, True)),
    ("chest_top",           chest_top),
    ("chest_side",          chest_side),
    ("treasure_chest_top",  treasure_top),
    ("treasure_chest_side", treasure_side),
    ("door_bottom",         lambda r: door_face(r, False)),
    ("door_top",            lambda r: door_face(r, True)),
    ("door_edge",           door_edge),
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
