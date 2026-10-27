#include "entity/spawner.hpp"
#include "block/block_data.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <cmath>

void Spawner::addRule(const SpawnRule &rule) {
  rules_.push_back(rule);
  timers_.push_back(rule.interval_ms);
}

namespace {

bool columnLoaded(const World &world, int x, int z) {
  int cx = static_cast<int>(std::floor(static_cast<float>(x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(z) / kChunkDepth));
  return world.getChunk(cx, cz) != nullptr;
}

} // namespace

std::optional<glm::ivec3> Spawner::groundAt(const World &world, int x, int z) {
  if (!columnLoaded(world, x, z)) return std::nullopt;

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

std::optional<glm::ivec3> Spawner::caveAt(const World &world, int x, int z, Rng &rng) {
  if (!columnLoaded(world, x, z)) return std::nullopt;

  auto block = [&](int y) { return world.getBlockAt(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f)); };
  int top = kChunkHeight - 1;
  while (top >= 0 && block(top) == BlockType::AIR) --top;

  std::vector<int> floors;
  for (int y = top - 3; y >= 0; --y)
    if (world.isSolidBlock(x, y, z) && block(y + 1) == BlockType::AIR &&
        block(y + 2) == BlockType::AIR)
      floors.push_back(y);
  if (floors.empty()) return std::nullopt;
  size_t pick = std::uniform_int_distribution<size_t>(0, floors.size() - 1)(rng);
  return glm::ivec3(x, floors[pick], z);
}

std::optional<glm::ivec3> Spawner::waterAt(const World &world, int x, int z, Rng &rng) {
  if (!columnLoaded(world, x, z)) return std::nullopt;

  auto wet = [&](int y) {
    return y >= 0 && y < kChunkHeight &&
           isLiquid(world.getBlockAt(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f)));
  };
  std::vector<int> cells;
  for (int y = 0; y < kChunkHeight; ++y)
    if (wet(y) && (wet(y - 1) || wet(y + 1))) cells.push_back(y);
  if (cells.empty()) return std::nullopt;
  size_t pick = std::uniform_int_distribution<size_t>(0, cells.size() - 1)(rng);
  return glm::ivec3(x, cells[pick], z);
}

void Spawner::tick(EntityStore &store, const World &world, Rng &rng,
                   std::span<const glm::vec3> players, float time_of_day,
                   Difficulty difficulty) {
  for (size_t r = 0; r < rules_.size(); ++r) {
    timers_[r] -= EntityStore::kTickMs;
    if (timers_[r] > 0) continue;
    timers_[r] = rules_[r].interval_ms;
    const SpawnRule &rule = rules_[r];
    if (rule.hostile && difficulty == Difficulty::Peaceful) continue;
    int cap = rule.hostile ? scaledCap(rule.max_per_player, difficulty) : rule.max_per_player;
    const bool cave = rule.placement == Placement::Cave;
    const bool water = rule.placement == Placement::Water || rule.placement == Placement::CaveWater;
    const bool near = cave || rule.placement == Placement::CaveWater;

    for (const glm::vec3 &player : players) {
      int nearby = 0;
      for (const auto &[id, e] : store.all())
        // Horizontal, like the placement band: a creature in a valley far
        // below the player still counts toward the cap.
        if (e.kind == rule.kind &&
            glm::length(glm::vec2(e.body.position.x - player.x,
                                  e.body.position.z - player.z)) <= kCountRange)
          ++nearby;
      if (nearby >= cap) continue;

      std::uniform_real_distribution<float> angle(0.0f, 2.0f * 3.14159265f);
      std::uniform_real_distribution<float> dist(near ? kCaveMinDistance : kMinDistance,
                                                 near ? kCaveMaxDistance : kMaxDistance);
      for (int t = 0; t < kTriesPerAttempt; ++t) {
        float a = angle(rng), d = dist(rng);
        int x = static_cast<int>(std::floor(player.x + std::cos(a) * d));
        int z = static_cast<int>(std::floor(player.z + std::sin(a) * d));
        auto ground = water ? waterAt(world, x, z, rng)
                      : cave ? caveAt(world, x, z, rng)
                             : groundAt(world, x, z);
        if (!ground) continue;
        if (rule.can_spawn && !rule.can_spawn(world, *ground, time_of_day)) continue;
        // Standing on the ground block, or floating low in the water cell.
        float y = water ? ground->y + 0.1f : ground->y + 1.0f;
        store.spawn(rule.kind, glm::vec3(x + 0.5f, y, z + 0.5f));
        break;
      }
    }
  }
}
