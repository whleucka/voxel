#include "entity/animals.hpp"
#include "biome/biome_manager.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>

bool animalCanSpawn(EntityKind kind, const World &world, glm::ivec3 ground,
                    float time_of_day) {
  if (!isAnimal(kind)) return false;
  if (time_of_day < kDaylightStart || time_of_day > kDaylightEnd) return false;

  const AnimalDef &a = *entityType(kind).animal;
  BlockType below = world.getBlockAt(glm::vec3(ground) + 0.5f);
  if (std::find(a.ground.begin(), a.ground.end(), below) == a.ground.end())
    return false;

  int cx = static_cast<int>(std::floor(static_cast<float>(ground.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(ground.z) / kChunkDepth));
  BiomeType biome = BiomeManager::getBiomeForChunk(cx, cz);
  return std::find(a.biomes.begin(), a.biomes.end(), biome) != a.biomes.end();
}

namespace {

// SpawnRule takes a plain function pointer, so one instantiation per kind.
template <EntityKind K>
bool canSpawn(const World &world, glm::ivec3 ground, float time_of_day) {
  return animalCanSpawn(K, world, ground, time_of_day);
}

} // namespace

void registerAnimalSpawns(Spawner &spawner) {
  spawner.addRule({EntityKind::Woolly, kAnimalsPerPlayer, kAnimalSpawnIntervalMs,
                   canSpawn<EntityKind::Woolly>});
  spawner.addRule({EntityKind::Bunny, kAnimalsPerPlayer, kAnimalSpawnIntervalMs,
                   canSpawn<EntityKind::Bunny>});
  spawner.addRule({EntityKind::ShellTurtle, kAnimalsPerPlayer, kAnimalSpawnIntervalMs,
                   canSpawn<EntityKind::ShellTurtle>});
  spawner.addRule({EntityKind::BlossomDeer, kAnimalsPerPlayer, kAnimalSpawnIntervalMs,
                   canSpawn<EntityKind::BlossomDeer>});
}
