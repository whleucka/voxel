#include "desert_biome.hpp"
#include "block_type.hpp"
#include <glm/gtc/noise.hpp>

const int SEA_LEVEL = 50;
const int CLOUD_LEVEL = 200;

const float FREQ = 0.03f;
const float AMP = 35.0f;
const float OCTAVES = 4.0f;
const float LACUNARITY = 1.2f;
const float GAIN = 0.4f;

static double fbm(glm::vec3 p, int octaves, double lacunarity, double gain) {
  double sum = 0.0;
  double amplitude = 1.0;
  float frequency = 1.0;
  double norm = 0.0;

  for (int i = 0; i < octaves; i++) {
    sum += amplitude * (glm::perlin(p * frequency) * 0.5 + 0.5);
    norm += amplitude;
    amplitude *= gain;
    frequency *= lacunarity;
  }
  return sum / norm;
}

static BlockType generateInternalBlock(int x, int y, int z, int world_x, int world_z) {
  const double bedrock_noise = glm::perlin(
      glm::vec3((world_x * Chunk::W + x) * 0.42, y * 0.02, (world_z * Chunk::L + z) * 0.42));

  if ((bedrock_noise > 0.1 && y <= 6) || (y < 5)) {
    return BlockType::BEDROCK;
  }
  return BlockType::SANDSTONE;
}

static BlockType generateTopBlock(int x, int y, int z, int world_x, int world_z) {
    return BlockType::SAND;
}

void DesertBiome::generateTerrain(Chunk& chunk) {
    for (int x = 0; x < Chunk::W; ++x) {
        for (int z = 0; z < Chunk::L; ++z) {
            const int baseX = chunk.world_x * Chunk::W;
            const int baseZ = chunk.world_z * Chunk::L;

            glm::vec3 pos((baseX + x + 0.1f) * FREQ, (baseZ + z + 0.1f) * FREQ, 0.0);
            double hNoise = fbm(pos, OCTAVES, LACUNARITY, GAIN);

            glm::vec2 biome_pos((chunk.world_x * Chunk::W + x) * 0.001f,
                                (chunk.world_z * Chunk::L + z) * 0.001f);
            double biome_noise = glm::perlin(biome_pos) * 0.5 + 0.5;
            double mountain_factor = glm::smoothstep(0.42, 0.65, biome_noise);

            int perlin_height =
                static_cast<int>(
                    hNoise * 15 // flatter desert
                    + mountain_factor * 60
                    ) +
                AMP;

            for (int y = 0; y < perlin_height; y++) {
                BlockType type;
                if (y == perlin_height - 1) {
                    type = generateTopBlock(x, y, z, chunk.world_x, chunk.world_z);
                } else {
                    type = generateInternalBlock(x, y, z, chunk.world_x, chunk.world_z);
                }
                chunk.setBlock(x, y, z, type);
            }

            for (int y = perlin_height; y < SEA_LEVEL; y++) {
                if (chunk.getBlock(x, y, z) == BlockType::AIR) {
                    chunk.setBlock(x, y, z, BlockType::WATER);
                }
            }
            
            const double cloud_noise = glm::perlin(
                glm::vec2((chunk.world_x * Chunk::W + x) * 0.03245, (chunk.world_z * Chunk::L + z) * 0.05376));
            if (cloud_noise > 0.4) {
                chunk.setBlock(x, CLOUD_LEVEL, z, BlockType::CLOUD);
            }
        }
    }
}

void DesertBiome::spawnDecorations(Chunk& chunk) {
    // No decorations for desert yet, maybe cacti later
}
