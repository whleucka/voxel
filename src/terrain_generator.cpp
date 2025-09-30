#include "terrain_generator.hpp"
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

  // Biome-scale noise
  glm::vec2 biome_pos(x * 0.0055f, z * 0.0055f);
  double biome_noise = glm::perlin(biome_pos) * 0.5 + 0.5;

  // ----- Ocean -----
  double ocean_factor = 1.0 - biome_noise;
  ocean_factor = glm::clamp(ocean_factor, 0.0, 1.0);
  double ocean_depth =
      ocean_factor * ocean_factor * 6.0;

  // ----- Lowlands -----
  double land_factor = glm::smoothstep(0.15, 0.65, biome_noise);
  double land_variation =
      hNoise * 50.0 * land_factor;

  // ----- Mountains -----
  double mountain_factor = glm::smoothstep(0.45, 0.85, biome_noise);
  double mountain_height =
      mountain_factor * 150.0;

  // ----- Final terrain height -----
  int terrain_height =
      static_cast<int>(land_variation - ocean_depth + mountain_height) + AMP;

  if (biome_noise_out) {
    *biome_noise_out = biome_noise;
  }

  return terrain_height;
}
