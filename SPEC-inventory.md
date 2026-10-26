# Spec: inventory

Module of [CAPABILITY_MAP.md](CAPABILITY_MAP.md). Depends on: `items`, `sim-core`.
Used by: `hud` (hotbar, inventory screen), `multiplayer` (per-player inventories).

**Status:** implemented. Criteria 1–3 and 5 are met; criterion 4 (manual play check) is up to you.

## Objective

Turn the game into survival: what you break goes into your inventory, and
placing a block uses one up. The inventory belongs to the authoritative
`Game`. Each client gets a read-only copy for display and aiming.

**What players will notice:**
- The hotbar has 10 slots on keys 1..0 instead of 9 fixed test blocks.
- Breaking a block collects its drop, and placing uses the held item.
- A new player starts with 8 glowstone.

Until `hud` is built, the inventory is visible in the F1 debug panel. The
panel also gets a "give item" control for testing and creative play.

### Acceptance criteria
- **Layout:** each player has 40 slots. Slots 0–9 are the hotbar (keys 1..9 and 0) and slots 10–39 are storage (3×10).
- **Breaking collects the drop:**
  - The drop (from `dropFor`) merges into existing stacks of that item first, hotbar before storage, then into the first empty slot, hotbar before storage.
  - If it doesn't fit, the block still breaks and the leftover is lost.
- **Placing uses the held item:**
  - It places the block item in the selected hotbar slot and consumes one.
  - An empty slot, or a non-block item, places nothing (rejected).
  - The client no longer chooses the block type.
- **Moving stacks:** `MoveItems {from, to, count}` behaves as follows.
  - `count == 0` means the whole stack.
  - Moving into an empty slot moves `count`.
  - Moving onto the same item merges up to the max stack size and leaves the remainder in `from`.
  - Moving a whole stack onto a different item swaps the two.
  - Moving part of a stack onto a different item is rejected.
  - `from == to`, out-of-range slots, or an empty source are rejected.
- **Starter kit:** a player with no saved inventory, whether in a new world or an old save, starts with 8 glowstone in hotbar slot 0.
- **Persistence:**
  - The host player's inventory is saved to a new `inventory.dat`.
  - `level.dat`, `player.dat` and `edits.dat` are unchanged, so old saves still load.
  - Slots holding item ids this build doesn't know load as empty.
- **Debug give:** the debug panel's "give item" sends `GiveItem {id, count}`, which adds the item like a drop would.

## Design

```
Engine (client)                         Game (authoritative)
───────────────                         ─────────────────────
inventory view (40 ItemStacks) ◄─────── InventoryContents{slots}  (after Join and
selected slot (mirror)                   after every change, to the owner only)
  │
  ├─ BreakBlock{pos} ─────────────────► break → dropFor → inventory.add (leftover lost)
  ├─ PlaceBlock{pos} ─────────────────► place held block item, consume 1
  ├─ MoveItems{from,to,count} ────────► inventory.move
  └─ GiveItem{id,count} (debug) ──────► inventory.add
```

```cpp
// inventory/inventory.hpp: pure logic, no Game or World dependency
constexpr int kHotbarSlots = 10;          // constants.hpp, was 9
constexpr int kStorageSlots = 30;
constexpr int kInventorySlots = kHotbarSlots + kStorageSlots;

class Inventory {
public:
  const ItemStack &slot(int i) const;
  const std::array<ItemStack, kInventorySlots> &slots() const;
  ItemStack add(ItemStack stack);                 // returns what didn't fit
  ItemStack take(int slot, uint16_t n);           // removes up to n
  bool move(int from, int to, uint16_t count);    // false = rejected, unchanged
  void set(int slot, ItemStack s);                // for loading
};
```

- **The protocol changes:**
  - `PlaceBlock` loses its `type` field.
  - New commands: `MoveItems` and `GiveItem`.
  - New event: `InventoryContents { std::array<ItemStack, 40> slots }`. A full snapshot is 160 bytes, small enough to resend on every change, so there's no delta logic to get wrong.
- **`PlayerState` gains an `Inventory`.** The client's `Player` loses `hotbar_blocks` and keeps only the selected-slot mirror.
- **Hotbar size:** `kHotbarSlots` goes from 9 to 10, and `SelectSlot` validation and scroll wrapping follow automatically. Key `0` selects slot 9.
- **The `inventory.dat` save format:**
  - Header: `VXIN` and version 1.
  - Then `uint16 slot_count`, followed by one `uint16 id, uint16 count` pair per slot.
  - Loading tolerates a different slot count: extra slots are dropped, and missing ones start empty.
  - `WorldSave` gains `loadInventory()` and `saveInventory()`. `save()` and the existing files are untouched.
- **Saved data and joining:** `Game::load` reads `inventory.dat` alongside `player.dat`. The first `Join` takes over both, as it already does for the saved player. Other joining players get a starter kit.
- **The starter kit** is defined in one place, `Game::starterKit()`.

### Not in scope
- Drawing the hotbar and inventory screen (`hud`).
- Dropped items on the ground when the inventory is full (`entities`).
- Crafting.
- Per-player saves for multiple players (`multiplayer`).

## Tech Stack / Commands

Unchanged from `SPEC-sim-core.md`: C++20, `make`, `make test`. No new dependencies.

## Project Structure

```
inc/inventory/inventory.hpp, src/inventory/inventory.cpp  → Inventory (pure logic)
inc/sim/protocol.hpp      → PlaceBlock{pos}; MoveItems; GiveItem; InventoryContents
inc/sim/player_state.hpp  → + Inventory
src/sim/game.cpp          → break collects, place consumes, move, give, kit, save/load
inc/world/world_save.hpp, src/world/world_save.cpp → loadInventory / saveInventory
inc/player/player.hpp, src/player/player.cpp → drop hotbar_blocks
src/core/engine.cpp       → key 0, inventory view, debug panel contents + give
tests/inventory_test.cpp  → Inventory logic
tests/game_test.cpp       → survival rules through Game
tests/world_save_test.cpp → inventory.dat round-trip and tolerance
```

## Code Style

Same as the earlier specs. `Inventory` is a small value type with no
knowledge of `Game` or `World`, like `ItemStack` in `items`.

## Testing Strategy

- **`Inventory` unit tests:**
  - `add`: fill order, merging before empty slots, hotbar before storage, partial fit, and returning the leftover.
  - `take`.
  - `move`: every rule listed in the acceptance criteria.
- **`Game` tests**, through `LocalConnection` and reusing the `EditGame` fixture:
  - Break collects the drop, including grass giving dirt and ore giving material.
  - A full inventory still breaks the block and the item is lost.
  - Place consumes from the selected slot, and is rejected with an empty slot or a non-block item.
  - `MoveItems` and `GiveItem` work.
  - `InventoryContents` arrives after `Join` and after each change, and only to the owner.
  - The starter kit appears on a fresh join.
- **Save tests:**
  - An `inventory.dat` round-trip.
  - A different slot count loads.
  - Unknown ids load as empty.
  - An old save with no `inventory.dat` gets the starter kit.
  - A full `Game` save → load keeps the inventory.
- **Manual check:**
  - Break a few blocks and watch the debug panel fill.
  - Place until a stack empties.
  - Use key 0.
  - Give glowstone.
  - Save, quit and relaunch.

## Boundaries

- **Always:**
  - Inventory changes happen only inside `Game`.
  - Keep `level.dat`, `player.dat` and `edits.dat` unchanged.
  - Run `make test`.
- **Ask first:**
  - Changing the slot counts or the starter kit.
  - Dropping items into the world.
  - Changing what full-inventory breaks do.
- **Never:**
  - Let the client choose what gets placed.
  - Break loading of existing saves.
  - Change `ItemId` values.

## Success Criteria

1. Every acceptance criterion is covered by a passing test.
2. `grep -n "hotbar_blocks" src inc` returns nothing, and `PlaceBlock` has no `type` field.
3. An existing save from `main` loads, gets the starter kit, and keeps its world edits and position.
4. Manual check: breaking and placing behave as survival in the debug panel view, key 0 works, and save/quit/relaunch keeps the inventory.
5. The build has zero new warnings and CI is green.

## Open Questions

None. Decisions so far: a full inventory loses the item but the block
still breaks; the starter kit is 8 glowstone; the layout is a 10-slot
hotbar plus 3×10 storage.
