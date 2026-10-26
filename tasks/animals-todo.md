# Tasks: animals

Plan: [animals-plan.md](animals-plan.md) · Spec: [SPEC-animals.md](../SPEC-animals.md)

**Every task's definition of done:**
- `make` builds with no warnings.
- `make test` passes.
- One commit per task.

## T1: Items
- [x] IDs 263–266 are valid. Berries heal 4 and Sweet Petals heal 6; no other item is food.
- [x] Icons sit on unique tiles, and the atlas regenerates identically.

## T2: Brains
- [x] A cautious walker turns away from water and 3+ block drops, but steps down 1 block. A swimmer may enter water.
- [x] A hopper jumps while walking on the ground.

## T3: Animals and spawning
- [x] Four kinds have models (with swinging legs), sizes, speeds and an `AnimalDef`. None can be hit.
- [x] The predicate accepts the right biome, ground and daylight, and rejects anything else.
- [x] On generated terrain, animals appear only in their own biomes.

## T4: Game
- [x] Feeding: works with the right item in reach and starts the cooldown. The wrong item, out of reach, during the cooldown, or a non-animal target are rejected with nothing consumed. The product drops after 1 s.
- [x] Eating: heals up to 20 and consumes one. Rejected at full health, with non-food held, or while dead.

## T5: Client
- [x] Right-click order: animal, then eat, then place. Legs swing while moving, and a heart shows while happy.
- [ ] Manual check (user): animals look right in their biomes; feeding, eating, and no lakes or cliffs.

## T6: Wrap-up
- [x] Map and spec status are updated, CI is green, and the PR is merged.
