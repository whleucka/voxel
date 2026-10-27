#!/usr/bin/env python3
"""Draw the building-block tiles into row 2 of res/block_atlas.png.

The atlas is 512x512 with 32 px tiles (16x16). Row 2 holds, in order, the
tiles listed in TILES below; inc/block/block_data.hpp points each block at
its cell. Every tile is drawn from a fixed seed, so running this again
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

from PIL import Image

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
]


def is_empty(atlas, col):
    cell = atlas.crop((col * TILE, ROW * TILE, (col + 1) * TILE, (ROW + 1) * TILE))
    return cell.getchannel("A").getbbox() is None


def main():
    ap = argparse.ArgumentParser(description="Draw the building-block tiles.")
    ap.add_argument("--force", action="store_true", help="redraw cells that hold art")
    ap.add_argument("--png-dir", type=Path, help="also save each tile here")
    args = ap.parse_args()

    atlas = Image.open(ATLAS).convert("RGBA")
    if args.png_dir:
        args.png_dir.mkdir(parents=True, exist_ok=True)
    for col, (name, draw) in enumerate(TILES):
        tile = draw(random.Random(f"{name}:{col}"))
        if args.png_dir:
            tile.save(args.png_dir / f"{name}.png")
        if not args.force and not is_empty(atlas, col):
            print(f"skip {name}: cell ({col}, {ROW}) already has art")
            continue
        atlas.paste(tile, (col * TILE, ROW * TILE))
        print(f"drew {name} at ({col}, {ROW})")
    atlas.save(ATLAS)


if __name__ == "__main__":
    main()
