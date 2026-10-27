"""Save PNG tiles as GIMP .xcf sources, for repainting in GIMP.

Runs inside GIMP 3's Python, not on its own:

  gimp-console -i --quit --batch-interpreter=python-fu-eval \
    -b "import sys; sys.argv=['png_to_xcf', 'PNG_DIR', 'OUT_DIR']; exec(open('tools/png_to_xcf.py').read())"

Every PNG_DIR/<name>.png becomes OUT_DIR/<name>.xcf.

With a third argument, ATLAS_XCF, each tile is also added to that layered
atlas as its own layer, at the cell gen_building_tiles.py gives it.
"""

import sys
from pathlib import Path

from gi.repository import Gimp, Gio  # pyright: ignore  (GIMP's own Python)

png_dir, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
for png in sorted(png_dir.glob("*.png")):
    xcf = out_dir / (png.stem + ".xcf")
    image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(str(png)))
    image.get_layers()[0].set_name(png.stem)
    Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, Gio.File.new_for_path(str(xcf)), None)
    image.delete()
    print(f"wrote {xcf}")

if len(sys.argv) > 3:
    sys.path.insert(0, "tools")  # run from the repo root, as shown above
    from gen_building_tiles import TILE, TILES, cell

    atlas_file = Gio.File.new_for_path(sys.argv[3])
    atlas = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, atlas_file)
    names = {layer.get_name() for layer in atlas.get_layers()}
    for i, (name, _) in enumerate(TILES):
        col, row = cell(i)
        if name in names:
            print(f"skip {name}: the atlas already has that layer")
            continue
        png = Gio.File.new_for_path(str(png_dir / f"{name}.png"))
        layer = Gimp.file_load_layer(Gimp.RunMode.NONINTERACTIVE, atlas, png)
        layer.set_name(name)
        atlas.insert_layer(layer, None, 0)
        layer.set_offsets(col * TILE, row * TILE)
        print(f"added layer {name}")
    Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, atlas, atlas_file, None)
    atlas.delete()
