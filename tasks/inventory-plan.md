# Implementation Plan: inventory

Spec: [SPEC-inventory.md](../SPEC-inventory.md) · Tasks: [inventory-todo.md](inventory-todo.md)

## Overview

Add a pure `Inventory` type, save it in `inventory.dat`, and make `Game`
apply survival rules: breaking collects, placing consumes, plus moving,
giving and a starter kit. Then switch the client over to an inventory view.
Every task leaves the game playable.

## Architecture Decisions

- **`Inventory` is a value type in `inc/inventory/`** with no `Game` or `World`
  dependency, so its rules are unit-tested in isolation, like `ItemStack`.
- **Full snapshots, no deltas.** `InventoryContents` sends all 40 slots
  (160 bytes) to the owner after any change, so there's no delta logic to
  get wrong.
- **`inventory.dat` is a separate file,** so the existing save files and
  their readers are untouched. Unknown item ids load as empty.
- **Changing `kHotbarSlots` to 10 happens in T1.** `Player::hotbar_blocks`
  is resized to match, with slot 9 empty, until T5 removes it.

## Tasks

- [ ] T1: `Inventory` type and `kHotbarSlots = 10`
- [ ] T2: `inventory.dat` load/save
- [ ] T3: `Game`: break collects, place consumes, `InventoryContents`, starter kit
- [ ] T4: `Game`: `MoveItems`, `GiveItem`, inventory in `Game` save/load
- [ ] T5: Client: inventory view, key 0, debug panel, remove `hotbar_blocks`
- [ ] T6: Wrap-up, PR, merge when green

Checkpoint after T3: the game plays as survival (verified by tests), and
the client still builds.

## Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Between T3 and T5 the client's hotbar doesn't match what `Game` places | Low | It's only intermediate, and T5 lands in the same PR. |
| Old saves | Med | A test loads a save with no `inventory.dat`. Manual check on your real save. |
