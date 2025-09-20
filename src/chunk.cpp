#include "chunk.hpp"
#include "block_type.hpp"
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/noise.hpp>

Chunk::Chunk(int32_t world_x, int32_t world_z)
    : world_x(world_x), world_z(world_z) {
  blocks.fill(BlockType::AIR);
}

BlockType Chunk::getBlock(int x, int y, int z) const {
  if (x < 0 || x >= W || y < 0 || y >= H || z < 0 || z >= L) {
    return BlockType::AIR;
  }
  return blocks[getIndex(x, y, z)];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
  if (x < 0 || x >= W || y < 0 || y >= H || z < 0 || z >= L) {
    return;
  }
  blocks[getIndex(x, y, z)] = type;
}

void Chunk::generateTree(int x, int y, int z) {
  int h = (rand() % 4) + 7;
  for (int j = 1; j <= h; j++) {
    setBlock(x, y + j, z, BlockType::TREE);
  }

  // leaves
  int radius = (rand() % 2) + 4;
  int top = y + h;

  for (int dy = -2; dy <= 2; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      for (int dz = -radius; dz <= radius; dz++) {
        // add some randomness
        if (rand()%5==0) continue;
        // little sphere-ish shape
        if (dx*dx + dz*dz + dy*dy <= radius*radius + 1) {
          if (getBlock(x + dx, top + dy, z + dz) != BlockType::TREE)
            setBlock(x + dx, top + dy, z + dz, BlockType::TREE_LEAF);
        }
      }
    }
  }
}

void Chunk::generateChunk() {
  const int SEA_LEVEL = 50;  // water below y
  const int SNOW_LEVEL = 80; // snow above y
  const float FREQ = 0.03f;  // noise frequency
  const float AMP = 35.0f;   // height amplitude
  const float OCTAVES = 4.0f;
  const float LACUNARITY = 1.2f;
  const float GAIN = 0.4f;
  const int baseX = world_x * W;
  const int baseZ = world_z * L;

  for (int x = 0; x < W; ++x) {
    for (int z = 0; z < L; ++z) {
      /** GENERATE TERRAIN */
      // multi-octave Perlin noise (fBm)
      auto fbm = [](glm::vec2 p, int octaves, double lacunarity, double gain) {
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
        return sum / norm; // normalize back to [0,1]
      };

      // Sample noise at world position
      glm::vec2 pos((baseX + x + 0.1f) * FREQ, (baseZ + z + 0.1f) * FREQ);

      // classic smooth fBm
      double hNoise = fbm(pos, OCTAVES, LACUNARITY, GAIN);

      // --- large scale biome noise ---
      glm::vec2 biome_pos((world_x * W + x) * 0.001f, // much lower frequency
                          (world_z * L + z) * 0.001f);

      double biome_noise = glm::perlin(biome_pos) * 0.5 + 0.5; // [0,1]

      // Emphasize mountain regions (smoothstep makes it "rare")
      // I'll probably forget what this means:
      // Takes your biome noise, says “only the highest values should count as
      // mountains,” and applies a smooth ramp so that instead of an ugly cliff
      // at x, you get a nice smooth transition into mountains.
      double mountain_factor = glm::smoothstep(0.42, 0.65, biome_noise);

      int perlin_height =
          static_cast<int>(
              hNoise * 25            // base hills
              + mountain_factor * 80 // extra mountains only where biome is high
              ) +
          AMP;

      for (int y = 0; y < perlin_height; y++) {

        const double stone_noise = glm::perlin(glm::vec3(
            (world_x * W + x) * 0.55, y * 0.25, (world_z * L + z) * 0.25));
        const double bedrock_noise = glm::perlin(glm::vec3(
            (world_x * W + x) * 0.42, y * 0.02, (world_z * L + z) * 0.42));
        const double tree_noise = glm::perlin(glm::vec3(
            (world_x * W + x) * 0.4, y * 0.02, (world_z * L + z) * 0.4));

        BlockType type = BlockType::AIR;

        // Figure out top block
        if (y == perlin_height - 1) {
          double snow_noise = glm::perlin(
              glm::vec2((world_x * W + x) * 0.02, (world_z * L + z) * 0.02));
          int block_rand = (rand() % 100) + 1;
          if (perlin_height > SNOW_LEVEL + snow_noise * 5.0) {
            if (block_rand <= 95) {
              type = BlockType::SNOW;
            } else if (block_rand <= 98) {
              type = BlockType::SNOW_STONE;
            } else {
              type = BlockType::SNOW_DIRT;
            }
          } else if (perlin_height > SNOW_LEVEL) {
            type = BlockType::SNOW;
          } else if (perlin_height <= SEA_LEVEL) {
            if (perlin_height < SEA_LEVEL - 5) {
              type = BlockType::SANDSTONE;
            } else {
              type = BlockType::SAND;
            }
          } else {
            if (y < SNOW_LEVEL) {
              if (y > SNOW_LEVEL - 2) {
                if (block_rand <= 80) {
                  type = BlockType::SNOW_STONE;
                } else {
                  type = BlockType::SNOW_DIRT;
                }
              } else {
                if (block_rand <= 5) {
                  type = BlockType::COBBLESTONE;
                } else if (block_rand <= 15) {
                  type = BlockType::STONE;
                } else if (block_rand <= 90) {
                  type = BlockType::GRASS;
                  if (perlin_height <= SNOW_LEVEL - 3 && tree_noise > 0.6) {
                    generateTree(x, y, z);
                  }
                } else {
                  type = BlockType::DIRT;
                }
              }
            }
          }
        } else if ((bedrock_noise > 0.1 && y <= 6) || (y < 5)) {
          type = BlockType::BEDROCK;
        } else if ((stone_noise > 0.4) || (y >= 5 && y <= 15)) {
          type = BlockType::STONE;
        } else {
          type = BlockType::DIRT;
        }

        setBlock(x, y, z, type);
      }

      // Fill water pass
      for (int y = perlin_height; y < SEA_LEVEL; y++) {
        BlockType type = getBlock(x, y, z);
        if (type == BlockType::AIR) {
          setBlock(x, y, z, BlockType::WATER);
        }
      }
    }
  }
}
