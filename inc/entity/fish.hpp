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

// ─── Fishing ───────────────────────────────────────────────────────────

constexpr int kBiteMinMs = 5000;
constexpr int kBiteMaxMs = 15000;
constexpr int kBiteWindowMs = 1200;
constexpr float kFishNearBobber = 6.0f;     // a fish this close halves the wait
constexpr float kLineBreakDistance = 24.0f; // player to bobber
constexpr float kCastSpeed = 12.0f;         // along the aim, plus kCastLift up
constexpr float kCastLift = 3.0f;
constexpr float kTreasureChance = 0.15f;

// How long until the next bite: 5-15 s, or half that with a fish nearby.
int biteWaitMs(Rng &rng, bool fish_near);
// The fish that lives in the water at `cell`: Glowfin in a dark pool, else
// the open-water fish of its biome.
ItemId fishFor(const World &world, glm::ivec3 cell);
// What a rod brings up from the water at `cell`: mostly its fish, sometimes
// treasure.
ItemStack rollCatch(const World &world, glm::ivec3 cell, Rng &rng);
