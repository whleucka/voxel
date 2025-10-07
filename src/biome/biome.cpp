#include "biome/biome.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"
#include "util/noise.hpp"
#include <glm/fwd.hpp>

const float FREQ = 0.04f;
const float AMP = 35.0f;
const float OCTAVES = 4.0f;
const float LACUNARITY = 1.2f;
const float GAIN = 0.5f;

void Biome::generateTerrain(Chunk &chunk) {
  glm::vec2 pos = chunk.getPos();
  for (int x = 0; x < kChunkWidth; x++) {
    for (int z = 0; z < kChunkDepth; z++) {
      const int world_x = pos[0] * kChunkWidth + x;
      const int world_z = pos[1] * kChunkDepth + z;
      int h = getHeight({world_x, world_z});
      for (int y = 0; y < h; y++) {
        if (y == h - 1) {
          chunk.at(x, y, z) = generateTopBlock();
        } else {
          chunk.at(x, y, z) = generateInternalBlock(x, y, z);
        }
      }
    }
  }
}

void Biome::fillWater(Chunk &chunk) {
  glm::vec2 pos = chunk.getPos();
  for (int x = 0; x < kChunkWidth; x++) {
    for (int z = 0; z < kChunkDepth; z++) {
      const int world_x = pos[0] * kChunkWidth + x;
      const int world_z = pos[1] * kChunkDepth + z;
      int h = getHeight({world_x, world_z});
      for (int y = 0; y < h; y++) {
        const BlockType& type = chunk.at(x, y, z);
        if (y <= kSeaLevel && type == BlockType::AIR) {
          chunk.at(x, y, z) = BlockType::WATER;
        }
      }
    }
  }
}

int Biome::getHeight(glm::vec2 pos, float *biome_noise_out) {
  int x = pos[0];
  int z = pos[1];
  glm::vec3 position((x + 0.1f) * FREQ, (z + 0.1f) * FREQ, 0.0);
  double noise = fbm(position, OCTAVES, LACUNARITY, GAIN);

  // Continental mask (defines large-scale landmasses)
  // Lower frequency = bigger continents
  float continent = glm::perlin(pos * 0.0008f) * 0.5f + 0.5f;

  // Stage weights
  float ocean_factor = glm::smoothstep(0.0f, 0.35f, 1.0f - continent);
  float shelf_factor = glm::smoothstep(0.10f, 0.65f, continent) *
                       (1.0f - glm::smoothstep(0.45f, 0.8f, continent));
  float inland_factor = glm::smoothstep(0.45f, 0.90f, continent);

  // Ocean (two layers of noise for seabed)
  double ocean_base = kSeaLevel - 12.0;
  double ocean_large = glm::perlin(pos * 0.001f) * 6.0;
  double ocean_small = glm::perlin(pos * 0.01f) * 1.5;
  double ocean_height = ocean_base + ocean_large + ocean_small;

  // Clamp ocean depth
  if (ocean_height < kSeaLevel - 22.0)
    ocean_height = kSeaLevel - 22.0;
  if (ocean_height > kSeaLevel - 6.0)
    ocean_height = kSeaLevel - 6.0;

  // Shelf zone (coastal plains/beaches)
  double shelf_height = kSeaLevel - 2.0 + noise * 4.0;

  // Biome noise (used inside inland for plains vs mountains)
  glm::vec2 biome_pos(x * 0.0055f, z * 0.0055f);
  double biome_noise = glm::perlin(biome_pos) * 0.5 + 0.5;

  // Inland plains & mountains
  double plains_height = AMP + noise * 40.0;

  double mountain_factor = glm::smoothstep(0.45, 0.95, biome_noise);
  double plains_factor = 1.0 - mountain_factor;

  double mountain_shape = pow(noise, 1.2); // sharper peaks
  double mountain_height = AMP + 160.0 + mountain_shape * 200.0;

  double inland_height =
      plains_factor * plains_height + mountain_factor * mountain_height;

  // Final blend
  double terrain_height = ocean_factor * ocean_height +
                          shelf_factor * shelf_height +
                          inland_factor * inland_height;

  if (biome_noise_out) {
    *biome_noise_out = biome_noise;
  }

  return static_cast<int>(terrain_height);
}
