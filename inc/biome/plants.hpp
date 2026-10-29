#pragma once

#include "biome/biome_type.hpp"
#include "block/block_type.hpp"
#include <optional>
#include <random>
#include <vector>

// What grows wild in each biome (worldgen v7), shared by world generation
// and compost, which sprouts the same plants in any world.
namespace plants {

struct Wild {
  std::vector<BlockType> greens;  // the common ones on grass: tall grass, ferns
  std::vector<BlockType> flowers; // on grass, now and then, in patches of a kind
  std::vector<BlockType> sand;    // on its sand
};
const Wild &wildFor(BiomeType biome);

// A plant compost sprouts on `ground` (grass or sand) in `biome`, if anything
// grows there: mostly greens, sometimes a flower; on desert sand, dead
// bushes and now and then a cactus; on other sand, shells.
std::optional<BlockType> sprout(BiomeType biome, BlockType ground, std::mt19937 &rng);

} // namespace plants
