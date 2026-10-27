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

// ─── Fishing ───────────────────────────────────────────────────────────

int biteWaitMs(Rng &rng, bool fish_near) {
  int wait = std::uniform_int_distribution<int>(kBiteMinMs, kBiteMaxMs)(rng);
  return fish_near ? wait / 2 : wait;
}

ItemId fishFor(const World &world, glm::ivec3 cell) {
  bool open = skyAboveWater(world, cell) >= kOpenWaterMinSkyLight;
  int cx = static_cast<int>(std::floor(static_cast<float>(cell.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(cell.z) / kChunkDepth));
  BiomeType biome = BiomeManager::getBiomeForChunk(cx, cz);
  for (EntityKind k : kFishKinds) {
    const FishDef &f = *entityType(k).fish;
    if (f.cave ? !open
               : open && std::find(f.biomes.begin(), f.biomes.end(), biome) != f.biomes.end())
      return f.raw;
  }
  return ItemId::RAW_PEBBLEFISH; // any other water
}

ItemStack rollCatch(const World &world, glm::ivec3 cell, Rng &rng) {
  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  if (roll(rng) >= kTreasureChance) return {fishFor(world, cell), 1};
  // Treasure, by weight out of 100.
  struct Treasure { ItemId id; int weight; };
  constexpr Treasure kTreasure[] = {
      {ItemId::SEA_SHELL, 40}, {ItemId::STICKS, 25}, {ItemId::SOGGY_BOOT, 20},
      {ItemId::RAW_GOLD, 10},  {ItemId::EMERALD, 4}, {ItemId::DIAMOND, 1}};
  int pick = std::uniform_int_distribution<int>(0, 99)(rng);
  for (const Treasure &t : kTreasure) {
    if (pick < t.weight) return {t.id, 1};
    pick -= t.weight;
  }
  return {ItemId::SEA_SHELL, 1};
}
