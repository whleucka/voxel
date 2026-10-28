#include "item/farming.hpp"

ItemId seedFor(CropKind kind) {
  return static_cast<ItemId>(static_cast<uint16_t>(ItemId::WHEAT_SEEDS) +
                             static_cast<uint16_t>(kind));
}

std::optional<CropKind> cropForSeed(ItemId seed) {
  if (seed < ItemId::WHEAT_SEEDS || seed > ItemId::STRAWBERRY_SEEDS) return std::nullopt;
  return static_cast<CropKind>(static_cast<uint16_t>(seed) -
                               static_cast<uint16_t>(ItemId::WHEAT_SEEDS));
}

bool tillable(BlockType b) { return b == BlockType::GRASS || b == BlockType::DIRT; }

std::optional<ItemId> seedFromTilling(BlockType tilled, std::mt19937 &rng) {
  if (tilled != BlockType::GRASS) return std::nullopt;
  if (std::uniform_int_distribution<int>(1, kSeedOneIn)(rng) != 1) return std::nullopt;
  return std::uniform_int_distribution<int>(0, 1)(rng) == 0 ? ItemId::WHEAT_SEEDS
                                                            : ItemId::CARROT_SEEDS;
}

std::vector<ItemStack> harvest(BlockType crop, std::mt19937 &rng) {
  if (!isCrop(crop)) return {};
  const ItemId seed = seedFor(cropKind(crop));
  if (!cropRipe(crop)) return {{seed, 1}};
  auto between = [&](int lo, int hi) {
    return static_cast<uint16_t>(std::uniform_int_distribution<int>(lo, hi)(rng));
  };
  switch (cropKind(crop)) {
    case CropKind::Wheat:  return {{ItemId::WHEAT, 1}, {seed, between(1, 2)}};
    case CropKind::Carrot: return {{ItemId::CARROT, between(2, 3)}, {seed, 1}};
    case CropKind::Strawberry:
    default:               return {{ItemId::STRAWBERRY, between(2, 3)}, {seed, 1}};
  }
}

int stageMs(std::mt19937 &rng) {
  return std::uniform_int_distribution<int>(kStageMs - kStageJitterMs,
                                            kStageMs + kStageJitterMs)(rng);
}
