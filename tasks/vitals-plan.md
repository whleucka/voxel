# Implementation Plan: vitals

Spec: [SPEC-vitals.md](../SPEC-vitals.md) · Tasks: [vitals-todo.md](vitals-todo.md)

## Architecture Decisions

- **Integer milliseconds inside `Vitals`.** Every rate is "ms per point",
  accumulated from whole-millisecond ticks, so the 50 ms ticks add up
  exactly and the timing tests can expect a change on a specific tick.
- **`FallTracker` measures from the last ground height** (or a higher
  airborne point), not from the first airborne sample. Otherwise walking
  off a 4-block ledge would measure 3.99 and do no damage.
- **`Game` sends `VitalsChanged` only when a shown value changes.** It
  compares with the last snapshot sent to that player.

## Tasks
- [x] T1: `Vitals` and `FallTracker` (pure) with tests
- [x] T2: Protocol and `Game` integration: falls, tick, death and respawn
- [x] T3: Client: `MovePlayer` flags, sprint allowed, respawn, input while dead
- [x] T4: HUD: breath strip, hearts, bolts, bubbles, damage flash, death overlay
- [x] T5: Wrap-up, PR, merge when green
