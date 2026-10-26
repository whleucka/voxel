# Tasks: inventory

Plan: [inventory-plan.md](inventory-plan.md) · Spec: [SPEC-inventory.md](../SPEC-inventory.md)

**Every task's definition of done:**
- `make` builds with no warnings.
- `make test` passes.
- One commit per task.

## T1: Inventory type
- [x] `add` fills matching stacks first, then empty slots, hotbar before storage, and returns the leftover.
- [x] `take` and `move` follow every rule in the spec.
- [x] `kHotbarSlots == 10`, and the app builds.

Files: `inc/inventory/inventory.hpp`, `src/inventory/inventory.cpp`, `inc/core/constants.hpp`, `inc/player/player.hpp`, `tests/inventory_test.cpp`

## T2: inventory.dat
- [x] Round-trip works, a different slot count is tolerated, and unknown ids load as empty.
- [x] A missing file loads as false.

Files: `inc/world/world_save.hpp`, `src/world/world_save.cpp`, `tests/world_save_test.cpp`

## T3: Survival rules in Game
- [x] Breaking collects `dropFor` (lost if full). Placing consumes the selected slot's block item and rejects an empty slot or a non-block item.
- [x] `PlaceBlock` has no `type`. `InventoryContents` goes to the owner only, after `Join` and after each change.
- [x] A fresh join gets 8 glowstone in slot 0.

Files: `inc/sim/protocol.hpp`, `inc/sim/player_state.hpp`, `inc/sim/game.hpp`, `src/sim/game.cpp`, `src/core/engine.cpp`, `tests/game_test.cpp`

## Checkpoint: survival rules green in tests; the app builds

## T4: Move, give, persistence
- [x] `MoveItems` and `GiveItem` work through `Game`.
- [x] `Game` save → load keeps the host's inventory, and an old save with no `inventory.dat` gets the starter kit.

Files: `inc/sim/protocol.hpp`, `src/sim/game.cpp`, `inc/sim/game.hpp`, `tests/game_test.cpp`

## T5: Client
- [x] The client keeps an inventory view from `InventoryContents`, and key 0 selects slot 9.
- [x] The debug panel shows the hotbar and storage with counts, and has "give item".
- [x] `grep hotbar_blocks` returns nothing.

Files: `inc/core/engine.hpp`, `src/core/engine.cpp`, `inc/player/player.hpp`, `src/player/player.cpp`

## T6: Wrap-up
- [x] Map and spec status are updated, CI is green, and the PR is merged.
- [ ] Manual check (user): break, place, key 0, give, save, quit, relaunch, and load an old save.
