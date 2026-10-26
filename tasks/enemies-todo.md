# Tasks: enemies

Plan: [enemies-plan.md](enemies-plan.md) · Spec: [SPEC-enemies.md](../SPEC-enemies.md)

**Every task's definition of done:**
- `make` builds with no warnings.
- `make test` passes.
- One commit per task.

## T1: Difficulty
- [ ] `difficulty = peaceful|easy|normal` parses, defaults to easy, and round-trips through `voxel.properties`.
- [ ] Easy halves caps and damage (at least 1) and moves at 80% speed. Normal is unchanged.

## T2: Pathfinding
- [ ] Finds a straight line, a way around a wall, and a way up a 1-block step.
- [ ] Won't climb 2 blocks. Drops 3 but not 4. Needs headroom.
- [ ] No path, the node limit and the range limit each return nothing.

## T3: Brains
- [ ] `BrainContext` replaces the brain arguments, and the animal brain tests pass unchanged.
- [ ] `ChaseBrain` reaches a player 10 blocks away on flat ground, and one behind a wall through a gap.
- [ ] With no player in range it wanders.

## T4: Enemy kinds
- [ ] Gloomling, Thorn Crawler and Cave Grumble have the spec's sizes, speeds, health, damage and models with glowing eyes.
- [ ] They can be hit. Killing one drops glowstone (2 for the Grumble), plus coal 50% of the time (seeded).

## T5: Spawning
- [ ] The Gloomling spawns only at night, on the surface, in the dark, and not next to glowstone.
- [ ] The Crawler spawns only in its biomes. The Grumble spawns only underground in the dark, at any time.
- [ ] Cave placement finds pockets and never the surface. Peaceful spawns nothing, and Easy caps are halved.

## T6: Game
- [ ] Contact damage with a per-enemy cooldown, and `KnockedBack` goes to the right player.
- [ ] Easy halves damage (at least 1). Peaceful removes enemies. Surface enemies vanish at sunrise, Grumbles don't.
- [ ] Dying to an enemy respawns you with your items.

## T7: Client
- [ ] `KnockedBack` pushes the player. F1 has a difficulty picker and enemy spawn buttons. Eyes glow.
- [ ] Manual check (user): the spec's night, cave, light, sunrise and Peaceful checks.

## T8: Wrap-up
- [ ] Map and spec status are updated, CI is green, and the PR is merged.
