#pragma once

#include "biome/biome_type.hpp"
#include "biome/caves.hpp"
#include "block/block_type.hpp"
#include <optional>
#include <random>
#include <vector>

class Chunk;

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

// How often things grow (worldgen v7), as one in this many of the columns
// that could have them.
constexpr unsigned kGreensOdds = 16;      // tall grass or ferns on grass
constexpr unsigned kFlowerOdds = 6;       // flowers, inside a patch
constexpr float kFlowerPatch = 0.3f;      // patch noise above this
constexpr float kMountainFlowerPatch = 0.45f; // wider on the mountain's thin grass
constexpr unsigned kBushOdds = 150;       // berry bushes in the tropics
constexpr unsigned kForestMushroomOdds = 10; // under mountain pines
constexpr unsigned kCactusOdds = 120;     // on desert sand
constexpr unsigned kDeadBushOdds = 60;
constexpr unsigned kShellOdds = 20;       // on the beach, at the waterline
constexpr unsigned kCoralOdds = 3;        // on the sea floor, inside a reef
constexpr float kReefPatch = 0.25f;
constexpr int kCoralDepth = 3;            // at least this much water over it
constexpr unsigned kCaveMushroomOdds = 80; // on a cave floor
constexpr int kMaxCactus = 3;             // tallest

// Everything that grows wild in a chunk of `biome` (worldgen v7): its
// plants on the ground, shells at the waterline, coral on the sea floor and
// mushrooms on cave floors. Last of all, on its own random stream, so the
// rest of the chunk comes out exactly as in v6.
void place(Chunk &chunk, BiomeType biome, const caves::Surfaces &surfaces);

} // namespace plants
