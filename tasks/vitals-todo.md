# Tasks: vitals

Plan: [vitals-plan.md](vitals-plan.md) · Spec: [SPEC-vitals.md](../SPEC-vitals.md)

**Every task's definition of done:**
- `make` builds with no warnings.
- `make test` passes.
- One commit per task.

## T1: Vitals and FallTracker
- [x] Stamina drains, recovers and exhausts with the threshold of 6. Fly mode doesn't drain it.
- [x] Breath drains and refills. Drowning does 2 damage per second at 0 breath.
- [x] Regen starts after a 4 s delay that damage resets. Values clamp. Death happens at 0, and `reset` restores everything.
- [x] Fall damage is `floor(distance − 3)` from the peak or ground height. Water and fly mode are safe.

## T2: Game integration
- [x] A fall in `MovePlayer` damages the owner, and drowning and regen run over ticks.
- [x] Death ignores commands. After 3 s the player gets `Respawned` at spawn with full vitals and their inventory kept.
- [x] `VitalsChanged` goes to the owner only: after `Join`, and whenever a value changes.

## T3: Client
- [x] `MovePlayer` carries `on_ground`, `sprinting`, `in_water` and `eyes_underwater`.
- [x] Sprint is blocked while exhausted. `Respawned` teleports the player and resets velocity. Input is ignored while dead.

## T4: HUD
- [x] `breathStrip()` sits between the bar strip and the name area (tested).
- [x] Draws hearts, bolts (with halves) and bubbles, plus the damage flash and the death overlay.
- [ ] Manual check (user): fall, drown, sprint until exhausted, die and respawn, regen.

## T5: Wrap-up
- [x] Map and spec status are updated, CI is green, and the PR is merged.
