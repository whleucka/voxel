# Implementation Plan: entities

Spec: [SPEC-entities.md](../SPEC-entities.md) · Tasks: [entities-todo.md](entities-todo.md)

## Architecture Decisions

- **Characterize, then extract.** `Player` physics is pinned down by
  tests that record exact numbers at a fixed 60 Hz timestep, run on blocks
  placed high in the sky so the terrain can't interfere. Only then does it
  move into `PhysicsBody`.
- **`PhysicsBody` is a plain struct with `step(dt, const World*)`.**
  `Player` keeps input handling (`processKeyboard`) and the camera.
  Crouching changes the body's height.
- **Entities run on the fixed 50 ms tick** inside `Game`, so their
  behaviour is deterministic and testable. The client interpolates
  between snapshots for smooth drawing.
- **Full snapshots, no deltas,** to the owner of each connection. Pure
  interpolation on the client.
- **One shader and one unit-cube VBO for every box.** Each box is a model
  matrix, plus a colour or atlas tile per face. Small entity counts mean
  per-box draw calls are fine.

## Tasks
- [x] T1: Player physics characterization tests
- [x] T2: Extract `PhysicsBody`; `Player` uses it
- [x] T3: Entity core: kinds and defs, store, tick with physics, despawn, `WanderBrain`
- [x] T4: Snapshots in `Game`, client `EntityInterpolator`, ray vs AABB
- [x] T5: `EntityRenderer` (shader, boxes, atlas faces), drawn in `Engine`
- [x] T6: Dropped items: overflow, Q/Ctrl+Q, pickup, merge, age despawn
- [x] T7: Hitting: `AttackEntity`, cooldown, knockback, hurt flash, death drops; client entity ray
- [x] T8: Spawner and spawn rules; debug "Spawn test creature"
- [x] T9: Wrap-up, PR, merge when green
