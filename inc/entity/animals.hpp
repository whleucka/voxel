#pragma once

#include "entity/entity_type.hpp"
#include "entity/spawner.hpp"
#include <glm/glm.hpp>

class World;

// Animals spawn in daylight only: time of day as a fraction, 0.5 = noon.
constexpr float kDaylightStart = 0.25f;
constexpr float kDaylightEnd = 0.75f;
constexpr int kAnimalsPerPlayer = 3;     // of each kind
constexpr int kAnimalSpawnIntervalMs = 4000;
constexpr int kStraysPerPlayer = 1;      // of each pet kind
constexpr int kStraySpawnIntervalMs = 8000;

// Whether `kind` may appear standing on `ground` (a solid block with headroom)
// at this time of day: its biome, its ground block, daylight.
bool animalCanSpawn(EntityKind kind, const World &world, glm::ivec3 ground,
                    float time_of_day);

// One spawn rule per animal kind, and one per pet kind (strays).
void registerAnimalSpawns(Spawner &spawner);
