# Spec: hud

Module of [CAPABILITY_MAP.md](CAPABILITY_MAP.md). Depends on: `inventory`, `items`.
Health and stamina bars come later with `vitals`. This module only reserves
their space. (The build order was swapped on 2026-10-26: `hud` comes before
`vitals`.)

## Objective

Put the inventory on screen. Players see a hotbar at all times and can open
an inventory screen to rearrange items, with no F1 panel needed. Every
change still goes through the authoritative `Game` as `MoveItems`.

### Acceptance criteria
- **Hotbar:**
  - 10 slots, bottom centre, always visible during play.
  - Each slot shows the item's icon and a count when it's above 1. The selected slot is highlighted.
  - The numbers 1..9 and 0 are shown faintly in the corners.
- **Held item name:** when the selected slot or its item changes, the item's name appears above the hotbar for about 1.5 s, then fades.
- **Icons:**
  - Block items are drawn as a mini isometric cube from the block's top and side tiles, with the left face darker than the right.
  - Material items use their flat tile from the item atlas.
  - Colours match the world, because the textures are loaded as sRGB like the block atlas.
- **Inventory screen:** E opens and closes it. Esc also closes it instead of quitting while it's open. It shows the 3×10 storage above the 10-slot hotbar, and the crosshair is hidden.
- **Controls in the inventory screen:**
  - **Left-click** a stack to pick it up. It follows the cursor and its source slot is dimmed.
  - **Left-click a slot while holding:** sends `MoveItems{src, dst, 0}`, which moves, merges or swaps. The held stack is then cleared.
  - **Right-click a slot while holding:** sends `MoveItems{src, dst, 1}`. You keep holding whatever is left.
  - **Clicking the source slot,** clicking outside the grid, or closing the screen cancels. No move is sent.
  - **Shift + left-click** a stack: sends it to the other section (hotbar ↔ storage). It goes to the first stack of the same item with room, otherwise the first empty slot. If there's no room, nothing is sent.
  - **Tooltip:** the slot under the cursor shows the item's name.
- **Bar space:** a strip above the hotbar is left empty for `vitals` bars.
- **Scaling:** layout scales with window size (slot size = 6% of the window height, clamped to 32–80 px). It stays inside the window at 1280×720 and above.

## Design

```
Engine (client)
 ├─ HudLayout     (pure)  window size → slot rects, hit-test, bar strip
 ├─ InventoryScreen (pure) clicks + current slots → held state, Optional<MoveItems>
 ├─ icon geometry (pure)  ItemIcon → atlas UVs; slot rect → cube face quads
 └─ Hud           (GL/ImGui) owns block + item atlas textures; draws hotbar,
                             inventory screen, held stack, tooltip, name popup
```

- **Drawing uses ImGui draw lists,** like the crosshair: `AddImageQuad` for cube faces, `AddImage` for flat icons, and rects and text for slots and counts. No new shaders.
  - `Hud` loads its own copies of `res/block_atlas.png` and `res/item_atlas.png` as `GL_SRGB_ALPHA` with nearest filtering. The renderer stays untouched, at the cost of one extra 1 MB texture.
- **Input coordinates:** layout and hit-tests use ImGui's display and mouse coordinates (`io.DisplaySize`, `io.MousePos`), so drawing and clicking always agree, including on scaled displays.
- **Click handling is pure and tested:**
  - `InventoryScreen` turns click events into an optional `MoveItems` plus the held state. It reads slots from the client's `inventory_view` and never changes them.
  - If the inventory changes so the held source becomes empty, the pick-up is cancelled.
- **Engine wiring:**
  - While the inventory is open, `mouseButtonCallback` forwards clicks to `InventoryScreen` instead of breaking or placing.
  - Esc closes the inventory if it's open.
  - The F1 inventory list stays, as a debug view.

### Not in scope
- Health and stamina bars (`vitals`).
- Dropping items into the world.
- Drag-and-drop.
- Controller support.
- Hiding the HUD.
- Any change to what `Game` allows.

## Tech Stack / Commands

Unchanged: C++20, `make`, `make test`. Dear ImGui and stb are already vendored, and there are no new dependencies.

## Project Structure

```
inc/ui/hud_layout.hpp, src/ui/hud_layout.cpp          → rects, hit-test (pure)
inc/ui/inventory_screen.hpp, src/ui/inventory_screen.cpp → click state machine (pure)
inc/ui/item_icon_geometry.hpp, src/ui/item_icon_geometry.cpp → UVs, cube quads (pure)
inc/ui/hud.hpp, src/ui/hud.cpp                        → textures + drawing (GL/ImGui)
src/core/engine.cpp, inc/core/engine.hpp              → wiring, Esc, click routing
tests/hud_layout_test.cpp, tests/inventory_screen_test.cpp, tests/item_icon_test.cpp
```

## Code Style

Same conventions as the earlier specs. The pure classes take plain values
(window size, mouse position, `Inventory::Slots`) and return plain values,
with no ImGui types, so the tests don't need ImGui.

## Testing Strategy

- **`HudLayout`:**
  - Slot counts and order.
  - Slots don't overlap.
  - Everything fits inside 1280×720 and 3840×2160.
  - Slot size is clamped at very small and very large windows.
  - Hit-testing returns the slot under the cursor, at slot centres and edges and in gaps.
  - The bar strip sits above the hotbar.
- **`InventoryScreen`:**
  - Every rule from the acceptance criteria: pick up, drop, right-click one, cancel, shift-click to each section (merge target, empty target, no room).
  - Held state after each action.
  - Cancelling when the source empties.
- **Icon geometry:**
  - UVs for block tiles (16-wide atlas) and item tiles (8-wide).
  - Cube face quads stay inside the slot rect and the top face is above the side faces.
- **Manual check** (can't be automated: no display in CI):
  - The hotbar renders with correct icons and colours, and the selection follows keys and scroll.
  - The name popup appears.
  - E opens the screen, and each control works.
  - Esc closes the screen without quitting.
  - Resizing the window and going fullscreen (F11) keeps the layout fitted.

## Boundaries

- **Always:**
  - Every inventory change is a `MoveItems` sent to `Game`.
  - Keep the pure classes free of ImGui and GL.
  - Run `make test`.
- **Ask first:**
  - Changing controls or keybindings beyond E/Esc/clicks.
  - Adding fonts or art assets.
  - Changing `Game` rules.
- **Never:**
  - Change the client's `inventory_view` locally to predict a move.
  - Edit the block atlas.
  - Add a dependency.

## Success Criteria

1. All the pure-logic behaviour above is covered by passing tests.
2. The manual check passes on your machine, including in fullscreen.
3. FPS with the HUD visible is within 5% of `main` at spawn.
4. The build has zero new warnings and CI is green.

## Open Questions

None. Decisions so far: block icons are isometric mini cubes; the inventory
uses click-to-pick-up controls (left, right, shift); `hud` comes before
`vitals`.
