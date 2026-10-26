#pragma once

#include "entity/entity_store.hpp"
#include <glm/glm.hpp>
#include <span>
#include <vector>

class World;

// When and where one kind of creature appears near players. Rules are added
// by the modules that own creatures (animals, enemies).
struct SpawnRule {
  EntityKind kind = EntityKind::TestCreature;
  int max_per_player = 0;   // of this kind within kCountRange of a player
  int interval_ms = 1000;   // between attempts
  // ground: the solid block the creature would stand on (with 2 air above);
  // time_of_day: fraction of the day, 0..1 (0.5 = noon).
  bool (*can_spawn)(const World &world, glm::ivec3 ground, float time_of_day) = nullptr;
};

class Spawner {
public:
  static constexpr float kMinDistance = 24.0f;
  static constexpr float kMaxDistance = 64.0f;
  static constexpr float kCountRange = 64.0f;
  static constexpr int kTriesPerAttempt = 4;

  void addRule(const SpawnRule &rule);
  const std::vector<SpawnRule> &rules() const { return rules_; }

  // Called every Game tick.
  void tick(EntityStore &store, const World &world, Rng &rng,
            std::span<const glm::vec3> players, float time_of_day);

  // A spot to stand on near (x, z): the top solid block of that column with
  // two air blocks above it, in a loaded chunk. Nothing otherwise.
  static std::optional<glm::ivec3> groundAt(const World &world, int x, int z);

private:
  std::vector<SpawnRule> rules_;
  std::vector<int> timers_; // ms until each rule's next attempt
};
