# Spec: entities

Module of [CAPABILITY_MAP.md](CAPABILITY_MAP.md). Depends on: `sim-core`, `items`, `inventory`, `vitals`.
Used by: `animals`, `enemies`, `multiplayer`.

## Objective

Give the world things other than players: objects that move with physics,
are simulated by the authoritative `Game`, appear on every client, and can
be hit. The module ships the shared machinery plus one real entity type:

- **Dropped items:** items you can't carry (or drop on purpose) land in the
  world and can be picked up. They replace "full inventory = item lost".
- **A debug test creature** that wanders and can be hit. It's spawned only
  from the F1 panel, to exercise brains, hitting and spawning until
  `animals` exists.

### Acceptance criteria

**Physics**
- **Shared body:** players and entities use one `PhysicsBody` with the same rules: gravity, collision axis by axis, auto-step, water drag and buoyancy, friction.
- **Player movement is unchanged:** characterization tests written before the extraction still pass.
- **Entities collide with blocks only.** Players and entities walk through each other.

**Simulation and sync**
- **Simulation:** `Game` simulates entities at 20 Hz. Entities in unloaded chunks are frozen.
- **Despawning:**
  - Entities more than 96 blocks from every player despawn.
  - Dropped items despawn after 5 minutes.
  - There's a global cap of 256 entities.
- **Snapshots:** each tick, every client gets an `EntitySnapshot` of the entities within 96 blocks. An entity missing from the snapshot is gone.
- **Smooth movement on screen:** the client interpolates between the last two snapshots, so entities move smoothly at any frame rate.
- **Not saved:** entities aren't saved. Quitting clears dropped items and creatures, and the save format is unchanged.

**Rendering**
- **Box models:** an entity is drawn as a box model, a list of cuboids, each with one of the following.
  - A flat colour (creatures).
  - Block-atlas faces (dropped block items: a 0.25-block textured cube).
  - Item-atlas faces (dropped materials: a thin card showing the icon, with transparent parts cut out).
- **Lighting:** boxes are lit by the world light at the entity's position (sky light × daylight, or block light) and shaded per face like blocks.
- **Dropped items spin and bob.** The spin and bob are client-side visuals only.
- **Hurt flash:** a hurt entity is tinted red for 0.3 s.

**Dropped items**
- **Inventory overflow:** breaking a block whose drop doesn't fit spawns the leftover as a dropped item at the block's centre, with a small random hop.
- **Q drops the held item.** Q drops one of it and Ctrl+Q drops the whole stack. The item is thrown forward from the eyes.
- **Picking up:**
  - Walking within 1.5 blocks of a dropped item picks it up once its pickup delay has passed.
  - The delay is 0.5 s for broken-block drops and 1.5 s for thrown items.
  - If only part of the stack fits, the rest stays on the ground.
- **Merging:** identical dropped items within 1 block of each other merge, up to the item's max stack.

**Hitting**
- **Aiming and the hit command:**
  - Left-click aims with one ray against both blocks and entity boxes, and the nearest wins.
  - Hitting an entity sends `AttackEntity {id}`.
- **Validation:** `Game` checks the attacker is alive and within reach (eye to box ≤ `kPlayerReach`), the target can be hit, and a 0.4 s per-player cooldown has passed.
- **Damage:**
  - A hit does 2 damage (bare hands; weapons come later).
  - It knocks the target back away from the attacker and starts the hurt flash.
  - At 0 health the entity is removed and its type's drops spawn as dropped items.

**Spawning machinery** (rules come with `animals` and `enemies`)
- **Spawn rules:** a `SpawnRule` names an entity type, a per-player cap, an attempt interval and a placement predicate.
- **The spawner:** tries random surface spots 24–64 blocks from each player, in loaded chunks only.
- **Debug panel:** "Spawn test creature" puts one 3 blocks in front of you.

## Design

```
Game (authoritative)                                Engine (client)
────────────────────                                ───────────────
EntityStore  id → Entity{kind, PhysicsBody, yaw,    EntityViews: last two snapshots
             health, hurt_ms, item, age, brain}     → interpolated per frame
EntityTypes  kind → {name, size, max_health,        EntityRenderer: unit cube VBO,
             model boxes, brain, drops}               entity.vert/frag, per-box draw
Spawner      SpawnRules (none registered yet)       Ray vs blocks + entity boxes
tick(): brains → PhysicsBody::step → pickups/merge  Q / Ctrl+Q, debug spawn button
        → despawn → spawner → EntitySnapshot
```

```cpp
// physics/physics_body.hpp: shared by Player and entities
struct PhysicsBody {
  glm::vec3 position, velocity;
  float half_width, height, step_height;
  bool on_ground, in_water, eyes_underwater;
  void step(float dt, const World &world);   // gravity, collide, step, drag
};

// entity/entity_type.hpp: append-only, like BlockType/ItemId
enum class EntityKind : uint16_t { DroppedItem = 1, TestCreature = 2 };
struct ModelBox { glm::vec3 offset, size; BoxLook look; };  // colour or atlas faces
struct EntityTypeDef {
  std::string_view name; float half_width, height; int max_health; // 0 = can't be hit
  std::vector<ModelBox> model; BrainFactory brain; std::vector<ItemStack> drops;
};

// entity/brain.hpp: what a creature wants to do each tick
struct Intent { glm::vec2 move_dir; float speed; bool jump; float yaw; };
class Brain { public: virtual Intent think(const Entity&, const World&, Rng&) = 0; };
class WanderBrain;  // walk somewhere random, pause, jump when blocked
```

- **Protocol:**
  - New commands: `AttackEntity {id}`, `DropItem {whole_stack}`, and `SpawnDebugEntity {kind}` (debug panel).
  - New event: `EntitySnapshot { std::vector<EntityView{id, kind, position, yaw, item, hurt}> }`.
  - A full snapshot is about 3 KB per tick for 100 entities, which is fine in-process. `multiplayer` can add deltas.
- **Randomness:** `Game` owns a seeded `std::mt19937` for spawning, hops and wandering. Tests seed it, so they're deterministic.
- **Pure, client-side interpolation:** an `EntityInterpolator` takes two snapshots and alpha and produces positions. It's tested.
- **Entity ray tests:** the client tests the ray against entity boxes using the interpolated views. A ray-versus-box helper is pure and tested.

### Not in scope
- Pathfinding (`enemies`).
- Actual animal and enemy types and their spawn rules.
- Weapons or tools.
- Entity shadows.
- Saving entities.
- Network deltas (`multiplayer`).

## Tech Stack / Commands

C++20, `make`, `make test`. Two new GLSL shaders (`assets/shaders/entity.vert`
and `entity.frag`, GLSL 330 like the others). No new dependencies.

## Project Structure

```
inc/physics/physics_body.hpp, src/physics/physics_body.cpp   → PhysicsBody (pure but needs World)
inc/entity/entity_type.hpp, src/entity/entity_type.cpp       → kinds, defs, models, drops
inc/entity/entity.hpp, inc/entity/entity_store.hpp, src/entity/entity_store.cpp
inc/entity/brain.hpp, src/entity/brain.cpp                   → Brain, WanderBrain
inc/entity/spawner.hpp, src/entity/spawner.cpp               → SpawnRule, Spawner
inc/entity/entity_interp.hpp, src/entity/entity_interp.cpp   → client interpolation (pure)
inc/physics/ray.hpp, src/physics/ray.cpp                     → ray vs AABB (pure)
inc/render/entity_renderer.hpp, src/render/entity_renderer.cpp, assets/shaders/entity.*
inc/sim/*, src/sim/game.cpp                                  → protocol + Game integration
inc/player/*, src/player/player.cpp                          → Player uses PhysicsBody
src/core/engine.cpp                                          → views, render, Q, attack, debug
tests/physics_test.cpp, tests/entity_test.cpp, tests/game_test.cpp, …
```

## Testing Strategy

- **Characterization first:** before extracting `PhysicsBody`, tests drive `Player::update` on a sync-loaded world. They pin down the following, then must still pass after the extraction.
  - Falling to the ground.
  - Stopping at walls.
  - Auto-stepping a slab-height ledge but not a full block.
  - Water slowing a fall.
  - Friction stopping horizontal motion.
- **`PhysicsBody`:** the same cases for an entity-sized body.
- **Entities through `Game`:**
  - Spawning, falling and resting on the ground.
  - Freezing in unloaded chunks.
  - Despawning by distance, by age and at the cap.
  - Snapshot contents and the 96-block range.
- **Dropped items:**
  - Overflow when breaking with a full inventory.
  - Q and Ctrl+Q.
  - The pickup delay, plus partial and full pickup.
  - Merging.
  - Despawning after 5 minutes.
- **Hitting:**
  - Reach, cooldown, dead attackers, and unhittable kinds (dropped items).
  - Damage and knockback direction.
  - Death removes the entity and spawns its drops.
- **Spawner (with a test rule):**
  - Respects the per-player cap and the 24–64 distance band.
  - Places only on loaded surfaces that pass the predicate.
- **Pure helpers:**
  - Ray versus box hits and misses, including the nearest of several.
  - Interpolation, including when an entity appears or disappears between snapshots.
- **Manual check:**
  - Dropped items look right, spin, get picked up and merge.
  - The test creature wanders, takes hits with knockback and a red flash, and dies leaving its drops.
  - Player movement feels unchanged.
  - FPS with about 50 creatures is still fine.

## Boundaries

- **Always:**
  - Entity state changes only in `Game`.
  - `EntityKind` is append-only.
  - Run `make test`.
  - Keep the characterization tests passing.
- **Ask first:**
  - Changing pickup, despawn or cap numbers.
  - Saving entities.
  - Adding real creature types (those belong to `animals`/`enemies`).
  - Changing player movement.
- **Never:**
  - Let the client decide entity positions, damage or pickups.
  - Delete items silently when they can be dropped instead.
  - Change the save format.

## Success Criteria

1. All the behaviour above is covered by passing tests (rendering excepted).
2. The player characterization tests pass unchanged after the `PhysicsBody` extraction.
3. The manual check passes, including movement feel and FPS.
4. The build has zero new warnings and CI is green.

## Open Questions

None. Decisions so far:
- Dropped items are the first real entity.
- Creatures are coloured boxes; dropped items use their block or item art.
- Generic hitting is built here.
- Players and entities walk through each other.
