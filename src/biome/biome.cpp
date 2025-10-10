#include "biome/biome.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"
#include "util/noise.hpp"
#include <glm/fwd.hpp>

void Biome::generateTerrain(Chunk &chunk) {
  glm::vec2 pos = chunk.getPos();
  for (int x = 0; x < kChunkWidth; x++) {
    for (int z = 0; z < kChunkDepth; z++) {
      const int world_x = pos[0] * kChunkWidth + x;
      const int world_z = pos[1] * kChunkDepth + z;
      int h = getHeight({world_x, world_z});
      if (h >= kChunkHeight) {
        h = kChunkHeight - 1;
      }
      for (int y = 0; y < kChunkHeight; ++y) {
        if (y < h) {
          if (y == h - 1) {
            BlockType top = generateTopBlock(y);
            if (y >= kSnowLevel) {
              if (top == BlockType::STONE) {
                top = BlockType::SNOW_STONE;
              } else if (top == BlockType::DIRT || top == BlockType::GRASS) {
                top = BlockType::SNOW_DIRT;
              }
            }
            if (y <= kSeaLevel) {
              if (y > kSeaLevel - 3) {
                top = BlockType::SAND;
              } else {
                top = BlockType::SANDSTONE;
              }
            }
            chunk.at(x, y, z) = top;
          } else {
            chunk.at(x, y, z) = generateInternalBlock(x, y, z);
          }
        } else {
          chunk.at(x, y, z) = BlockType::AIR;
        }
      }
    }
  }
}

void Biome::fillWater(Chunk &chunk) {
  glm::vec2 pos = chunk.getPos();
  for (int x = 0; x < kChunkWidth; ++x) {
    for (int z = 0; z < kChunkDepth; ++z) {
      const int world_x = pos.x * kChunkWidth + x;
      const int world_z = pos.y * kChunkDepth + z;
      int h = getHeight({world_x, world_z});

      for (int y = h; y <= kSeaLevel && y < kChunkHeight; ++y) {
        if (chunk.at(x, y, z) == BlockType::AIR) {
          chunk.at(x, y, z) = BlockType::WATER;
        }
      }
    }
  }
}

void Biome::generateMinerals(Chunk &chunk) {
  glm::vec2 pos = chunk.getPos();

  for (int x = 0; x < kChunkWidth; ++x) {
    for (int z = 0; z < kChunkDepth; ++z) {
      for (int y = 6; y < kChunkHeight - 5; ++y) {
        BlockType &block = chunk.at(x, y, z);

        if (block == BlockType::STONE) {
          float n = glm::perlin(glm::vec3(
            (pos.x * kChunkWidth + x) * 0.05f,
            y * 0.05f,
            (pos.y * kChunkDepth + z) * 0.05f
          ));

          // RARE ORES
          if (n >= 0.75f) {
            int gem_chance = rand() % 10 + 1;
            if (gem_chance <= 3)
              block = BlockType::EMERALD_ORE;
            else if (gem_chance <= 6)
              block = BlockType::RUBY_ORE;
            else
              block = BlockType::GOLD_ORE;
          } else if (n >= 0.6f) {
            block = BlockType::IRON_ORE;
          } else if (n >= 0.45f) {
            block = BlockType::COAL_ORE;
          }
        }
      }
    }
  }
}

BlockType Biome::generateInternalBlock(int, int y, int) {
  int rando = rand() % 10 + 1;

  // Always bedrock at the bottom few layers
  if (y <= 5) {
    return rando > 9 ? BlockType::STONE : BlockType::BEDROCK;
  }

  // The deeper you go, the denser the rock
  if (y < kSeaLevel - 20) {
    return rando > 9 ? BlockType::BEDROCK : BlockType::STONE;
  }

  if (y < kSeaLevel - 10) {
    return rando > 9 ? BlockType::STONE : BlockType::COBBLESTONE;
  }

  // Upper layers are dirt
  return rando > 8 ? BlockType::COBBLESTONE : BlockType::DIRT;
}

int Biome::getHeight(glm::vec2 pos, float *biome_noise_out) {
  constexpr int HMIN = 2;
  constexpr int HMAX = kChunkHeight - 1;
  constexpr int SEA = kSeaLevel;
  const float sea_norm = float(SEA) / float(HMAX);

  // --- Continents (0=ocean, 1=land) ---
  float c = glm::clamp(glm::perlin(pos * 0.0008f) * 0.5f + 0.5f, 0.0f, 1.0f);

  // --- Local detail ---
  glm::vec3 p((pos.x + 0.1f) * kBiomeFreq, (pos.y + 0.1f) * kBiomeFreq, 0.0f);
  double n = fbm(p, kBiomeOctaves, kBiomeLacunarity, kBiomeGain);
  float e = glm::clamp(float(n * 0.5 + 0.5), 0.0f, 1.0f);

  // --- Biome noise (plains vs mountains) ---
  glm::vec2 bpos = pos * 0.0045f;
  float biome_n = glm::clamp(glm::perlin(bpos) * 0.5f + 0.5f, 0.0f, 1.0f);
  if (biome_noise_out)
    *biome_noise_out = biome_n;

  // --- Ocean floor ---
  float ocean_floor = glm::mix(0.05f, sea_norm - 0.15f, e);
  ocean_floor += glm::perlin(pos * 0.003f) * 0.015f;
  ocean_floor = glm::clamp(ocean_floor, 0.0f, sea_norm - 0.02f);

  // --- Land elevation ---
  float plains = glm::mix(sea_norm - 0.02f, 0.60f, e);
  float peaks = glm::mix(0.55f, 0.95f, std::pow(e, 1.4f));
  float mountain = glm::mix(plains, peaks, glm::smoothstep(0.5f, 0.85f, biome_n));

  // --- Blend ocean vs land ---
  float coast = glm::smoothstep(0.25f, 0.65f, c);
  float elev_norm = glm::mix(ocean_floor, mountain, coast);
  elev_norm = glm::clamp(elev_norm, float(HMIN) / float(HMAX), 1.0f);

  // --- Convert to voxel height ---
  int h = int(std::round(elev_norm * HMAX));
  return glm::clamp(h, HMIN, HMAX);
}
