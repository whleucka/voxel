#include "entity/spawner.hpp"
#include "block/block_data.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <cmath>

void Spawner::addRule(const SpawnRule &rule) {
  rules_.push_back(rule);
  timers_.push_back(rule.interval_ms);
}

std::optional<glm::ivec3> Spawner::groundAt(const World &world, int x, int z) {
  int cx = static_cast<int>(std::floor(static_cast<float>(x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(z) / kChunkDepth));
  if (!world.getChunk(cx, cz)) return std::nullopt;

  auto block = [&](int y) { return world.getBlockAt(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f)); };
  for (int y = kChunkHeight - 3; y >= 0; --y) {
    BlockType b = block(y);
    if (b == BlockType::AIR) continue;
    // The first non-air block from the top must be solid ground, not water.
    if (isLiquid(b)) return std::nullopt;
    if (block(y + 1) == BlockType::AIR && block(y + 2) == BlockType::AIR)
      return glm::ivec3(x, y, z);
    return std::nullopt;
  }
  return std::nullopt;
}

void Spawner::tick(EntityStore &store, const World &world, Rng &rng,
                   std::span<const glm::vec3> players, float time_of_day) {
  for (size_t r = 0; r < rules_.size(); ++r) {
    timers_[r] -= EntityStore::kTickMs;
    if (timers_[r] > 0) continue;
    timers_[r] = rules_[r].interval_ms;
    const SpawnRule &rule = rules_[r];

    for (const glm::vec3 &player : players) {
      int nearby = 0;
      for (const auto &[id, e] : store.all())
        if (e.kind == rule.kind && glm::distance(e.body.position, player) <= kCountRange)
          ++nearby;
      if (nearby >= rule.max_per_player) continue;

      std::uniform_real_distribution<float> angle(0.0f, 2.0f * 3.14159265f);
      std::uniform_real_distribution<float> dist(kMinDistance, kMaxDistance);
      for (int t = 0; t < kTriesPerAttempt; ++t) {
        float a = angle(rng), d = dist(rng);
        int x = static_cast<int>(std::floor(player.x + std::cos(a) * d));
        int z = static_cast<int>(std::floor(player.z + std::sin(a) * d));
        auto ground = groundAt(world, x, z);
        if (!ground) continue;
        if (rule.can_spawn && !rule.can_spawn(world, *ground, time_of_day)) continue;
        store.spawn(rule.kind, glm::vec3(x + 0.5f, ground->y + 1.0f, z + 0.5f));
        break;
      }
    }
  }
}
