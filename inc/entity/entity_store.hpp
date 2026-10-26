#pragma once

#include "entity/entity.hpp"
#include <map>
#include <optional>
#include <span>

class World;

// Every entity in the Game, simulated at the Game's fixed tick.
class EntityStore {
public:
  static constexpr int kTickMs = 50;
  static constexpr size_t kMaxEntities = 256;
  static constexpr float kDespawnDistance = 96.0f;

  // Returns nothing when the cap is reached or the kind is unknown.
  std::optional<EntityId> spawn(EntityKind kind, glm::vec3 pos);
  void remove(EntityId id);

  Entity *find(EntityId id);
  const Entity *find(EntityId id) const;
  // Ordered by id, so iteration (and anything random driven by it) is
  // deterministic.
  const std::map<EntityId, Entity> &all() const { return entities; }
  std::map<EntityId, Entity> &all() { return entities; }
  size_t size() const { return entities.size(); }

  // One tick: brains, physics, ageing and despawning. `players` are the
  // positions that keep entities alive; entities in unloaded chunks freeze.
  void tick(const World &world, Rng &rng, std::span<const glm::vec3> players);

private:
  std::map<EntityId, Entity> entities;
  EntityId next_id = 1;
};
