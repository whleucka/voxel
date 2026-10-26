# Implementation Plan: enemies

Spec: [SPEC-enemies.md](../SPEC-enemies.md) · Tasks: [enemies-todo.md](enemies-todo.md)

## Architecture Decisions
- **Difficulty is a pure core enum.** `inc/core/difficulty.hpp` holds
  `Difficulty`, the name parse/format helpers, and the Easy scaling
  (`scaledCap`, `scaledDamage`, `speedScale`). `Settings` stores one, and
  `Game` reads `g_settings.difficulty` every tick, so an F1 change takes
  effect immediately.
- **Pathfinding is a pure function over `World`.** `findPath` is a grid A*
  (4 neighbours, up 1 or down at most 3, node and range limits) with no
  entity knowledge, so rigs in the sky can test it exhaustively.
- **Brains get a `BrainContext`.** It holds the world, the RNG and the
  positions of living players. `WanderBrain` keeps its behaviour, and
  `ChaseBrain` wraps one as its fallback when no player is in range.
- **Enemies are ordinary entity kinds with an `EnemyDef`.** Hitting them uses
  the existing attack path. Death drops now include `chance_drops`, rolled on
  the Game's seeded RNG.
- **The spawner learns placement and hostility.** `SpawnRule` gains
  `Placement {Surface, Cave}` and `hostile`. For hostile rules the spawner
  skips Peaceful and scales caps by difficulty. The cave search is a pure
  per-column scan below the surface.
- **The Game owns all enemy outcomes.** Contact damage, the per-enemy
  cooldown, sunrise vanishing and Peaceful removal run in `Game::tick`.
  Knockback reaches the client as a `KnockedBack` event, and the client's
  `Player` keeps it for a short time before input steers again.
- **Glow is a per-box flag.** The entity shader gains `uGlow`, which skips
  lighting for that box.

## Tasks
- [x] T1: Difficulty setting (enum, scaling, properties key, tests)
- [x] T2: Pathfinding (`findPath`, tests)
- [ ] T3: `BrainContext` and `ChaseBrain` (tests)
- [ ] T4: Enemy kinds, models with glow, chance drops on death
- [ ] T5: Spawning: cave placement, predicates, hostile rules, difficulty caps
- [ ] T6: Game: contact attacks, `KnockedBack`, sunrise, Peaceful, speed scaling
- [ ] T7: Client: knockback, F1 difficulty and spawn buttons, glowing boxes
- [ ] T8: Wrap-up, PR, merge when green

### Checkpoints
- After T3: tests pass; existing animals behave the same (animal tests unchanged).
- After T6: every rule in the spec has a passing test.

## Risks and Mitigations
| Risk | Impact | Mitigation |
|---|---|---|
| A* cost with many enemies | Med | Node cap of 600, re-plan at most once a second per enemy. |
| Changing the brain signature breaks animals | Med | Animal brain tests run unchanged after T3. |
| Sunrise and Peaceful removing test entities | Low | Tests pin the difficulty and time of day with scoped guards. |
| Knockback fighting client input | Low | A short knockback timer stops input overwriting horizontal velocity. |
