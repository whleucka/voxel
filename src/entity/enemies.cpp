#include "entity/enemies.hpp"
#include "entity/animals.hpp"
#include "biome/biome_manager.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>

bool isNight(float time_of_day) {
  return time_of_day < kDaylightStart || time_of_day >= kDaylightEnd;
}

bool enemyCanSpawn(EntityKind kind, const World &world, glm::ivec3 ground,
                   float time_of_day) {
  if (!isEnemy(kind)) return false;
  const EnemyDef &enemy = *entityType(kind).enemy;
  glm::ivec3 feet = ground + glm::ivec3(0, 1, 0);
  int sky = world.getSkyLightAt(feet);
  int light = world.getBlockLightAt(feet);

  if (!enemy.surface) return sky == 0 && light < kCaveEnemyMaxBlockLight;

  if (!isNight(time_of_day)) return false;
  if (sky < kSurfaceEnemyMinSkyLight || light >= kEnemyMaxBlockLight) return false;
  if (enemy.biomes.empty()) return true;
  int cx = static_cast<int>(std::floor(static_cast<float>(ground.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(ground.z) / kChunkDepth));
  BiomeType biome = BiomeManager::getBiomeForChunk(cx, cz);
  return std::find(enemy.biomes.begin(), enemy.biomes.end(), biome) != enemy.biomes.end();
}

namespace {

template <EntityKind K>
bool canSpawn(const World &world, glm::ivec3 ground, float time_of_day) {
  return enemyCanSpawn(K, world, ground, time_of_day);
}

SpawnRule hostileRule(EntityKind kind, int cap, bool (*can)(const World &, glm::ivec3, float)) {
  SpawnRule rule{kind, cap, kEnemySpawnIntervalMs, can};
  rule.placement = entityType(kind).enemy->surface ? Placement::Surface : Placement::Cave;
  rule.hostile = true;
  return rule;
}

} // namespace

void registerEnemySpawns(Spawner &spawner) {
  spawner.addRule(hostileRule(EntityKind::Gloomling, 4, canSpawn<EntityKind::Gloomling>));
  spawner.addRule(hostileRule(EntityKind::ThornCrawler, 3, canSpawn<EntityKind::ThornCrawler>));
  spawner.addRule(hostileRule(EntityKind::CaveGrumble, 3, canSpawn<EntityKind::CaveGrumble>));
  spawner.addRule(hostileRule(EntityKind::Zombie, 3, canSpawn<EntityKind::Zombie>));
  spawner.addRule(hostileRule(EntityKind::Mummy, 3, canSpawn<EntityKind::Mummy>));
}
