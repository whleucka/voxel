#include "biome/plants.hpp"
#include "block/block_data.hpp"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"
#include "core/settings.hpp"
#include "util/worldgen_rng.hpp"
#include <glm/gtc/noise.hpp>

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

namespace {

bool isLeaf(BlockType b) {
  return b == BlockType::OAK_LEAF || b == BlockType::PINE_LEAF || b == BlockType::PALM_LEAF ||
         b == BlockType::CHERRY_LEAF;
}

// Smooth noise in 0..1 at a world column, `salt` keeping fields apart.
float field(glm::vec2 world, float scale, float salt) {
  const glm::vec2 p = (world + g_settings.noise_offset) * scale + glm::vec2(salt, -salt);
  return glm::perlin(p) * 0.5f + 0.5f;
}

} // namespace

void place(Chunk &chunk, BiomeType biome, const caves::Surfaces &surfaces) {
  uint64_t h = worldgen::mix(g_settings.world_seed ^ 0x91A7u);
  h = worldgen::mix(h ^ static_cast<uint32_t>(chunk.getPos().x));
  h = worldgen::mix(h ^ static_cast<uint32_t>(chunk.getPos().y));
  std::mt19937 rng(static_cast<uint32_t>(h ^ (h >> 32)));
  auto oneIn = [&](unsigned odds) { return rng() % odds == 0; };
  auto pick = [&](const std::vector<BlockType> &from) {
    return from[rng() % from.size()];
  };
  const Wild &wild = wildFor(biome);
  const float patch = biome == BiomeType::MOUNTAIN ? kMountainFlowerPatch : kFlowerPatch;

  for (int z = 0; z < kChunkDepth; ++z)
    for (int x = 0; x < kChunkWidth; ++x) {
      const glm::vec2 world(chunk.getPos().x * kChunkWidth + x,
                            chunk.getPos().y * kChunkDepth + z);
      const int top = surfaces[x + kChunkWidth * z]; // first cell above the ground
      const int g = top - 1;
      if (g < 1 || top + kMaxCactus >= kChunkHeight) continue;
      const BlockType ground = chunk.at(x, g, z);
      const BlockType above = chunk.at(x, top, z);

      if (ground == BlockType::GRASS && above == BlockType::AIR) {
        bool under_pine = false;
        for (int y = top + 1; y < std::min(top + 24, kChunkHeight) && !under_pine; ++y)
          under_pine = chunk.at(x, y, z) == BlockType::PINE_LEAF;
        BlockType plant = BlockType::AIR;
        if (biome == BiomeType::MOUNTAIN && under_pine && oneIn(kForestMushroomOdds))
          plant = oneIn(2) ? BlockType::RED_MUSHROOM : BlockType::BROWN_MUSHROOM;
        else if (biome == BiomeType::TROPICAL && oneIn(kBushOdds))
          plant = BlockType::BERRY_BUSH;
        else if (!wild.flowers.empty() && field(world, 0.08f, 17.0f) > 1.0f - patch &&
                 oneIn(kFlowerOdds)) {
          // A patch is all one kind of flower.
          const float kind = field(world, 0.02f, 41.0f);
          const size_t i = std::min(wild.flowers.size() - 1,
                                    static_cast<size_t>(kind * 1.6f * wild.flowers.size() -
                                                        0.3f * wild.flowers.size()));
          plant = wild.flowers[i];
        } else if (!wild.greens.empty() && oneIn(kGreensOdds)) {
          plant = pick(wild.greens);
        }
        if (plant != BlockType::AIR) chunk.at(x, top, z) = plant;
      } else if (ground == BlockType::SAND && above == BlockType::AIR) {
        if (biome == BiomeType::DESERT) {
          // A cactus stands alone: nothing beside it to prick through.
          auto clear = [&](int dx, int dz) {
            const int nx = x + dx, nz = z + dz;
            if (nx < 0 || nx >= kChunkWidth || nz < 0 || nz >= kChunkDepth) return false;
            return chunk.at(nx, top, nz) == BlockType::AIR;
          };
          if (oneIn(kCactusOdds) && clear(1, 0) && clear(-1, 0) && clear(0, 1) && clear(0, -1)) {
            const int tall = 1 + static_cast<int>(rng() % kMaxCactus);
            for (int y = top; y < top + tall && chunk.at(x, y, z) == BlockType::AIR; ++y)
              chunk.at(x, y, z) = BlockType::CACTUS;
          } else if (oneIn(kDeadBushOdds)) {
            chunk.at(x, top, z) = BlockType::DEAD_BUSH;
          }
        } else if (g == kSeaLevel && oneIn(kShellOdds)) {
          chunk.at(x, top, z) = BlockType::SHELLS; // just above the waterline
        }
      } else if (biome == BiomeType::OCEAN && above == BlockType::WATER &&
                 g + kCoralDepth < kSeaLevel && blocksMovement(ground) &&
                 field(world, 0.06f, 73.0f) > 1.0f - kReefPatch && oneIn(kCoralOdds)) {
        // A reef: coral of a colour by patch, now and then two high.
        const float colour = field(world, 0.12f, 97.0f);
        const BlockType coral = colour < 0.4f   ? BlockType::PINK_CORAL
                                : colour < 0.6f ? BlockType::BLUE_CORAL
                                                 : BlockType::YELLOW_CORAL;
        chunk.at(x, top, z) = coral;
        if (oneIn(3) && chunk.at(x, top + 1, z) == BlockType::WATER &&
            top + 1 + kCoralDepth <= kSeaLevel)
          chunk.at(x, top + 1, z) = coral;
      }

      // Mushrooms in the dark on cave floors, under the ground.
      for (int y = caves::kFloor; y < g - 3; ++y) {
        if (chunk.at(x, y, z) != BlockType::AIR) continue;
        const BlockType floor = chunk.at(x, y - 1, z);
        if (!isSolidGround(floor) || floor == BlockType::GLOW_CRYSTAL || isLeaf(floor)) continue;
        if (oneIn(kCaveMushroomOdds)) chunk.at(x, y, z) = BlockType::BROWN_MUSHROOM;
      }
    }
}

} // namespace plants
