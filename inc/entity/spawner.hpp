#pragma once

#include "core/difficulty.hpp"
#include "entity/entity_store.hpp"
#include <glm/glm.hpp>
#include <span>
#include <vector>

class World;

// Where a rule looks for a spot: the top of a column, an air pocket below it
// (a cave), or a water cell in it (fish). Cave spots are looked for closer to
// players. For water, `ground` in can_spawn is the water cell itself.
enum class Placement { Surface, Cave, Water, CaveWater };

// When and where one kind of creature appears near players. Rules are added
// by the modules that own creatures (animals, enemies).
struct SpawnRule {
  EntityKind kind = EntityKind::TestCreature;
  int max_per_player = 0;   // of this kind within kCountRange (horizontally) of a player
  int interval_ms = 1000;   // between attempts
  // ground: the solid block the creature would stand on (with 2 air above);
  // time_of_day: fraction of the day, 0..1 (0.5 = noon).
  bool (*can_spawn)(const World &world, glm::ivec3 ground, float time_of_day) = nullptr;
  Placement placement = Placement::Surface;
  bool hostile = false; // enemies: none on Peaceful, caps scaled by difficulty
};

class Spawner {
public:
  static constexpr float kMinDistance = 24.0f;
  static constexpr float kMaxDistance = 64.0f;
  static constexpr float kCountRange = 64.0f;
  static constexpr int kTriesPerAttempt = 4;
  // Dug tunnels are small and near where players are, so caves look closer.
  static constexpr float kCaveMinDistance = 12.0f;
  static constexpr float kCaveMaxDistance = 32.0f;

  void addRule(const SpawnRule &rule);
  const std::vector<SpawnRule> &rules() const { return rules_; }

  // Called every Game tick.
  void tick(EntityStore &store, const World &world, Rng &rng,
            std::span<const glm::vec3> players, float time_of_day,
            Difficulty difficulty = Difficulty::Normal);

  // A spot to stand on near (x, z): the top solid block of that column with
  // two air blocks above it, in a loaded chunk. Nothing otherwise.
  static std::optional<glm::ivec3> groundAt(const World &world, int x, int z);
  // A random spot to stand on below the top of column (x, z): solid floor
  // with two air blocks above, under the column's highest block. In a loaded
  // chunk; nothing if the column has no such pocket.
  static std::optional<glm::ivec3> caveAt(const World &world, int x, int z, Rng &rng);
  // A random water cell in column (x, z) with water directly above or below
  // it (water at least 2 deep). In a loaded chunk; nothing if there is none.
  static std::optional<glm::ivec3> waterAt(const World &world, int x, int z, Rng &rng);

private:
  std::vector<SpawnRule> rules_;
  std::vector<int> timers_; // ms until each rule's next attempt
};
