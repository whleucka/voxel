# Spec: enemies

Module of [CAPABILITY_MAP.md](CAPABILITY_MAP.md). Depends on: `entities`, `vitals`, `items`.

**Status:** implemented. Criteria 1 and 3 are met; criterion 2 (manual check) is up to you.

## Objective

Give nights and caves some tension, without making it too scary for young
players. Three original, spooky-cute enemies come out in the dark. They
find their way to you and attack. You can fight back, and defeating them
drops **glowstone**, the light that keeps them away. A **difficulty
setting** (Peaceful / Easy / Normal, default Easy) lets the kids just
build when they want to.

### The enemies

| Enemy | Where and when | Size (w × h) | Speed | Health | Hit (Normal) | At sunrise |
|---|---|---|---|---|---|---|
| **Gloomling** (shadow blob, glowing eyes, hops) | Surface, at night, in the dark | 0.6 × 0.6 | 3.0 | 6 | 1 (half a heart) | vanishes |
| **Thorn Crawler** (bramble bug) | Surface at night in Plains, Mountain or Cherry | 0.7 × 0.5 | 4.2 | 4 | 2 | vanishes |
| **Cave Grumble** (mossy rock creature, glowing eyes) | Dark caves, any time | 0.8 × 1.3 | 1.6 | 16 | 2 (one heart) | stays |

**Drops:** each enemy always drops 1 glowstone, plus a 50% chance of 1 coal.
The Grumble drops 2 glowstone.

### Acceptance criteria

**Difficulty**
- **The setting:** `difficulty = peaceful | easy | normal` in `voxel.properties`, default `easy`. It can be changed in the F1 panel, and the change takes effect immediately.
- **Peaceful:** nothing spawns, and enemies already in the world are removed.
- **Easy:**
  - Caps are halved (rounded down, at least 1).
  - Hits deal half damage (at least 1).
  - Enemies move at 80% speed.
- **Normal:** the values in the table.

**Spawning**
- **Darkness:** enemies spawn only where the block light at the spawn spot is below 7. A lit base is safe.
- **Surface enemies** (Gloomling, Thorn Crawler):
  - On the surface, with sky light of at least 8 at their feet.
  - Only at night (time of day outside 0.25–0.75).
  - The Crawler only in its biomes.
- **Cave Grumble:**
  - Below the surface, with sky light 0 at its feet and block light below 4.
  - Any time of day.
  - It needs a new **cave placement** that finds an air pocket with solid floor and 2 blocks of headroom in the column.
  - The world has no natural caves yet, so Grumbles live in the tunnels players dig. Tunnels are small and close to their diggers, so cave placement looks 12–32 blocks from a player instead of 24–64. *(Added during the build.)*
- **Caps per player within 64 blocks (Normal):** Gloomling 4, Crawler 3, Grumble 3. The spawner tries every 3 s.
  - Distance for the caps is measured horizontally, like the placement band. Before this, a creature far below the player didn't count, so the cap leaked. *(Fixed during the build; animals benefit too.)*

**Finding the player**
- **A\* paths over the block grid:**
  - A cell is walkable if it has solid ground below and air for the creature's height.
  - Moves go to the 4 neighbours on the same level, up 1 block (a jump), or down at most 3 blocks.
  - The search stops after 600 explored cells, or when the target is more than 32 blocks away. No path means no path.
- **Chasing:**
  - An enemy senses the nearest living player within 24 blocks.
  - It re-plans its path every second, or sooner when the player has moved more than 2 blocks.
  - It follows the waypoints, jumping where the path goes up.
  - With no player in range it wanders cautiously, like animals do.

**Attacking**
- **Contact:** when an enemy's box comes within 0.3 blocks of a living player's box, the player takes the enemy's hit damage. There's a 1 s cooldown per enemy.
- **Knockback:** the player is knocked back away from the enemy. Movement is decided on the client, so `Game` sends `KnockedBack {velocity}` and the client adds it to the player's velocity.
- **Death:** dying to an enemy uses the existing death and respawn rules, and the inventory is kept.

**Fighting back**
- **Hitting:** enemies are hittable with the existing rules (2 damage, knockback, a red flash). At 0 health they drop their loot.

**Sunrise**
- **Vanishing:** at sunrise (time of day ≥ 0.25 and < 0.75), Gloomlings and Crawlers that can see the sky (sky light ≥ 8) disappear with no drops. Grumbles stay.

**Looks**
- **Models:** coloured boxes. Eyes are **glowing boxes**, unlit by the world, so they shine in the dark. That's new: a `glow` flag on `ModelBox`.

## Design

```cpp
// settings: enum class Difficulty { Peaceful, Easy, Normal }; g_settings.difficulty
// entities:
EntityKind::{Gloomling = 7, ThornCrawler = 8, CaveGrumble = 9}
struct EnemyDef { int damage; bool surface; bool vanish_at_dawn;
                  std::vector<BiomeType> biomes; /* empty = any */ };
struct ChanceDrop { ItemStack stack; float chance; };      // EntityTypeDef gains chance_drops
struct ModelBox { …; bool glow = false; };

// Brains see the world, the RNG and where players are.
struct BrainContext { const World &world; Rng &rng; std::span<const glm::vec3> players; };
class Brain { virtual Intent think(const Entity &, const BrainContext &) = 0; };
class ChaseBrain;  // nearest player within 24 → path → waypoints; else wander

// physics/path.hpp: pure, testable
struct PathParams { int height_cells = 2; int max_drop = 3; int max_nodes = 600; float max_range = 32; };
std::optional<std::vector<glm::ivec3>> findPath(const World &, glm::ivec3 from, glm::ivec3 to,
                                                const PathParams &);

// spawner: SpawnRule gains Placement { Surface, Cave }
```

- **Protocol:** a new event, `KnockedBack {velocity}`. Changing the difficulty sets `g_settings` on the host. In single-player the host is this machine, so no command is needed. `multiplayer` will make it a host-only setting.
- **`Game::tick` order:**
  1. Entities (brains and physics).
  2. Sunrise vanishing, and removal on Peaceful.
  3. Enemy contact attacks.
  4. The spawner (with difficulty-adjusted caps).
  5. Dropped items.
- **Shared speed and damage scaling:** `difficultyScale()` returns the Easy multipliers for caps, damage and speed.

### Not in scope
- Ranged enemies.
- Weapons and armour.
- Enemies breaking blocks.
- Sounds.
- Saving enemies.
- Particle puffs at sunrise (they simply vanish).
- Bosses.

## Tech Stack / Commands

Unchanged: C++20, `make`, `make test`. One shader uniform is added (`uGlow`).

## Project Structure

```
inc/core/settings.hpp, src/core/config.cpp           → Difficulty, properties key
inc/physics/path.hpp, src/physics/path.cpp           → A* (pure)
inc/entity/brain.hpp, src/entity/brain.cpp           → BrainContext, ChaseBrain
inc/entity/entity_type.hpp, src/entity/entity_type.cpp → enemy kinds, EnemyDef, glow, chance drops
inc/entity/enemies.hpp, src/entity/enemies.cpp       → spawn predicates, rules, scaling
inc/entity/spawner.*                                 → cave placement
inc/sim/*, src/sim/game.cpp                          → contact attacks, sunrise, peaceful, KnockedBack
src/player/player.cpp, src/core/engine.cpp           → apply knockback, difficulty in F1
src/render/entity_renderer.cpp, assets/shaders/entity.frag → glowing boxes
tests/path_test.cpp, tests/enemy_test.cpp, tests/game_test.cpp, …
```

## Testing Strategy

- **Pathfinding** (sky rigs):
  - A straight line, around a wall, and up a 1-block step.
  - It won't climb 2 blocks, and will drop 3 but not 4.
  - It needs headroom.
  - No path returns nothing, and the node limit and range limit hold.
- **`ChaseBrain`:**
  - On a flat rig it reaches a player 10 blocks away.
  - It reaches a player behind a wall with a gap.
  - With no one in range it wanders.
- **Spawn predicates:**
  - The Gloomling spawns only at night, on the surface, in the dark, and not next to glowstone.
  - The Crawler only in its biomes.
  - The Grumble only underground in the dark, at any time.
  - Cave placement finds pockets and never the surface.
- **`Game`:**
  - Contact damage, the per-enemy cooldown, and the knockback event to the right player.
  - Easy halves damage, with a minimum of 1.
  - Peaceful removes enemies and stops spawning.
  - Sunrise vanishing applies to surface enemies only.
  - Kills drop glowstone (and sometimes coal; checked with a seeded RNG).
  - Death to an enemy respawns you with your items.
- **Settings:** `difficulty` round-trips through `voxel.properties`, and the default is easy.
- **Manual check:**
  - A night outdoors brings Gloomlings that hop at you and hurt a little.
  - A lit area stays safe.
  - Dark caves have Grumbles.
  - Crawlers are fast in forests.
  - Glowing eyes show at night.
  - Fighting back drops glowstone.
  - At sunrise surface enemies vanish.
  - Peaceful clears them.

## Boundaries

- **Always:**
  - Enemy decisions and damage happen only in `Game`.
  - Kinds and IDs are append-only.
  - Run `make test`.
  - Keep the Peaceful option working.
- **Ask first:**
  - Changing any number in the enemy table, the caps, light thresholds or drops.
  - Adding enemies.
  - Letting enemies break blocks.
- **Never:**
  - Make Peaceful spawn enemies.
  - Let the client decide damage.
  - Change the save format (`voxel.properties` gets a new key, but it isn't a save file).

## Success Criteria

1. Every rule above is covered by passing tests (rendering excepted).
2. The manual check passes, including FPS during a busy night.
3. The build has zero new warnings and CI is green.

## Open Questions

None. Decisions so far: a difficulty setting (default Easy); all three
enemies; surface enemies vanish at sunrise; defeated enemies drop
glowstone, plus coal sometimes.
