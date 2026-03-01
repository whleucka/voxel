#include "biome/biome_manager.hpp"
#include "biome/biome.hpp"
#include "biome/desert_biome.hpp"
#include "biome/mountain_biome.hpp"
#include "biome/ocean_biome.hpp"
#include "biome/plains_biome.hpp"
#include "biome/tropical_biome.hpp"
#include "core/constants.hpp"
#include "core/settings.hpp"
#include <glm/gtc/noise.hpp>
#include <glm/vec3.hpp>
#include <memory>

std::unique_ptr<Biome> BiomeManager::createBiome(BiomeType type) {
  switch (type) {
  case BiomeType::MOUNTAIN:
    return std::make_unique<MountainBiome>();
  case BiomeType::DESERT:
    return std::make_unique<DesertBiome>();
  case BiomeType::OCEAN:
    return std::make_unique<OceanBiome>();
  case BiomeType::TROPICAL:
    return std::make_unique<TropicalBiome>();
  default:
    return std::make_unique<PlainsBiome>();
  };
}

BiomeType BiomeManager::getBiomeForChunk(int cx, int cz) {
  float chunk_center_x = (cx + 0.5f) * kChunkWidth;
  float chunk_center_z = (cz + 0.5f) * kChunkDepth;

  float biome_noise;
  int center_height =
      Biome::getHeight({chunk_center_x, chunk_center_z}, &biome_noise);

  // --- Ocean: anything below sea level ---
  if (center_height < kSeaLevel) {
    return BiomeType::OCEAN;
  }

  // --- Temperature & moisture noise for biome variety ---
  // These use different frequencies and offsets so they're independent of
  // the terrain height noise.  The +1000/+2000 offsets prevent correlation
  // with the continent/biome_noise used in getHeight().
  glm::vec2 spos(chunk_center_x + g_settings.noise_offset.x,
                 chunk_center_z + g_settings.noise_offset.y);

  float temperature =
      glm::clamp(glm::perlin(glm::vec2((spos.x + 1000.0f) * 0.003f,
                                        (spos.y + 1000.0f) * 0.003f)) *
                         0.5f +
                     0.5f,
                 0.0f, 1.0f);

  float moisture =
      glm::clamp(glm::perlin(glm::vec2((spos.x + 2000.0f) * 0.004f,
                                        (spos.y + 2000.0f) * 0.004f)) *
                         0.5f +
                     0.5f,
                 0.0f, 1.0f);

  // --- High altitude is always mountain ---
  if (center_height >= 150) {
    return BiomeType::MOUNTAIN;
  }

  // --- Mid-to-high terrain: mountain if biome noise says so ---
  if (center_height >= 130 && biome_noise > 0.45f) {
    return BiomeType::MOUNTAIN;
  }

  // --- Land biome selection based on temperature & moisture ---
  // Hot + dry  -> Desert
  // Hot + wet  -> Tropical
  // Otherwise  -> Plains, with mountain at high biome_noise

  if (temperature > 0.6f && moisture < 0.35f) {
    return BiomeType::DESERT;
  }

  if (temperature > 0.55f && moisture > 0.5f) {
    return BiomeType::TROPICAL;
  }

  // Elevated terrain with high biome_noise -> mountain
  if (center_height >= 115 && biome_noise > 0.55f) {
    return BiomeType::MOUNTAIN;
  }

  return BiomeType::PLAINS;
}
