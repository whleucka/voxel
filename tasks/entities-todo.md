# Tasks: entities

Plan: [entities-plan.md](entities-plan.md) · Spec: [SPEC-entities.md](../SPEC-entities.md)

**Every task's definition of done:**
- `make` builds with no warnings.
- `make test` passes.
- One commit per task.

## T1: Physics characterization
- [ ] Tests record falling onto a platform, stopping at a wall, not climbing a full block, jumping up a ledge, water slowing a fall, friction, and crouch height.

## T2: PhysicsBody
- [ ] `Player` uses `PhysicsBody`, and the T1 tests pass unchanged.

## T3: Entity core
- [ ] Entities spawn, fall and rest on the ground. They freeze in unloaded chunks.
- [ ] They despawn beyond 96 blocks, by age where the kind sets one, and at the 256 cap.
- [ ] `WanderBrain` moves the test creature, and it jumps when blocked.

## T4: Sync
- [ ] `EntitySnapshot` reaches each client every tick with the entities within 96 blocks.
- [ ] Interpolation and ray-vs-AABB are tested.

## T5: Rendering
- [ ] Coloured, block-atlas and item-atlas boxes are lit by the world light.
- [ ] Dropped items spin and bob, and the hurt tint shows.
- [ ] Manual check (user): it looks right, and FPS with about 50 creatures is fine.

## T6: Dropped items
- [ ] Inventory overflow, Q and Ctrl+Q drops, the pickup delay, partial pickup, merging, and the 5-minute despawn.

## T7: Hitting
- [ ] Reach, cooldown, dead attackers, unhittable kinds, damage and knockback, and death drops.
- [ ] Left-click picks the nearer of block and entity.

## T8: Spawner
- [ ] Per-player cap, the 24–64 block band, only loaded surface spots, and the predicate.
- [ ] The debug spawn button.

## T9: Wrap-up
- [ ] Map and spec status are updated, CI is green, and the PR is merged.
- [ ] Manual check (user): movement feels unchanged, and items and the creature behave as described.
