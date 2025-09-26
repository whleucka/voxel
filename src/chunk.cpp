#include "chunk.hpp"
#include "block_type.hpp"
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/noise.hpp>

const int CLOUD_LEVEL = 200;         // clouds at y
const int SEA_LEVEL = 50;            // water below y
const int SNOW_LEVEL = 80;           // snow above y

const float FREQ = 0.03f;            // noise frequency
const float AMP = 35.0f;             // height amplitude
const float OCTAVES = 4.0f;          // terrain octaves
const float LACUNARITY = 1.2f;       // terrain lacunarity
const float GAIN = 0.4f;             // terrain gain
const int TREE_BORDER_THRESHOLD = 5; // trees spawn in centre of chunk

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
  return sum / norm; // normalize back to [0,1]
}

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

BlockType Chunk::generateInternalBlock(int x, int y, int z) {
  const double stone_noise = glm::perlin(
      glm::vec3((world_x * W + x) * 0.55, y * 0.25, (world_z * L + z) * 0.25));
  const double bedrock_noise = glm::perlin(
      glm::vec3((world_x * W + x) * 0.42, y * 0.02, (world_z * L + z) * 0.42));

  if ((bedrock_noise > 0.1 && y <= 6) || (y < 5)) {
    return BlockType::BEDROCK;
  } else if ((stone_noise > 0.4) || (y >= 5 && y <= 15)) {
    return BlockType::STONE;
  }
  return BlockType::DIRT;
}

BlockType Chunk::generateTopBlock(int x, int y, int z) {
  const double tree_noise = glm::perlin(
      glm::vec3((world_x * W + x) * 0.8, y * 0.02, (world_z * L + z) * 0.8));
  const double snow_noise = glm::perlin(
      glm::vec2((world_x * W + x) * 0.02, (world_z * L + z) * 0.02));
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
          if (y <= SNOW_LEVEL - 3 && tree_noise > 0.4 &&
              x >= TREE_BORDER_THRESHOLD &&
              x <= W - 1 - TREE_BORDER_THRESHOLD &&
              z >= TREE_BORDER_THRESHOLD &&
              z <= L - 1 - TREE_BORDER_THRESHOLD) {
            generateTrees(x, y, z);
          }
          return BlockType::GRASS;
        }
      }
    }
  }
  return BlockType::DIRT;
}

void Chunk::generateTerrain(int x, int z) {
  const int baseX = world_x * W;
  const int baseZ = world_z * L;

  // Sample noise at world position
  glm::vec3 pos((baseX + x + 0.1f) * FREQ, (baseZ + z + 0.1f) * FREQ, 0.0);

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

  // Default air
  BlockType type = BlockType::AIR;

  for (int y = 0; y < perlin_height; y++) {
    if (y == perlin_height - 1) {
      type = generateTopBlock(x, y, z);
    } else {
      type = generateInternalBlock(x, y, z);
    }

    // Set chunk block
    setBlock(x, y, z, type);
  }

  // Fill water pass
  for (int y = perlin_height; y < SEA_LEVEL; y++) {
    BlockType type = getBlock(x, y, z);
    if (type == BlockType::AIR) {
      setBlock(x, y, z, BlockType::WATER);
    }
  }

  // Generate clouds
  generateClouds(x, z);
}

void Chunk::generateClouds(int x, int z) {
  const double cloud_noise_big = glm::perlin(
      glm::vec2((world_x * W + x) * 0.02, (world_z * L + z) * 0.01));
  const double cloud_noise_mid = glm::perlin(
      glm::vec2((world_x * W + x) * 0.04, (world_z * L + z) * 0.02));
  const double cloud_noise_small = glm::perlin(
      glm::vec2((world_x * W + x) * 0.06, (world_z * L + z) * 0.04));
  if (cloud_noise_big > 0.4) {
      setBlock(x, CLOUD_LEVEL, z, BlockType::CLOUD);
  }
  if (cloud_noise_mid > 0.5) {
      setBlock(x, CLOUD_LEVEL-10, z, BlockType::CLOUD);
  }
  if (cloud_noise_small > 0.6) {
      setBlock(x, CLOUD_LEVEL-20, z, BlockType::CLOUD);
  }
}

void Chunk::generateTrees(int x, int y, int z) {
  // set trunk
  int h = (rand() % 6) + 10;
  for (int j = 1; j <= h; j++) {
    setBlock(x, y + j, z, BlockType::TREE);
  }

  // set leaves
  int radius = (rand() % 2) + 4;
  int top = y + h;
  int t_h = (rand() % 3) + h - radius; // leaf height
  int t_d = (rand() % 2) + 4; // leaf depth

  for (int dy = -t_d; dy <= t_h; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      for (int dz = -radius; dz <= radius; dz++) {
        // add some randomness
        if (rand() % 100 > 35)
          continue;
        // little sphere-ish shape
        if (dx * dx + dz * dz + dy * dy <= radius * radius + 1) {
          if (getBlock(x + dx, top + dy, z + dz) != BlockType::TREE)
            setBlock(x + dx, top + dy, z + dz, BlockType::TREE_LEAF);
        }
      }
    }
  }
}

void Chunk::generateChunk() {
  for (int x = 0; x < W; ++x) {
    for (int z = 0; z < L; ++z) {
      generateTerrain(x, z);
    }
  }
}
