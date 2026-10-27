#include "entity/fish.hpp"
#include "biome/biome_manager.hpp"
#include "block/block_data.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>

namespace {

BlockType blockAt(const World &world, glm::ivec3 c) {
  return world.getBlockAt(glm::vec3(c) + 0.5f);
}

// SpawnRule takes a plain function pointer, so one instantiation per kind.
template <EntityKind K>
bool canSpawn(const World &world, glm::ivec3 cell, float) {
  return fishCanSpawn(K, world, cell);
}

} // namespace

int skyAboveWater(const World &world, glm::ivec3 cell) {
  while (cell.y < kChunkHeight - 1 && isLiquid(blockAt(world, cell))) ++cell.y;
  return world.getSkyLightAt(cell);
}

bool fishCanSpawn(EntityKind kind, const World &world, glm::ivec3 cell) {
  if (!isFish(kind) || !isLiquid(blockAt(world, cell))) return false;
  if (!isLiquid(blockAt(world, cell + glm::ivec3(0, 1, 0))) &&
      !isLiquid(blockAt(world, cell - glm::ivec3(0, 1, 0))))
    return false; // too shallow

  const FishDef &fish = *entityType(kind).fish;
  int sky = skyAboveWater(world, cell);
  if (fish.cave) return sky <= kCaveWaterMaxSkyLight;
  if (sky < kOpenWaterMinSkyLight) return false;

  int cx = static_cast<int>(std::floor(static_cast<float>(cell.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(cell.z) / kChunkDepth));
  BiomeType biome = BiomeManager::getBiomeForChunk(cx, cz);
  return std::find(fish.biomes.begin(), fish.biomes.end(), biome) != fish.biomes.end();
}

void registerFishSpawns(Spawner &spawner) {
  spawner.addRule({EntityKind::Pebblefish, kFishPerPlayer, kFishSpawnIntervalMs,
                   canSpawn<EntityKind::Pebblefish>, Placement::Water});
  spawner.addRule({EntityKind::Sunfin, kFishPerPlayer, kFishSpawnIntervalMs,
                   canSpawn<EntityKind::Sunfin>, Placement::Water});
  spawner.addRule({EntityKind::Glowfin, kFishPerPlayer, kFishSpawnIntervalMs,
                   canSpawn<EntityKind::Glowfin>, Placement::CaveWater});
}
