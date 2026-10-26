# Implementation Plan: animals

Spec: [SPEC-animals.md](../SPEC-animals.md) · Tasks: [animals-todo.md](animals-todo.md)

## Architecture Decisions
- **Animals are ordinary entity kinds.** Each has an `AnimalDef` (favourite,
  product, biomes, ground, can_swim) and `max_health = 0`, so the existing
  attack check already rejects them.
- **Spawn rules come from the animal table.** `Game` registers them
  (`registerAnimalSpawns`). The placement check is pure: biome of the
  column, the ground block, and daylight.
- **Item icons wrap onto new atlas rows.** Material tiles are laid out as
  `(i % 8, i / 8)`, so more than 8 materials still fit the 8×8 atlas.
- **Leg swing and hearts are client-side visuals.** They come from two
  snapshot flags: `moving` (set by the interpolator) and `happy` (from `Game`).

## Tasks
- [ ] T1: New items (WOOL, BERRIES, SEA_SHELL, SWEET_PETALS), `food_heal`, icons
- [ ] T2: `WanderBrain` options: cautious (water and drops) and hop
- [ ] T3: Animal kinds, models, `AnimalDef`, spawn predicate and rules
- [ ] T4: `Game`: `InteractEntity` feeding, `EatItem`, rules registered
- [ ] T5: Client: right-click routing, `moving`/`happy` flags, leg swing, heart
- [ ] T6: Wrap-up, PR, merge when green
