# Tasks: items

Plan: [items-plan.md](items-plan.md) · Spec: [SPEC-items.md](../SPEC-items.md)

**Every task's definition of done:**
- `make -j"$(nproc)"` builds with no new warnings.
- `make test` passes.
- One commit per task.

---

## Task 1: Item definitions

**Description:** Add `ItemId`, `ItemIcon` and `ItemDef`, plus `itemForBlock`, `isValidItem` and `itemDef`.
- Block items (1..255) come from `blockName()` and the block's side tile in `block_data`.
- The 7 materials come from a static table with tiles (0..6, 0) in the item atlas.

**Acceptance criteria:**
- [ ] Every `BlockType` except AIR and WATER maps to a valid item whose `places` is that block.
- [ ] All 7 materials are valid, have names, have a max stack of 64, and have unique item-atlas tiles inside the 8×8 grid. They place nothing.
- [ ] `isValidItem` is false for NONE, WATER's ID, unused block values, and IDs past the last material.

**Verification:**
- [ ] `make test` passes with the new `item_test.cpp` cases.

**Dependencies:** None

**Files:** `inc/item/item.hpp`, `src/item/item.cpp`, `tests/item_test.cpp`

**Scope:** Small

---

## Task 2: Drop table

**Description:** Implement `dropFor(BlockType)` as specified.

**Acceptance criteria:**
- [ ] AIR, WATER and BEDROCK drop nothing.
- [ ] GRASS and SNOW_DIRT drop DIRT, and STONE drops COBBLESTONE.
- [ ] The 7 ores drop their material. Every other block drops itself, and every drop is a valid item.

**Verification:**
- [ ] `make test` passes, with a case per rule plus a loop over every block.

**Dependencies:** T1

**Files:** `inc/item/item.hpp`, `src/item/item.cpp`, `tests/item_test.cpp`

**Scope:** XS

---

## Task 3: Stack logic

**Description:** Add `ItemStack` (`empty`, `space`), `mergeInto` and `split`. Stacks are normalized to `{NONE, 0}` when empty.

**Acceptance criteria:**
- [ ] Merging into an empty, partial or full stack moves the right amount and leaves any remainder in the source.
- [ ] Different IDs never merge, and an emptied source becomes `{NONE, 0}`.
- [ ] `split` takes `min(n, count)`, a split of 0 returns an empty stack, and splitting everything empties the source.

**Verification:**
- [ ] `make test` passes with the stack cases.

**Dependencies:** T1

**Files:** `inc/item/item_stack.hpp`, `src/item/item_stack.cpp`, `tests/item_test.cpp`

**Scope:** Small

---

## Checkpoint: logic complete
- [ ] All tests pass, and the app builds and runs unchanged.

---

## Task 4: Placeholder item atlas

**Description:** Add `tools/gen_item_placeholders.py` (Pillow), which writes `res/item_atlas.png` at 256×256 RGBA with 32 px tiles. Material tiles go at (0..6, 0) as simple deterministic shapes: a dark lump for coal, coloured gems for diamond, emerald and ruby, and coloured nuggets for the raw ores. Everything else is transparent. Commit the PNG.

**Acceptance criteria:**
- [ ] The PNG is 256×256, and tiles 0..6 of row 0 are non-empty while the rest are transparent.
- [ ] Rerunning the script produces a pixel-identical image.
- [ ] `make` copies it to `bin/item_atlas.png`.

**Verification:**
- [ ] Run the script twice and compare the pixels.
- [ ] `ls bin/item_atlas.png` after `make`.
- [ ] Look at the PNG.

**Dependencies:** T1 (tile layout)

**Files:** `tools/gen_item_placeholders.py`, `res/item_atlas.png`

**Scope:** Small

---

## Task 5: Wrap-up

**Description:** Mark `items` as done in the capability map, update the spec status, open a PR, and merge once CI is green.

**Acceptance criteria:**
- [ ] All spec success criteria are met.
- [ ] CI is green.

**Dependencies:** T1–T4

**Files:** `CAPABILITY_MAP.md`, `SPEC-items.md`, `tasks/items-todo.md`

**Scope:** XS
