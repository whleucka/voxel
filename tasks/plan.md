# Implementation Plan: sim-core

Spec: [SPEC-sim-core.md](../SPEC-sim-core.md) · Map: [CAPABILITY_MAP.md](../CAPABILITY_MAP.md)
Tasks: [todo.md](todo.md)

## Overview

This plan refactors the single-player engine into an authoritative `Game`
(the simulation) and an `Engine` (the client). They talk through a
`Connection` that carries `Command`s and `Event`s. Gameplay must not change.
Every task leaves the game buildable and playable.

## Architecture Decisions

- **Tests first, against the current behaviour.** The test harness and
  characterization tests of `World::setBlockAt`, edits and save land before
  anything moves. The refactor is then checked against behaviour that has
  already been pinned down.
- **Tests run without a window.** A `World` that is never `init()`ed makes no
  GL calls: meshing runs on the CPU and the GPU upload only happens in
  `World::update()`. Tests construct `World`/`Game` directly and link the same
  libs as the app, so no display is needed. Async chunk generation needs a
  synchronous test hook (`World::loadChunkSync`).
- **Commands apply at once; the tick is only for time.** `Game::pump()`
  drains the connection every frame, so breaking and placing blocks has no
  added latency. `Game::tick()` runs at a fixed 20 Hz from an accumulator and
  advances time-based state: the clock now, vitals and entities later.
- **Spawn search moves out of `World::init()`** into
  `World::findSpawn() -> glm::vec3`, so `Game` decides where players appear.
- **`World::render` takes the view state as parameters**: camera frustum
  planes, eye position, and whether the player is underwater. It no longer
  reads a member player.
- **The `GameClock` moves into `Game`.** The renderer reads
  `game.clock().fractionOfDay()`. In multiplayer, syncing the clock becomes
  an event.

## Dependency Graph

```
T1 test harness
 └─ T2 characterization tests (World edits, save round-trip)
     └─ T3 World without Player (findSpawn, interest points, render params)
         └─ T4 protocol + Connection + Game skeleton (Join, clock, tick)
             ├─ T5 MovePlayer + SelectSlot → PlayerState drives streaming
             │   └─ T6 Break/PlaceBlock in Game + BlockChanged
             └─ T7 Save/load owned by Game
                 └─ T8 parity + perf pass
```

The tasks run in order (T5 and T7 could swap). Every task touches shared
central files (`engine.cpp`, `world.cpp`), so parallel work isn't worth it.

## Task List

See [todo.md](todo.md) for acceptance criteria and verification.

### Phase 1: Safety net
- [ ] T1: Vendor doctest; add `make test`; add a CI step
- [ ] T2: Characterization tests for World edits and save round-trip

### Checkpoint A: Tests green locally and in CI; game unchanged

### Phase 2: Decouple
- [ ] T3: World no longer owns Player
- [ ] T4: Protocol, Connection, LocalConnection, Game skeleton

### Checkpoint B: Manual parity check passes; human review of the new structure

### Phase 3: Route state through Game
- [ ] T5: MovePlayer and SelectSlot commands
- [ ] T6: BreakBlock and PlaceBlock validated in Game
- [ ] T7: Game owns save/load

### Checkpoint C: All spec success criteria met (T8)
- [ ] T8: Parity, performance and warnings pass; update docs

## Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Tests accidentally call GL (e.g. a `ChunkMesh` dtor after an upload) and segfault without a context | Med | Tests never call `World::update()`/`init()`. The dtors already guard on non-zero handles. Add a comment noting that tests rely on this. |
| Async chunk generation makes tests flaky | Med | `loadChunkSync` generates on the calling thread. Tests never touch the thread pools. |
| Moving `Player` out of `World` misses a coupling (camera, underwater fog, shadows follow the player) | Med | T3 is a pure move with the parity checklist run right after. Grep for `player->` in `world.cpp` must return nothing. |
| A command queue reorders input relative to today (e.g. place then select slot in the same frame) | Low | `LocalConnection` is FIFO and drained every frame before render. |
| The fixed-tick clock makes the sun move in visible steps | Low | 1.75 in-game seconds per tick is invisible. If it isn't, interpolate `fractionOfDay` with the accumulator alpha. |
| Perf regression from per-frame `MovePlayer` structs or variant dispatch | Low | Measure FPS at spawn against `main` in T8. The budget is 5%. |

## Open Questions

None. The spec's questions were answered: doctest is OK, 20 Hz is OK, and
the save format stays unchanged.
