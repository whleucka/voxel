#include "forest_biome.hpp"
#include "block_type.hpp"
#include "terrain_generator.hpp"
#include "world_constants.hpp"
#include <glm/gtc/noise.hpp>

static BlockType generateInternalBlock(int x, int y, int z, int world_x,
                                       int world_z) {
  const double stone_noise =
      glm::perlin(glm::vec3((world_x * Chunk::W + x) * 0.55, y * 0.25,
                            (world_z * Chunk::L + z) * 0.25));
  const double bedrock_noise =
      glm::perlin(glm::vec3((world_x * Chunk::W + x) * 0.42, y * 0.02,
                            (world_z * Chunk::L + z) * 0.42));

  if ((bedrock_noise > 0.1 && y <= 6) || (y < 5)) {
    return BlockType::BEDROCK;
  } else if ((stone_noise > 0.4) || (y >= 5 && y <= 15)) {
    return BlockType::STONE;
  }
  return BlockType::DIRT;
}

static BlockType generateTopBlock(int x, int y, int z, int world_x,
                                  int world_z) {
  const double snow_noise = glm::perlin(glm::vec2(
      (world_x * Chunk::W + x) * 0.02, (world_z * Chunk::L + z) * 0.02));
  const int block_rand = (rand() % 100) + 1;

  if (y > SNOW_LEVEL + snow_noise * 5.0) {
    if (block_rand <= 95) {
      return BlockType::SNOW;
    } else if (block_rand <= 98) {
      return BlockType::SNOW_STONE;
    } else {
      return BlockType::SNOW_DIRT;
    }
  } else if (y > SNOW_LEVEL) {
    return BlockType::SNOW;
  } else if (y <= SEA_LEVEL) {
    if (y < SEA_LEVEL - 5) {
      return BlockType::SANDSTONE;
    } else {
      return BlockType::SAND;
    }
  } else {
    if (y < SNOW_LEVEL) {
      if (y > SNOW_LEVEL - 2) {
        if (block_rand <= 80) {
          return BlockType::SNOW_STONE;
        } else {
          return BlockType::SNOW_DIRT;
        }
      } else {
        if (block_rand <= 5) {
          return BlockType::COBBLESTONE;
        } else if (block_rand <= 15) {
          return BlockType::STONE;
        } else {
          return BlockType::GRASS;
        }
      }
    }
  }
  return BlockType::DIRT;
}

void ForestBiome::generateTerrain(Chunk &chunk) {
  for (int x = 0; x < Chunk::W; ++x) {
    for (int z = 0; z < Chunk::L; ++z) {
      const int world_x = chunk.world_x * Chunk::W + x;
      const int world_z = chunk.world_z * Chunk::L + z;

      int height = TerrainGenerator::getHeight(world_x, world_z);

      for (int y = 0; y < height; y++) {
        BlockType type;
        if (y == height - 1) {
          type = generateTopBlock(x, y, z, chunk.world_x, chunk.world_z);
        } else {
          type = generateInternalBlock(x, y, z, chunk.world_x, chunk.world_z);
        }
        chunk.setBlock(x, y, z, type);
      }

      for (int y = height; y < SEA_LEVEL; y++) {
        if (chunk.getBlock(x, y, z) == BlockType::AIR) {
          chunk.setBlock(x, y, z, BlockType::WATER);
        }
      }

      const double cloud_noise =
          glm::perlin(glm::vec2((chunk.world_x * Chunk::W + x) * 0.03245,
                                (chunk.world_z * Chunk::L + z) * 0.05376));
      if (cloud_noise > 0.4) {
        chunk.setBlock(x, CLOUD_LEVEL, z, BlockType::CLOUD);
      }
    }
  }
}

void ForestBiome::spawnDecorations(Chunk &chunk) {
  for (int x = TREE_BORDER_THRESHOLD; x <= Chunk::W - 1 - TREE_BORDER_THRESHOLD;
       ++x) {
    for (int z = TREE_BORDER_THRESHOLD;
         z <= Chunk::L - 1 - TREE_BORDER_THRESHOLD; ++z) {
      int y = 0;
      for (int i = Chunk::H - 1; i > 0; --i) {
        if (chunk.getBlock(x, i, z) != BlockType::AIR) {
          y = i;
          break;
        }
      }

      if (y > 0 && chunk.getBlock(x, y, z) == BlockType::GRASS) {
        const double tree_noise =
            (glm::perlin(glm::vec3((chunk.world_x * Chunk::W + x) * 0.8,
                                   y * 0.02,
                                   (chunk.world_z * Chunk::L + z) * 0.8)) *
                 0.5 +
             0.5);

        if (tree_noise > 0.6) {
          // set trunk
          int h = (rand() % 6) + 10;
          for (int j = 1; j <= h; j++) {
            chunk.setBlock(x, y + j, z, BlockType::TREE);
          }

          // set leaves
          int radius = (rand() % 2) + 4;
          int top = y + h;
          int t_h = (rand() % 3) + h - radius; // leaf height
          int t_d = (rand() % 2) + 4;          // leaf depth

          for (int dy = -t_d; dy <= t_h; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
              for (int dz = -radius; dz <= radius; dz++) {
                if (rand() % 100 > 35)
                  continue;
                if (dx * dx + dz * dz + dy * dy <= radius * radius + 1) {
                  if (chunk.getBlock(x + dx, top + dy, z + dz) !=
                      BlockType::TREE)
                    chunk.setBlock(x + dx, top + dy, z + dz,
                                   BlockType::TREE_LEAF);
                }
              }
            }
          }
        }
      }
    }
  }
}
