#include "terrain_generator.hpp"
#include "world_constants.hpp"
#include <glm/gtc/noise.hpp>

const float FREQ = 0.04f;
const float AMP = 35.0f;
const float OCTAVES = 4.0f;
const float LACUNARITY = 1.2f;
const float GAIN = 0.5f;

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

int TerrainGenerator::getHeight(float x, float z, float *biome_noise_out) {
  glm::vec3 pos((x + 0.1f) * FREQ, (z + 0.1f) * FREQ, 0.0);
  double hNoise = fbm(pos, OCTAVES, LACUNARITY, GAIN);

  // --- Continental mask (defines large-scale landmasses) ---
  // Lower frequency = bigger continents
  float continent = glm::perlin(glm::vec2(x, z) * 0.0008f) * 0.5f + 0.5f;

  // Stage weights
  float ocean_factor  = glm::smoothstep(0.0f, 0.35f, 1.0f - continent);
  float shelf_factor  = glm::smoothstep(0.10f, 0.65f, continent) *
                        (1.0f - glm::smoothstep(0.45f, 0.8f, continent));
  float inland_factor = glm::smoothstep(0.45f, 0.90f, continent);

  // --- Ocean (two layers of noise for seabed) ---
  double ocean_base = SEA_LEVEL - 12.0;
  double ocean_large = glm::perlin(glm::vec2(x, z) * 0.001f) * 6.0; 
  double ocean_small = glm::perlin(glm::vec2(x, z) * 0.01f) * 1.5; 
  double ocean_height = ocean_base + ocean_large + ocean_small;

  // Clamp ocean depth
  if (ocean_height < SEA_LEVEL - 22.0) ocean_height = SEA_LEVEL - 22.0;
  if (ocean_height > SEA_LEVEL - 6.0)  ocean_height = SEA_LEVEL - 6.0;

  // --- Shelf zone (coastal plains/beaches) ---
  double shelf_height = SEA_LEVEL - 2.0 + hNoise * 4.0;

  // --- Biome noise (used inside inland for plains vs mountains) ---
  glm::vec2 biome_pos(x * 0.0055f, z * 0.0055f);
  double biome_noise = glm::perlin(biome_pos) * 0.5 + 0.5;

  // --- Inland plains & mountains ---
  double plains_height = AMP + hNoise * 40.0;

  double mountain_factor = glm::smoothstep(0.45, 0.95, biome_noise);
  double plains_factor   = 1.0 - mountain_factor;

  double mountain_shape = pow(hNoise, 1.2); // sharper peaks
  double mountain_height = AMP + 160.0 + mountain_shape * 200.0;

  double inland_height =
      plains_factor   * plains_height +
      mountain_factor * mountain_height;

  // --- Final blend across stages ---
  double terrain_height =
      ocean_factor  * ocean_height +
      shelf_factor  * shelf_height +
      inland_factor * inland_height;

  if (biome_noise_out) {
    *biome_noise_out = biome_noise;
  }

  return static_cast<int>(terrain_height);
}
