#!/usr/bin/env python3
"""Draw the building-block tiles into rows 2 and up of res/block_atlas.png.

The atlas is 512x512 with 32 px tiles (16x16). The tiles listed in TILES
below fill it in order from row 2, sixteen to a row (see cell());
inc/block/block_data.hpp points each block at its cell. Every tile is drawn from a fixed seed, so running this again
produces the same image.

Only empty cells are written. Pass --force to redraw cells that already hold
art (for example after tweaking a colour here), which overwrites any
hand-painted version. --only NAME... draws just the named tiles (such as
glow_crystal), over whatever their cells hold. With --png-dir DIR, each tile is also saved as
DIR/<name>.png, which tools/png_to_xcf.py turns into the GIMP sources in
assets/blocks/.

Requires Pillow. Usage: python3 tools/gen_building_tiles.py [--force] [--only NAME...] [--png-dir DIR]
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
    """Glowing shards growing up from the bottom edge of a clear tile.

    Everything round the shards is alpha 0, so the cutout test in block.vert
    leaves only the shards. A crystal hanging from a ceiling shows this tile
    upside down (chunk_mesh.cpp), so the shards always grow out of the rock."""
    img = new_tile()
    draw = ImageDraw.Draw(img)
    # (base x, height, half width, lean, colour): back to front.
    shards = [
        (5, 14, 2, -2, (110, 225, 240)),
        (27, 13, 2, 2, (200, 150, 250)),
        (8, 18, 3, -3, (150, 110, 240)),
        (24, 16, 3, 3, (150, 110, 240)),
        (19, 23, 4, 1, (110, 225, 240)),
        (12, 27, 4, -1, (120, 235, 250)),
    ]
    by = TILE - 1
    for bx, h, w, lean, colour in shards:
        tip = (bx + lean, by - h)
        left = [(bx - w, by), (bx - w + lean, by - h + w + 1), tip, (bx, by)]
        right = [(bx, by), tip, (bx + w + lean, by - h + w + 1), (bx + w, by)]
        draw.polygon(left, fill=shade(colour, -35) + (255,))
        draw.polygon(right, fill=colour + (255,))
        draw.line([(bx, by - 1), tip], fill=(235, 250, 255, 255))
    return img


def crystal_end(rng):
    """The tips of the shards seen end-on: a few gems on a clear tile.

    The top and bottom faces of a glow crystal; see crystal()."""
    img = new_tile()
    draw = ImageDraw.Draw(img)
    # (centre x, centre y, radius, colour)
    gems = [
        (10, 11, 5, (120, 235, 250)),
        (22, 9, 3, (200, 150, 250)),
        (21, 22, 5, (150, 110, 240)),
        (8, 24, 3, (110, 225, 240)),
    ]
    for cx, cy, r, colour in gems:
        diamond = [(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)]
        draw.polygon(diamond, fill=shade(colour, -35) + (255,))
        draw.polygon([(cx, cy - r), (cx + r, cy), (cx, cy)], fill=colour + (255,))
        draw.point((cx, cy), fill=(235, 250, 255, 255))
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


def bed_frame(rng):
    """The bed's wooden frame: dark boards running along it."""
    img = new_tile()
    px = pixels(img)
    wood = (120, 84, 50)
    for y in range(TILE):
        tone = shade(wood, ((y // 8) % 2) * 8 - 4)
        for x in range(TILE):
            c = shade(tone, rng.randint(-5, 5))
            if y % 8 == 0:
                c = shade(wood, -34)  # seam between boards
            px[x, y] = c + (255,)
    return img


def bed_pillow(rng):
    """A plump white pillow, puffed in the middle and creased at the edges."""
    img = new_tile()
    px = pixels(img)
    for y in range(TILE):
        for x in range(TILE):
            dx, dy = abs(x - 15.5) / 15.5, abs(y - 15.5) / 15.5
            puff = 1.0 - max(dx, dy) ** 3
            c = shade((214, 214, 218), puff * 34 + rng.randint(-3, 3))
            if x in (0, TILE - 1) or y in (0, TILE - 1):
                c = shade((214, 214, 218), -40)
            px[x, y] = c + (255,)
    return img


def blanket(base, rng):
    """A quilted blanket in one colour.

    Plain all over: the top face isn't turned with the bed, so a band at one
    edge would sit on a different side for each facing."""
    img = new_tile()
    px = pixels(img)
    for y in range(TILE):
        for x in range(TILE):
            c = shade(base, rng.randint(-5, 5))
            if (x % 8 == 4 and y % 2 == 0) or (y % 8 == 4 and x % 2 == 0):
                c = shade(base, -26)  # quilting stitches
            px[x, y] = c + (255,)
    return img


# ─── Farming ─────────────────────────────────────────────────────────────

FARMLAND_DRY = (150, 108, 70)
FARMLAND_WET = (92, 62, 40)


def farmland_top(base):
    """Tilled soil: ridges and furrows running across, darker once wet."""
    def draw(rng):
        img = new_tile()
        px = pixels(img)
        for y in range(TILE):
            furrow = y % 8 in (6, 7)
            for x in range(TILE):
                c = shade(base, (-30 if furrow else 6) + rng.randint(-8, 8))
                px[x, y] = c + (255,)
        return img
    return draw


def farmland_side(base):
    """Soil seen from the side: the tilled crust over plain dirt."""
    def draw(rng):
        img = new_tile()
        px = pixels(img)
        dirt = (134, 96, 66)
        for y in range(TILE):
            for x in range(TILE):
                c = shade(base if y < 5 else dirt, rng.randint(-9, 9))
                if y == 5:
                    c = shade(base, -24)
                px[x, y] = c + (255,)
        return img
    return draw


def crop_top(rng):
    """All clear: a crop is only seen from the side."""
    return new_tile()


def stalk(draw, x, height, colour, lean=0):
    top = (x + lean, TILE - 1 - height)
    draw.line([(x, TILE - 1), top], fill=colour + (255,), width=2)
    return top


def wheat(stage):
    """Wheat: thin green shoots that grow tall and turn gold with ears of grain."""
    def draw_tile(rng):
        img = new_tile()
        d = ImageDraw.Draw(img)
        height = [7, 13, 21, 27][stage]
        green, gold = (88, 168, 56), (222, 186, 74)
        for x in range(3, TILE - 2, 4):
            h = height - rng.randint(0, 4)
            top = stalk(d, x, h, gold if stage == 3 else shade(green, rng.randint(-14, 8)),
                        rng.randint(-1, 1))
            if stage >= 2:  # an ear of grain on top, green until ripe
                ear = shade(gold, -20) if stage == 3 else (150, 190, 80)
                d.ellipse([top[0] - 2, top[1] - 1, top[0] + 2, top[1] + 6], fill=ear + (255,))
        return img
    return draw_tile


def carrot(stage):
    """Carrots: feathery green tops, with orange roots showing once ripe."""
    def draw_tile(rng):
        img = new_tile()
        d = ImageDraw.Draw(img)
        height = [6, 11, 16, 18][stage]
        for x in (6, 16, 26):
            x += rng.randint(-1, 1)
            for lean in (-3, 0, 3):
                stalk(d, x, height - abs(lean), shade((70, 160, 60), rng.randint(-14, 10)), lean)
            if stage == 3:  # the shoulder of the root, at the soil
                d.ellipse([x - 3, TILE - 6, x + 3, TILE + 2], fill=(236, 122, 36, 255),
                          outline=(176, 80, 20, 255))
        return img
    return draw_tile


def strawberry(stage):
    """Strawberries: a low leafy bush, then white flowers, then red berries."""
    def draw_tile(rng):
        img = new_tile()
        d = ImageDraw.Draw(img)
        height = [5, 9, 13, 14][stage]
        leaf = (56, 140, 52)
        for x in range(4, TILE - 2, 6):
            x += rng.randint(-1, 1)
            h = height - rng.randint(0, 3)
            d.ellipse([x - 3, TILE - 1 - h, x + 3, TILE - 1 - h + 5],
                      fill=shade(leaf, rng.randint(-12, 12)) + (255,))
            d.line([(x, TILE - 1), (x, TILE - 1 - h + 3)], fill=shade(leaf, -30) + (255,))
        if stage == 2:
            for x in (8, 19, 27):
                d.ellipse([x - 2, TILE - 13, x + 2, TILE - 9], fill=(250, 250, 240, 255))
                d.point((x, TILE - 11), fill=(250, 210, 60, 255))
        if stage == 3:
            for x, y in ((7, 8), (17, 6), (26, 9), (12, 3)):
                d.polygon([(x - 3, TILE - y - 3), (x + 3, TILE - y - 3), (x, TILE - y + 3)],
                          fill=(220, 34, 50, 255))
                d.point((x, TILE - y - 1), fill=(255, 220, 120, 255))
        return img
    return draw_tile


def tall_grass(rng):
    """Blades of grass leaning this way and that."""
    img = new_tile()
    d = ImageDraw.Draw(img)
    for x in range(2, TILE - 1, 3):
        x += rng.randint(-1, 1)
        h = rng.randint(10, 24)
        d.line([(x, TILE - 1), (x + rng.randint(-3, 3), TILE - 1 - h)],
               fill=shade((84, 156, 56), rng.randint(-20, 14)) + (255,), width=2)
    return img


def flower(petal, centre, kind="round"):
    """A stem with leaves and a bloom on top: round, a tulip cup, a spike or a big hibiscus."""
    def draw_tile(rng):
        img = new_tile()
        d = ImageDraw.Draw(img)
        stem = (60, 140, 50)
        for x, h in ((9, 17), (21, 21), (15, 13)):
            x += rng.randint(-1, 1)
            top = (x, TILE - 1 - h)
            d.line([(x, TILE - 1), top], fill=stem + (255,), width=2)
            d.ellipse([x - 4, TILE - 8, x, TILE - 5], fill=shade(stem, 10) + (255,))
            cx, cy = top
            if kind == "tulip":
                d.polygon([(cx - 3, cy - 5), (cx + 3, cy - 5), (cx + 3, cy), (cx - 3, cy)],
                          fill=petal + (255,))
                d.polygon([(cx - 3, cy - 5), (cx - 1, cy - 7), (cx, cy - 5), (cx + 1, cy - 7),
                           (cx + 3, cy - 5)], fill=petal + (255,))
            elif kind == "spike":
                for k in range(5):
                    d.ellipse([cx - 2, cy - 2 - k * 2, cx + 2, cy + 1 - k * 2],
                              fill=shade(petal, rng.randint(-20, 10)) + (255,))
            else:
                r = 5 if kind == "big" else 3
                for ox, oy in ((-r, 0), (r, 0), (0, -r), (0, r)):
                    d.ellipse([cx + ox - r + 1, cy + oy - r + 1, cx + ox + r - 1, cy + oy + r - 1],
                              fill=petal + (255,))
                d.ellipse([cx - 1, cy - 1, cx + 1, cy + 1], fill=centre + (255,))
        return img
    return draw_tile


def fern(rng):
    """Arching fronds with little leaflets."""
    img = new_tile()
    d = ImageDraw.Draw(img)
    green = (52, 128, 60)
    for lean in (-10, -4, 4, 10):
        top = (16 + lean, 8 + abs(lean) // 2)
        d.line([(16, TILE - 1), top], fill=green + (255,), width=1)
        for t in range(1, 6):
            px_ = 16 + lean * t // 6
            py_ = TILE - 1 - (TILE - 1 - top[1]) * t // 6
            d.line([(px_ - 2, py_ + 1), (px_ + 2, py_ - 1)],
                   fill=shade(green, rng.randint(-10, 20)) + (255,))
    return img


def mushroom(cap, spots):
    def draw_tile(rng):
        img = new_tile()
        d = ImageDraw.Draw(img)
        for x, w, h in ((11, 6, 12), (22, 4, 8)):
            d.rectangle([x - 1, TILE - h, x + 1, TILE - 1], fill=(236, 226, 206, 255))
            d.pieslice([x - w, TILE - h - w, x + w, TILE - h + w], 180, 360, fill=cap + (255,))
            if spots:
                d.point((x - 2, TILE - h - 2), fill=(250, 250, 250, 255))
                d.point((x + 2, TILE - h - 3), fill=(250, 250, 250, 255))
        return img
    return draw_tile


def dead_bush(rng):
    """Dry, bare twigs."""
    img = new_tile()
    d = ImageDraw.Draw(img)
    brown = (140, 100, 60)
    for lean in (-9, -4, 2, 8):
        top = (16 + lean, 10 + rng.randint(0, 6))
        d.line([(16, TILE - 1), top], fill=shade(brown, rng.randint(-15, 10)) + (255,))
        mid = ((16 + top[0]) // 2, (TILE - 1 + top[1]) // 2)
        d.line([mid, (mid[0] + (3 if lean > 0 else -3), mid[1] - 4)], fill=brown + (255,))
    return img


def berry_bush(ripe):
    def draw_tile(rng):
        img = new_tile()
        d = ImageDraw.Draw(img)
        leaf = (50, 120, 48)
        for _ in range(40):
            x, y = rng.randint(4, 27), rng.randint(10, 29)
            d.ellipse([x - 3, y - 2, x + 3, y + 2], fill=shade(leaf, rng.randint(-18, 16)) + (255,))
        if ripe:
            for _ in range(9):
                x, y = rng.randint(6, 25), rng.randint(12, 27)
                d.ellipse([x - 2, y - 2, x + 1, y + 1], fill=(196, 30, 60, 255))
                d.point((x - 1, y - 1), fill=(255, 150, 160, 255))
        return img
    return draw_tile


def shells(rng):
    """A few seashells lying on the sand, at the bottom of the tile."""
    img = new_tile()
    d = ImageDraw.Draw(img)
    for x, c in ((8, (246, 230, 214)), (19, (240, 180, 170)), (26, (236, 236, 226))):
        d.pieslice([x - 4, TILE - 8, x + 4, TILE], 180, 360, fill=c + (255,),
                   outline=shade(c, -60) + (255,))
        for k in (-2, 0, 2):
            d.line([(x, TILE - 1), (x + k, TILE - 7)], fill=shade(c, -50) + (255,))
    return img


def cactus_side(rng):
    img = new_tile()
    px = pixels(img)
    green = (62, 138, 58)
    for y in range(TILE):
        for x in range(TILE):
            c = shade(green, rng.randint(-8, 8) + (-24 if x % 8 == 0 else 0))
            if x % 8 == 4 and y % 6 == 2:
                c = (236, 230, 190)  # a spine
            px[x, y] = c + (255,)
    return img


def cactus_top(rng):
    img = new_tile()
    px = pixels(img)
    for y in range(TILE):
        for x in range(TILE):
            ring = max(abs(x - 15.5), abs(y - 15.5)) > 13
            px[x, y] = shade((76, 150, 66), rng.randint(-8, 8) - (20 if ring else 0)) + (255,)
    return img


def coral(base, rng):
    """Knobbly, bumpy coral."""
    img = new_tile()
    px = pixels(img)
    for y in range(TILE):
        for x in range(TILE):
            px[x, y] = shade(base, rng.randint(-14, 8)) + (255,)
    d = ImageDraw.Draw(img)
    for _ in range(14):
        x, y = rng.randint(0, 31), rng.randint(0, 31)
        d.ellipse([x - 2, y - 2, x + 2, y + 2], fill=shade(base, 30) + (255,),
                  outline=shade(base, -40) + (255,))
    return img


def torch(rng):
    """A stick with a flame on top, in the middle of a clear tile."""
    img = new_tile()
    d = ImageDraw.Draw(img)
    d.rectangle([15, 12, 16, 31], fill=(120, 84, 48, 255))
    d.ellipse([13, 5, 18, 13], fill=(250, 170, 40, 255))
    d.ellipse([14, 8, 17, 12], fill=(255, 236, 140, 255))
    return img


def cake_top(rng):
    img = new_tile()
    px = pixels(img)
    for y in range(TILE):
        for x in range(TILE):
            px[x, y] = shade((248, 244, 240), rng.randint(-6, 4)) + (255,)
    d = ImageDraw.Draw(img)
    for x, y in ((8, 8), (22, 10), (12, 22), (24, 24), (16, 15)):
        d.ellipse([x - 2, y - 2, x + 2, y + 2], fill=(216, 34, 52, 255))
    return img


def cake_side(cut):
    """Icing on top, sponge with a jam layer below; cut shows the inside."""
    def draw_tile(rng):
        img = new_tile()
        px = pixels(img)
        for y in range(TILE):
            for x in range(TILE):
                if y < 8:
                    c = (248, 244, 240)
                elif 18 <= y < 21:
                    c = (210, 40, 60)
                else:
                    c = (236, 200, 140) if cut else (226, 184, 120)
                px[x, y] = shade(c, rng.randint(-6, 4)) + (255,)
        return img
    return draw_tile


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
    ("bed_frame",           bed_frame),
    ("bed_pillow",          bed_pillow),
    # Row 4 from the third blanket on; the colours of the coloured blocks.
    ("bed_white",           lambda r: blanket((226, 226, 222), r)),
    ("bed_red",             lambda r: blanket((214, 46, 46), r)),
    ("bed_orange",          lambda r: blanket((240, 132, 30), r)),
    ("bed_yellow",          lambda r: blanket((246, 214, 46), r)),
    ("bed_green",           lambda r: blanket((84, 184, 60), r)),
    ("bed_blue",            lambda r: blanket((48, 104, 220), r)),
    ("bed_purple",          lambda r: blanket((142, 66, 196), r)),
    ("bed_pink",            lambda r: blanket((244, 140, 190), r)),
    ("glow_crystal_end",    crystal_end),
    # Farming (block/crop.hpp): soil, then each crop's four stages.
    ("farmland_dry_top",    farmland_top(FARMLAND_DRY)),
    ("farmland_wet_top",    farmland_top(FARMLAND_WET)),
    ("farmland_dry_side",   farmland_side(FARMLAND_DRY)),
    ("farmland_wet_side",   farmland_side(FARMLAND_WET)),
    ("crop_top",            crop_top),
] + [(f"wheat_{i}", wheat(i)) for i in range(4)] + [
    (f"carrot_{i}", carrot(i)) for i in range(4)] + [
    (f"strawberry_{i}", strawberry(i)) for i in range(4)] + [
    # Wild plants (block/plant.hpp order), then the blocks after them.
    ("tall_grass",          tall_grass),
    ("poppy",               flower((214, 40, 40), (40, 30, 20))),
    ("buttercup",           flower((250, 214, 40), (230, 150, 30))),
    ("cornflower",          flower((70, 110, 230), (30, 40, 120))),
    ("pink_tulip",          flower((244, 140, 190), (0, 0, 0), "tulip")),
    ("daisy",               flower((250, 250, 246), (246, 200, 40))),
    ("lavender",            flower((150, 100, 210), (0, 0, 0), "spike")),
    ("hibiscus",            flower((244, 120, 36), (250, 220, 80), "big")),
    ("fern",                fern),
    ("red_mushroom",        mushroom((210, 40, 40), True)),
    ("brown_mushroom",      mushroom((150, 106, 72), False)),
    ("dead_bush",           dead_bush),
    ("berry_bush",          berry_bush(True)),
    ("berry_bush_bare",     berry_bush(False)),
    ("shells",              shells),
    ("cactus_top",          cactus_top),
    ("cactus_side",         cactus_side),
    ("pink_coral",          lambda r: coral((236, 120, 170), r)),
    ("blue_coral",          lambda r: coral((60, 120, 226), r)),
    ("yellow_coral",        lambda r: coral((236, 200, 60), r)),
    ("torch",               torch),
    ("cake_top",            cake_top),
    ("cake_side",           cake_side(False)),
    ("cake_cut",            cake_side(True)),
    ("sandstone_bricks",    lambda r: bricks((214, 196, 140), (176, 154, 100), r)),
    ("snow_bricks",         lambda r: bricks((236, 242, 248), (190, 204, 220), r,
                                             h=16, bevel=True)),
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
    ap.add_argument("--only", nargs="+", metavar="NAME",
                    help="draw just these tiles, over any art in their cells")
    ap.add_argument("--png-dir", type=Path, help="also save each tile here")
    args = ap.parse_args()

    atlas = Image.open(ATLAS).convert("RGBA")
    if args.png_dir:
        args.png_dir.mkdir(parents=True, exist_ok=True)
    for i, (name, draw) in enumerate(TILES):
        if args.only and name not in args.only:
            continue
        col, row = cell(i)
        tile = draw(random.Random(f"{name}:{i}"))
        if args.png_dir:
            tile.save(args.png_dir / f"{name}.png")
        if not (args.force or args.only) and not is_empty(atlas, col, row):
            print(f"skip {name}: cell ({col}, {row}) already has art")
            continue
        atlas.paste(tile, (col * TILE, row * TILE))
        print(f"drew {name} at ({col}, {row})")
    atlas.save(ATLAS)


if __name__ == "__main__":
    main()
