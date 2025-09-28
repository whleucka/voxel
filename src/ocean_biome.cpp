#include "ocean_biome.hpp"
#include "block_type.hpp"
#include "terrain_generator.hpp"
#include "world_constants.hpp"
#include <glm/gtc/noise.hpp>

void OceanBiome::generateTerrain(Chunk &chunk) {
  for (int x = 0; x < Chunk::W; ++x) {
    for (int z = 0; z < Chunk::L; ++z) {
      const int world_x = chunk.world_x * Chunk::W + x;
      const int world_z = chunk.world_z * Chunk::L + z;

      int height = TerrainGenerator::getHeight(world_x, world_z);

      for (int y = 0; y < height; y++) {
        if (y < 5) {
          chunk.setBlock(x, y, z, BlockType::BEDROCK);
        } else if (y < 20) {
          chunk.setBlock(x, y, z, BlockType::SANDSTONE);
        } else {
          chunk.setBlock(x, y, z, BlockType::SAND);
        }
      }

      for (int y = height; y < SEA_LEVEL; y++) {
        if (chunk.getBlock(x, y, z) == BlockType::AIR) {
          chunk.setBlock(x, y, z, BlockType::WATER);
        }
      }
    }
  }
}

void OceanBiome::spawnDecorations(Chunk &chunk) {
  // No decorations for oceans yet
}
