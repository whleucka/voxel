# Tasks: hud

Plan: [hud-plan.md](hud-plan.md) · Spec: [SPEC-hud.md](../SPEC-hud.md)

**Every task's definition of done:**
- `make` builds with no warnings.
- `make test` passes.
- One commit per task.

## T1: HudLayout
- [x] Slot size is 6% of the window height, clamped to 32–80 px.
- [x] The hotbar is bottom centre, the storage grid (3×10) sits above the inventory-screen hotbar, and nothing overlaps or leaves the window at 1280×720 or 4K.
- [x] `hitTest` returns the right slot or none, and the bar strip sits above the hotbar.

Files: `inc/ui/hud_layout.hpp`, `src/ui/hud_layout.cpp`, `tests/hud_layout_test.cpp`

## T2: Icon geometry
- [x] Tile UVs are correct for the 16-wide block atlas and the 8-wide item atlas.
- [x] Cube faces stay within the slot, and the top face sits above the side faces.

Files: `inc/ui/item_icon_geometry.hpp`, `src/ui/item_icon_geometry.cpp`, `tests/item_icon_test.cpp`

## T3: InventoryScreen
- [x] Pick up, drop, right-click one, cancel and shift-click behave as the spec says.
- [x] The pick-up is cancelled when the held source becomes empty.

Files: `inc/ui/inventory_screen.hpp`, `src/ui/inventory_screen.cpp`, `tests/inventory_screen_test.cpp`

## Checkpoint: pure logic complete

## T4: Hotbar drawing
- [x] `Hud` loads both atlases as sRGB and draws the hotbar with icons and counts, plus the name popup.
- [x] Manual check (user): the hotbar looks right and the selection follows keys and scroll.

Files: `inc/ui/hud.hpp`, `src/ui/hud.cpp`, `inc/core/engine.hpp`, `src/core/engine.cpp`

## T5: Inventory screen
- [x] E toggles the screen, clicks are routed to `InventoryScreen`, moves are sent, and Esc closes the screen while it's open.
- [x] Manual check (user): every control works, including after resizing and in fullscreen.

Files: `src/ui/hud.cpp`, `src/core/engine.cpp`, `inc/core/engine.hpp`

## T6: Wrap-up
- [x] Map and spec status are updated, CI is green, and the PR is merged.
- [x] Manual check (user): FPS with the HUD is within 5% of `main`.
