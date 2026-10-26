# Spec: animals

Module of [CAPABILITY_MAP.md](CAPABILITY_MAP.md). Depends on: `entities`, `items`, `inventory`, `vitals`.

## Objective

Bring the world to life with four friendly, original animals, each at home
in a biome. Animals **can't be hurt**. You befriend them by feeding them
their favourite block. In return they drop a product, and some products are
food that heals you when eaten. There's no killing; this is a gentle loop
for young players.

### The animals

| Animal | Lives in (biome, ground) | Size (w × h) | Speed | Favourite | Gives | Food? |
|---|---|---|---|---|---|---|
| **Woolly** | Mountain, Plains, on grass/snow | 0.9 × 1.1 | 1.4 | Grass | Wool | no |
| **Bunny** | Plains, Cherry, on grass | 0.4 × 0.5 | 2.6 (hops) | Cherry Blossom | Berries | +2 hearts |
| **Shell turtle** | Tropical, on sand | 0.8 × 0.5 | 0.7 | Palm Leaf | Sea Shell | no |
| **Blossom deer** | Cherry, on grass | 0.8 × 1.5 | 2.2 | Oak Leaf | Sweet Petals | +3 hearts |

The names, products and favourites are easy to change. They're in one table.

### Acceptance criteria

**Feeding**
- **How:** right-click an animal while holding its favourite block. `Game` checks reach, that the item is the right one, and that the animal isn't on its cooldown.
- **Effect:** one item is consumed. The animal shows hearts for 2 s, and its product pops out after 1 s. That animal then has a 60 s cooldown.
- **Rejections:** feeding the wrong item, or feeding during the cooldown, is rejected. The F1 panel shows the reason, and nothing is consumed.
- **Animals can't be hit.** `AttackEntity` on an animal is rejected, like on dropped items.

**Food**
- **New items:** WOOL, BERRIES, SEA_SHELL and SWEET_PETALS are appended to `ItemId`. The two foods have a `food_heal` value of 4 and 6 points.
- **Eating:**
  - Right-clicking while holding food, and not aiming at an animal, eats one and heals instantly. Health is capped at 20.
  - Eating at full health is rejected, and nothing is consumed.
  - The heal doesn't reset the regen delay.

**Spawning**
- **Spawn rules:** one per animal.
  - Each spawns only in its biomes, on its ground blocks, with two blocks of headroom, and only in daylight (time of day 0.25–0.75).
  - Each has a cap of 3 per player within 64 blocks, and tries every 4 s.
- **Despawning:** animals despawn by the existing distance rule (more than 96 blocks from every player). They're never removed by age, and they aren't saved.

**Behaviour**
- **Wandering:** animals wander like the test creature. They never step into water (the turtle may) or off a drop of more than 2 blocks; they pick a new direction instead.
- **Bunnies hop:** while walking, they jump whenever they're on the ground.
- **Panic:** a fed or nudged animal is never scared. There's no fleeing, because nothing hurts them.

**Looks**
- **Models:** each animal is a coloured-box model with simple leg swing while moving. Each leg box swings around its top edge, and the swing is client-side only.
- **Hearts:** a fed animal shows a small bobbing red heart above its head for 2 s.
- **Icons:** placeholder icons are added for the 4 new items.

## Design

```cpp
// items: ItemDef gains food_heal (0 = not food); new ids appended after RAW_COPPER
enum class ItemId : uint16_t { …, RAW_COPPER, WOOL, BERRIES, SEA_SHELL, SWEET_PETALS };

// entities: EntityTypeDef gains
struct AnimalDef {                    // set for animals, nullopt otherwise
  ItemId favourite;                   // block item they eat
  ItemStack product;
  std::vector<BiomeType> biomes;
  std::vector<BlockType> ground;      // blocks they spawn on
  bool can_swim;                      // turtle: may walk into water
};
// ModelBox gains: int swing = 0;     // +1 / -1: leg swings in that phase
EntityKind::{Woolly = 3, Bunny = 4, ShellTurtle = 5, BlossomDeer = 6}

// Entity gains: int fed_cooldown_ms, happy_ms, product_delay_ms
// EntityView gains: bool happy; bool moving (moving is set by the interpolator)
```

- **Protocol:**
  - New commands: `InteractEntity {id}` (right-click on an entity) and `EatItem {}`.
- **Client right-click order:**
  1. An animal under the crosshair (nearer than any block) → `InteractEntity`.
  2. Otherwise, holding food → `EatItem`.
  3. Otherwise → `PlaceBlock`, as today.
- **`WanderBrain` gains two options:**
  - `cautious`: it checks the cell ahead at foot level and the drop below it before walking.
  - `hop`: it jumps whenever it's on the ground while walking.
  - Each animal's brain factory sets these.
- **Spawn rules** are registered in `Game`'s constructor from the animal table. The placement check uses `BiomeManager::getBiomeForChunk` on the spawn column, the ground block, and the daylight window.
- **Leg swing:** the renderer reads `EntityView::moving`. A swing box is rotated about its top edge by `±sin(time × 10) × 30°`.

### Not in scope
- Hunting or hurting animals.
- Breeding, babies and taming.
- Sounds.
- Hunger.
- Cooking.
- Saving animals.
- Night creatures (that's `enemies`).

## Tech Stack / Commands

Unchanged: C++20, `make`, `make test`. The placeholder art script gains the 4 new icons.

## Project Structure

```
inc/item/item.hpp, src/item/item.cpp                 → new items, food_heal
tools/gen_item_placeholders.py, res/item_atlas.png   → 4 new icons
inc/entity/entity_type.hpp, src/entity/entity_type.cpp → animal kinds, models, AnimalDef
inc/entity/brain.hpp, src/entity/brain.cpp           → cautious + hop options
inc/entity/animals.hpp, src/entity/animals.cpp       → spawn predicate, rule list
inc/entity/entity_interp.* , inc/sim/protocol.hpp   → moving/happy flags; InteractEntity, EatItem
src/sim/game.cpp                                     → feeding, eating, rules
src/render/entity_renderer.cpp                       → leg swing, heart
src/core/engine.cpp                                  → right-click routing
tests/animal_test.cpp, tests/game_test.cpp, tests/item_test.cpp, …
```

## Testing Strategy

- **Items:**
  - The new items are valid, with stable IDs 263–266.
  - The two foods have the right heal values, and nothing else is food.
  - Their icons sit on unique atlas tiles.
- **Feeding, through `Game`:**
  - The right item, in reach, works: one is consumed, hearts show, the product drops after 1 s, and the cooldown starts.
  - The wrong item, out of reach, during the cooldown, an empty hand, or a non-animal target are each rejected with nothing consumed.
- **Eating:**
  - Heals and consumes one, capped at 20.
  - Rejected at full health or with non-food held.
  - Rejected while dead.
- **Animals can't be hit:** `AttackEntity` is rejected.
- **Spawn rules:**
  - Each animal's predicate accepts its own biome, ground and daylight, and rejects the wrong biome, the wrong ground and night.
  - On a generated world with the rules registered, animals appear only in their biomes.
- **Brains:**
  - A cautious walker turns away from water ahead and from a 3-block drop, but steps down 1 block.
  - The turtle may enter water.
  - A bunny hops while walking.
- **Interpolator:** `moving` is true when the position changed between snapshots.
- **Manual check:**
  - Each animal appears in its biome and looks right, with legs swinging.
  - Feeding gives hearts and a product, and the cooldown works.
  - Eating berries heals.
  - Animals don't walk into lakes or off cliffs.

## Boundaries

- **Always:**
  - Keep animals unhurtable.
  - IDs and kinds are append-only.
  - Run `make test`.
- **Ask first:**
  - Changing the roster, favourites, products, heal values, cooldowns or caps.
  - Adding hunting.
- **Never:**
  - Let animals take damage or die.
  - Let the client decide feeding results.
  - Change the save format.

## Success Criteria

1. Every rule above is covered by passing tests (rendering excepted).
2. The manual check passes, including FPS with every animal visible.
3. The build has zero new warnings and CI is green.

## Open Questions

None. Decisions so far: friendly animals only (feeding, no hurting); food
heals; the roster is Woolly, Bunny, Shell turtle and Blossom deer. The
favourites and products in the table are my proposal, open to the kids'
edits.
