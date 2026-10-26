#pragma once

#include "entity/entity_type.hpp"
#include "entity/spawner.hpp"
#include <glm/glm.hpp>

class World;

// Light levels (0-15) at an enemy's feet that allow it to appear.
constexpr int kEnemyMaxBlockLight = 7;      // a lit base is safe
constexpr int kCaveEnemyMaxBlockLight = 4;
constexpr int kSurfaceEnemyMinSkyLight = 8; // under open sky
constexpr int kEnemySpawnIntervalMs = 3000;

// Night is the time of day outside the daylight window (0.25-0.75).
bool isNight(float time_of_day);

// Whether `kind` may appear standing on `ground`: darkness, and for surface
// enemies the open sky at night (and the biome, if it has any), or for cave
// enemies no sky light at all, at any time.
bool enemyCanSpawn(EntityKind kind, const World &world, glm::ivec3 ground,
                   float time_of_day);

// One hostile spawn rule per enemy kind, with the Normal caps.
void registerEnemySpawns(Spawner &spawner);
