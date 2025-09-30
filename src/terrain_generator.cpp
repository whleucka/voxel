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

int TerrainGenerator::getHeight(float x, float z, float* biome_noise_out) {
    glm::vec3 pos((x + 0.1f) * FREQ, (z + 0.1f) * FREQ, 0.0);
    double hNoise = fbm(pos, OCTAVES, LACUNARITY, GAIN);

    glm::vec2 biome_pos(x * 0.001f, z * 0.001f);
    double biome_noise = glm::perlin(biome_pos) * 0.5 + 0.5;
    double mountain_factor = glm::smoothstep(0.42, 0.65, biome_noise);

    int perlin_height =
        static_cast<int>(
            hNoise * 30
            + mountain_factor * 170
            ) +
        AMP;
    
    if (biome_noise_out) {
        *biome_noise_out = biome_noise;
    }

    return perlin_height;
}
