#include "biome/plants.hpp"

namespace plants {

const Wild &wildFor(BiomeType biome) {
  using enum BlockType;
  static const Wild kPlains{{TALL_GRASS}, {POPPY, BUTTERCUP, CORNFLOWER}, {SHELLS}};
  static const Wild kCherry{{TALL_GRASS}, {PINK_TULIP, DAISY}, {SHELLS}};
  static const Wild kMountain{{FERN}, {LAVENDER}, {}};
  static const Wild kTropical{{TALL_GRASS}, {HIBISCUS}, {SHELLS}};
  static const Wild kDesert{{}, {}, {DEAD_BUSH, DEAD_BUSH, CACTUS}};
  switch (biome) {
    case BiomeType::CHERRY:   return kCherry;
    case BiomeType::MOUNTAIN: return kMountain;
    case BiomeType::TROPICAL: return kTropical;
    case BiomeType::DESERT:   return kDesert;
    default:                  return kPlains; // and the ocean's shores
  }
}

std::optional<BlockType> sprout(BiomeType biome, BlockType ground, std::mt19937 &rng) {
  const Wild &wild = wildFor(biome);
  auto pick = [&](const std::vector<BlockType> &from) -> std::optional<BlockType> {
    if (from.empty()) return std::nullopt;
    return from[std::uniform_int_distribution<size_t>(0, from.size() - 1)(rng)];
  };
  if (ground == BlockType::SAND) return pick(wild.sand);
  if (ground != BlockType::GRASS) return std::nullopt;
  // Greens two times in three; a biome without any always flowers.
  if (wild.greens.empty() || std::uniform_int_distribution<int>(0, 2)(rng) == 0)
    if (auto flower = pick(wild.flowers)) return flower;
  return pick(wild.greens);
}

} // namespace plants
