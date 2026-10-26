# Implementation Plan: hud

Spec: [SPEC-hud.md](../SPEC-hud.md) · Tasks: [hud-todo.md](hud-todo.md)

## Overview

Build the pure, tested pieces first: layout, icon geometry and the
inventory click state machine. Then add the ImGui/GL drawing and the
`Engine` wiring on top. Drawing can only be checked by eye, so it's kept
thin.

## Architecture Decisions

- **The pure classes take plain values** (window size, mouse position,
  `Inventory::Slots`), so tests need neither ImGui nor GL.
- **`Hud` owns its own atlas textures** (sRGB, nearest filtering) instead of
  reaching into `World`/`Renderer`.
- **Draw and click in the same coordinates.** Both use ImGui display and
  mouse coordinates.

## Tasks

- [x] T1: `HudLayout`: slot rects, hit-test, bar strip
- [x] T2: Icon geometry: atlas UVs and cube face quads
- [x] T3: `InventoryScreen` click state machine
- [x] T4: `Hud` drawing for the hotbar and name popup, wired into `Engine`
- [x] T5: Inventory screen drawing, click routing, Esc to close
- [x] T6: Wrap-up, PR, merge when green

Checkpoint after T3: the pure logic is done and tested.

## Risks

| Risk | Impact | Mitigation |
|---|---|---|
| The drawing can't be checked by CI or by me | Med | Keep the drawing thin over tested geometry. You do the manual check. |
| Colours drift if the HUD textures aren't sRGB | Low | Load them as `GL_SRGB_ALPHA`, like the block atlas. |
| ImGui's own colours are gamma-shifted by `GL_FRAMEBUFFER_SRGB` | Low | The crosshair already lives with this. Pick slot colours by eye. |
