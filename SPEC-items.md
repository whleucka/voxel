# Spec: items

Module of [CAPABILITY_MAP.md](CAPABILITY_MAP.md). Depends on: nothing.
Used by: `inventory`, `hud`, `animals`, and later `vitals`/`enemies` (which add items).

## Objective

Give the game a notion of *things you can hold*. Every module that stores,
shows or drops items uses one definition of an item, a stack and a drop.

This module is data and logic only. **Nothing changes on screen or in
gameplay.** It's built so that the following can use it directly:
- `inventory` collects drops and moves stacks around.
- `hud` draws icons, names and counts.
- later modules add items (food, a weapon) by appending to one table.

### Acceptance criteria
- **Block items:** every placeable block (all `BlockType`s except AIR and WATER) has an item that places it.
- **Material items:** 7 non-block items are COAL, DIAMOND, EMERALD, RUBY, RAW_IRON, RAW_GOLD and RAW_COPPER.
- **Drops:** breaking a block yields a defined drop.
  - Most blocks drop themselves.
  - GRASS and SNOW_DIRT drop DIRT, and STONE drops COBBLESTONE.
  - Each ore drops its material.
  - AIR, WATER and BEDROCK drop nothing.
- **Stacks:** stack logic (merge, split, overflow) is correct against the max stack size, which is 64 for all current items.
- **Item properties:** every item has a display name, a max stack size and an icon. Block items use their existing block-atlas tile. Material items use a tile in a new item atlas with placeholder art.
- **Stable IDs:** item IDs are stable numbers that are safe to save, following the same append-only rule as `BlockType`.

## Design

```cpp
// item/item.hpp
enum class ItemId : uint16_t {
  NONE = 0,                // == BlockType::AIR; the empty item
  // 1..255 are block items: ItemId(n) places BlockType(n). Not enumerated.
  COAL = 256,
  DIAMOND, EMERALD, RUBY, RAW_IRON, RAW_GOLD, RAW_COPPER,
  // Append only: the numeric value is persisted in inventory saves.
};

struct ItemIcon {
  enum class Atlas : uint8_t { Block, Item } atlas;
  glm::ivec2 tile;          // 32 px tile coords in that atlas
};

struct ItemDef {
  std::string_view name;
  uint16_t max_stack;       // 64 for everything today; 1 for future tools
  ItemIcon icon;
  std::optional<BlockType> places; // set for block items
};

ItemId itemForBlock(BlockType b);            // a cast; NONE for AIR
bool isValidItem(ItemId id);                 // known to this build
const ItemDef &itemDef(ItemId id);           // precondition: isValidItem
std::optional<ItemId> dropFor(BlockType b);  // drop table; count is always 1

// item/item_stack.hpp
struct ItemStack {
  ItemId id = ItemId::NONE;
  uint16_t count = 0;
  bool empty() const;                      // NONE or count == 0
  uint16_t space() const;                  // room left before max_stack
};
// Move as much of src into dst as fits; returns how many moved. dst may be
// empty (adopts src's id). Different ids don't merge.
uint16_t mergeInto(ItemStack &dst, ItemStack &src);
// Remove up to n from s and return them as a new stack.
ItemStack split(ItemStack &s, uint16_t n);
```

- **Block items are a cast.** `ItemId` 1..255 *is* the `BlockType` value. Existing block names and atlas tiles are reused, and a new block automatically gets an item. One rule protects both: values are append-only.
- **One definition table.** A static table defines the non-block items, and `itemDef` builds block-item definitions from `blockName()` and `block_data`. For a block's icon, `ItemIcon` uses the *side* tile, and `hud` decides whether to draw it flat or as a cube.
- **Normalized stacks.** A stack with count 0 is always reset to `{NONE, 0}`, so "empty" has one representation.
- **Item atlas.** `res/item_atlas.png` is a 256×256 image of 32 px tiles (8×8 = 64 slots). It's copied to `bin/` like the block atlas.
  - Material items take tiles (0..6, 0) in the order of the `ItemId` list.
  - The placeholders are simple coloured shapes: a black lump for coal, and coloured gems or nuggets for the rest.
  - The placeholder generator is a small Python script, `tools/gen_item_placeholders.py`.
  - Real art replaces the PNG tiles in place, with no code change, and its sources go in `assets/items/*.xcf` like the block sources.
  - Nothing loads the item atlas yet; that's `hud`'s job.
- **Not in scope:** collecting drops (`inventory`), dropped-item entities in the world (`entities`), icon rendering (`hud`), tools, food and crafting.

## Tech Stack

C++20 and the existing libraries. No new dependencies. Python 3 with Pillow
is used only by the placeholder-art script, which is a dev tool and not part
of the build.

## Commands

```
Build:   make -j"$(nproc)"
Test:    make test
Art:     python3 tools/gen_item_placeholders.py   # regenerates res/item_atlas.png
```

## Project Structure

```
inc/item/item.hpp, src/item/item.cpp     → ItemId, ItemDef, itemDef, dropFor
inc/item/item_stack.hpp, src/item/item_stack.cpp → ItemStack, mergeInto, split
res/item_atlas.png                       → placeholder icons (copied to bin/)
tools/gen_item_placeholders.py           → regenerates the placeholder atlas
tests/item_test.cpp                      → definitions, drop table, stack logic
```

## Code Style

Same conventions as `SPEC-sim-core.md`. Lookups are `switch`es or static
tables, like `block_data.hpp`, rather than class hierarchies.

```cpp
std::optional<ItemId> dropFor(BlockType b) {
  switch (b) {
    case BlockType::AIR:
    case BlockType::WATER:
    case BlockType::BEDROCK:     return std::nullopt;
    case BlockType::GRASS:
    case BlockType::SNOW_DIRT:   return itemForBlock(BlockType::DIRT);
    case BlockType::STONE:       return itemForBlock(BlockType::COBBLESTONE);
    case BlockType::COAL_ORE:    return ItemId::COAL;
    // ... other ores → their material
    default:                     return itemForBlock(b);
  }
}
```

## Testing Strategy

doctest unit tests in `tests/item_test.cpp`. This module is pure logic, so it
needs no world and no GL.

- **Every block:**
  - Every `BlockType` except AIR and WATER has a valid item that places it.
  - Every `dropFor` result is a valid item.
  - No placeable block drops itself as the wrong type.
- **Every material item:** each has a name, a max stack of 64, and an icon in the item atlas within the 8×8 grid. No two items share an item-atlas tile.
- **Drop table:** the rules from the acceptance criteria, case by case.
- **Stack logic:**
  - Merging into an empty stack, a partial stack and a full stack.
  - Overflow leaves the remainder in the source.
  - Different IDs don't merge.
  - Split, including more than the count and 0.
  - Normalization to `{NONE, 0}`.
- **Unknown IDs:** `isValidItem` is false for IDs above the last known value and for IDs that aren't block values.

## Boundaries

- **Always:**
  - Keep `ItemId` and `BlockType` values append-only.
  - Keep every non-block item in the definition table.
  - Run `make test`.
- **Ask first:**
  - Changing a max stack size.
  - Changing which blocks drop what.
  - Adding items beyond the 7 materials.
  - Changing the item atlas tile size or layout.
- **Never:**
  - Renumber existing IDs.
  - Change on-screen behaviour or gameplay in this module.
  - Edit `res/block_atlas.png`.

## Success Criteria

1. All acceptance criteria above are covered by passing tests in `tests/item_test.cpp`.
2. `res/item_atlas.png` exists at 256×256 with 7 placeholder tiles, and `make` copies it to `bin/`.
3. Running the placeholder script regenerates an identical PNG (byte-for-byte or pixel-identical).
4. There is no gameplay or rendering change: the game builds and runs as before, and `Engine`/`Game` are untouched.
5. The build has zero new warnings and CI is green.

## Open Questions

None. Decisions so far: ores drop material items, the max stack is 64, and
the icons are placeholders until real art is drawn.
