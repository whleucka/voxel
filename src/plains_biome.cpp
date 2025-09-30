#include "plains_biome.hpp"
#include "block_type.hpp"
#include "oak_tree_generator.hpp"
#include "terrain_generator.hpp"
#include "world_constants.hpp"
#include <glm/gtc/noise.hpp>

PlainsBiome::PlainsBiome()
    : m_tree_spawner(0.7, std::make_unique<OakTreeGenerator>()) {}

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

static BlockType generateTopBlock(int y) {
  if (y <= SEA_LEVEL) {
    if (y < SEA_LEVEL - 5) {
      return BlockType::SANDSTONE;
    } else {
      return BlockType::SAND;
    }
  } else {
    return BlockType::GRASS;
  }
}

void PlainsBiome::generateTerrain(Chunk &chunk) {
  for (int x = 0; x < Chunk::W; ++x) {
    for (int z = 0; z < Chunk::L; ++z) {
      const int world_x = chunk.world_x * Chunk::W + x;
      const int world_z = chunk.world_z * Chunk::L + z;

      int height = TerrainGenerator::getHeight(world_x, world_z);

      for (int y = 0; y < height; y++) {
        BlockType type;
        if (y == height - 1) {
          type = generateTopBlock(y);
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
    }
  }
}

void PlainsBiome::spawnDecorations(Chunk &chunk) {
  m_tree_spawner.spawn(chunk, [](Chunk &c, int x, int y, int z) {
    return c.getBlock(x, y, z) == BlockType::GRASS;
  });
}
