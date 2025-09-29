#include "desert_biome.hpp"
#include "block_type.hpp"
#include "terrain_generator.hpp"
#include "world_constants.hpp"
#include "palm_tree_generator.hpp"
#include <glm/gtc/noise.hpp>

DesertBiome::DesertBiome()
    : m_palm_tree_spawner(0.98, std::make_unique<PalmTreeGenerator>()) {}

static BlockType generateInternalBlock(int x, int y, int z, int world_x,
                                       int world_z) {
  const double bedrock_noise =
      glm::perlin(glm::vec3((world_x * Chunk::W + x) * 0.42, y * 0.02,
                            (world_z * Chunk::L + z) * 0.42));

  if ((bedrock_noise > 0.1 && y <= 6) || (y < 5)) {
    return BlockType::BEDROCK;
  }
  return BlockType::SANDSTONE;
}

static BlockType generateTopBlock() {
  return BlockType::SAND;
}

void DesertBiome::generateTerrain(Chunk &chunk) {
  for (int x = 0; x < Chunk::W; ++x) {
    for (int z = 0; z < Chunk::L; ++z) {
      const int world_x = chunk.world_x * Chunk::W + x;
      const int world_z = chunk.world_z * Chunk::L + z;

      int height = TerrainGenerator::getHeight(world_x, world_z);

      for (int y = 0; y < height; y++) {
        BlockType type;
        if (y == height - 1) {
          type = generateTopBlock();
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

void DesertBiome::spawnDecorations(Chunk &chunk) {
  m_palm_tree_spawner.spawn(chunk, [](Chunk &c, int x, int y, int z) {
    return c.getBlock(x, y, z) == BlockType::SAND;
  });
}
