#include "entity/entity_store.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <cmath>
#include <vector>

namespace {

bool chunkLoaded(const World &world, glm::vec3 p) {
  int cx = static_cast<int>(std::floor(p.x / kChunkWidth));
  int cz = static_cast<int>(std::floor(p.z / kChunkDepth));
  return world.getChunk(cx, cz) != nullptr;
}

} // namespace

std::optional<EntityId> EntityStore::spawn(EntityKind kind, glm::vec3 pos) {
  if (!isValidEntityKind(kind) || entities.size() >= kMaxEntities)
    return std::nullopt;

  const EntityTypeDef &def = entityType(kind);
  Entity e;
  e.id = next_id++;
  e.kind = kind;
  e.body.position = pos;
  e.body.half_width = def.half_width;
  e.body.height = def.height;
  e.body.eye_height = def.height * 0.85f;
  e.body.swims = def.fish.has_value();
  e.health = def.max_health;
  if (def.brain) e.brain = def.brain();

  EntityId id = e.id;
  entities.emplace(id, std::move(e));
  return id;
}

void EntityStore::remove(EntityId id) { entities.erase(id); }

Entity *EntityStore::find(EntityId id) {
  auto it = entities.find(id);
  return it == entities.end() ? nullptr : &it->second;
}

const Entity *EntityStore::find(EntityId id) const {
  auto it = entities.find(id);
  return it == entities.end() ? nullptr : &it->second;
}

void EntityStore::tick(const World &world, Rng &rng, std::span<const glm::vec3> players,
                       std::span<const glm::vec3> targets, float hostile_speed) {
  constexpr float dt = kTickMs / 1000.0f;
  const BrainContext ctx{world, rng, targets};
  std::vector<EntityId> gone;

  for (auto &[id, e] : entities) {
    const EntityTypeDef &def = entityType(e.kind);
    e.age_ms += kTickMs;
    e.pickup_delay_ms = std::max(0, e.pickup_delay_ms - kTickMs);
    e.fed_cooldown_ms = std::max(0, e.fed_cooldown_ms - kTickMs);
    e.happy_ms = std::max(0, e.happy_ms - kTickMs);
    e.product_delay_ms = std::max(0, e.product_delay_ms - kTickMs);
    e.attack_cooldown_ms = std::max(0, e.attack_cooldown_ms - kTickMs);

    // Boats stay where they were left, and only the Game moves them.
    if (def.vehicle) continue;
    if (def.despawn_age_ms > 0 && e.age_ms >= def.despawn_age_ms) {
      gone.push_back(id);
      continue;
    }
    if (!players.empty()) {
      float nearest = kDespawnDistance + 1.0f;
      for (const glm::vec3 &p : players)
        nearest = std::min(nearest, glm::distance(p, e.body.position));
      if (nearest > kDespawnDistance) {
        gone.push_back(id);
        continue;
      }
    }

    // Unloaded ground: freeze rather than fall out of the world.
    if (!chunkLoaded(world, e.body.position)) continue;

    if (def.fish) {
      e.stranded_ms = SwimBrain::inWater(e, world) ? 0 : e.stranded_ms + kTickMs;
      if (e.stranded_ms >= kStrandedMs) {
        gone.push_back(id);
        continue;
      }
    }

    // A freshly hit creature is knocked back, not steering.
    if (e.brain && e.hurt_ms == 0) {
      Intent in = e.brain->think(e, ctx);
      e.yaw = in.yaw;
      float speed = def.walk_speed * (def.enemy ? hostile_speed : 1.0f);
      if (glm::length(in.move_dir) > 0.0f) {
        e.body.velocity.x = in.move_dir.x * speed;
        e.body.velocity.z = in.move_dir.y * speed;
      } else if (in.halt && e.body.on_ground) {
        e.body.velocity.x = e.body.velocity.z = 0.0f;
      }
      if (e.body.swims && e.body.inWater()) e.body.velocity.y = in.rise * speed;
      if (in.jump && e.body.on_ground) {
        // A stranded fish flops: a small hop.
        e.body.velocity.y = kPlayerJumpStrength * (def.fish ? 0.5f : 1.0f);
        e.body.on_ground = false;
      }
    }
    e.body.step(dt, &world);
    e.hurt_ms = std::max(0, e.hurt_ms - kTickMs);
  }

  for (EntityId id : gone) entities.erase(id);
}
