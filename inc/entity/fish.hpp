#pragma once

#include "entity/entity_type.hpp"
#include "entity/spawner.hpp"
#include <glm/glm.hpp>

class World;

constexpr int kFishPerPlayer = 4; // of each kind
constexpr int kFishSpawnIntervalMs = 4000;
// Sky light just above the water: open water is bright, a cave pool dark.
constexpr int kOpenWaterMinSkyLight = 8;
constexpr int kCaveWaterMaxSkyLight = 0;

// The sky light in the first non-water cell above `cell`.
int skyAboveWater(const World &world, glm::ivec3 cell);

// Whether `kind` may appear in the water cell `cell`: water at least 2 deep
// there, its biome for open-water fish, open sky or a roof.
bool fishCanSpawn(EntityKind kind, const World &world, glm::ivec3 cell);

// One spawn rule per fish kind.
void registerFishSpawns(Spawner &spawner);
