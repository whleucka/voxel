#pragma once

#include "block/crop.hpp"
#include "item/item_stack.hpp"
#include <glm/glm.hpp>
#include <optional>
#include <random>
#include <vector>

// Farming rules: what a hoe, seeds and a harvest give, how fast crops grow
// and what keeps soil wet. Pure, so the Game and the tests share them.

// ─── Seeds ─────────────────────────────────────────────────────────────

ItemId seedFor(CropKind kind);
std::optional<CropKind> cropForSeed(ItemId seed);

// ─── Tilling ───────────────────────────────────────────────────────────

// What a hoe turns into farmland: grass and dirt.
bool tillable(BlockType b);

// Tilling grass turns up a seed one time in kSeedOneIn, wheat or carrot with
// even odds; dirt never does.
constexpr int kSeedOneIn = 6;
std::optional<ItemId> seedFromTilling(BlockType tilled, std::mt19937 &rng);

// ─── Harvest ───────────────────────────────────────────────────────────

// What breaking a crop gives: its produce and seeds when ripe, else just
// its seed back.
std::vector<ItemStack> harvest(BlockType crop, std::mt19937 &rng);

// ─── Growing ───────────────────────────────────────────────────────────

// A crop on wet soil goes up a stage after kStageMs give or take
// kStageJitterMs, so sprout to ripe takes about a day (20 minutes); a field
// planted at once doesn't all change at once.
constexpr int kStageMs = 400'000;
constexpr int kStageJitterMs = 100'000;
int stageMs(std::mt19937 &rng);

// Watered soil stays wet for a day.
constexpr int kWetMs = 1'200'000;

// Soil with water within kWaterReach blocks sideways, at its own level or one
// below, never dries.
constexpr int kWaterReach = 4;

template <class BlockAt> bool nearWater(glm::ivec3 soil, BlockAt &&at) {
  for (int dy = -1; dy <= 0; ++dy)
    for (int dz = -kWaterReach; dz <= kWaterReach; ++dz)
      for (int dx = -kWaterReach; dx <= kWaterReach; ++dx)
        if (at(soil + glm::ivec3(dx, dy, dz)) == BlockType::WATER) return true;
  return false;
}

// A splash from the watering can wets the farmland in the 3x3 round where it
// lands.
constexpr int kSplashRadius = 1;
