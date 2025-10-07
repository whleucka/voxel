#include "biome/biome_manager.hpp"
#include "biome/biome.hpp"
#include "biome/desert_biome.hpp"
#include "biome/mountain_biome.hpp"
#include "biome/ocean_biome.hpp"
#include "biome/plains_biome.hpp"
#include "biome/tropical_biome.hpp"
#include "core/constants.hpp"
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

  // Normalize latitude factor (-1 at south pole, 0 at equator,
  // +1 at north pole)
  float latitude = (chunk_center_z / 5000.0f);
  latitude = glm::clamp(latitude, -1.0f, 1.0f);
  float lat_abs = fabs(latitude);

  // --- Stage detection from height ---
  if (center_height < kSeaLevel) {
    return BiomeType::OCEAN;
  }

  if (center_height >= kSeaLevel && center_height < kSeaLevel + 10) {
    // Shelf stage → deserts / tropics / plains
    if (lat_abs < 0.3f) { // near equator
      if (biome_noise < 0.5f)
        return BiomeType::TROPICAL;
      else
        return BiomeType::PLAINS;
    } else if (lat_abs < 0.7f) { // mid-latitudes
      if (biome_noise < 0.4f)
        return BiomeType::DESERT;
      else
        return BiomeType::PLAINS;
    } else {
      return BiomeType::PLAINS; // higher latitudes default to plains
    }
  }

  // Inland lowlands
  if (center_height < 160) {
    if (biome_noise < 0.5f) {
      return BiomeType::PLAINS;
    } else {
      return BiomeType::MOUNTAIN;
    }
  }

  // High mountains
  return BiomeType::MOUNTAIN;
}
