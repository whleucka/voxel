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

      float biome_noise_val;
      int height = TerrainGenerator::getHeight(world_x, world_z, &biome_noise_val);

      // Use biome_noise to make oceans deeper
      // When biome_noise_val is low (e.g., 0.0-0.3), depth_factor is high (e.g., 1.0-0.7)
      // When biome_noise_val is high (e.g., 0.7-1.0), depth_factor is low (e.g., 0.3-0.0)
      float depth_factor = glm::smoothstep(0.0f, 1.0f, 1.0f - biome_noise_val);
      height = static_cast<int>(height * (1.0f - 0.7f * depth_factor)); // Reduce height by up to 70%

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
