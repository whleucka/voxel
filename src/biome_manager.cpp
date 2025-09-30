#include "biome_manager.hpp"
#include "chunk.hpp"
#include "desert_biome.hpp"
#include "forest_biome.hpp"
#include "ocean_biome.hpp"
#include "plains_biome.hpp"
#include "tropical_biome.hpp"
#include "terrain_generator.hpp"
#include <glm/gtc/noise.hpp>

const int SEA_LEVEL = 42;

std::unique_ptr<Biome> BiomeManager::createBiome(BiomeType type) {
  switch (type) {
  case BiomeType::PLAINS:
    return std::make_unique<PlainsBiome>();
  case BiomeType::FOREST:
    return std::make_unique<ForestBiome>();
  case BiomeType::DESERT:
    return std::make_unique<DesertBiome>();
  case BiomeType::OCEAN:
    return std::make_unique<OceanBiome>();
  case BiomeType::TROPICAL:
    return std::make_unique<TropicalBiome>();
  }
  return std::make_unique<PlainsBiome>(); // Default to plains
}

BiomeType BiomeManager::getBiomeForChunk(int cx, int cz) {
  float chunk_center_x = (cx + 0.5f) * Chunk::W;
  float chunk_center_z = (cz + 0.5f) * Chunk::L;

  float biome_noise;
  int center_height =
      TerrainGenerator::getHeight(chunk_center_x, chunk_center_z, &biome_noise);

  // Normalize latitude factor (-1 at south pole, 0 at equator, +1 at north pole)
  float latitude = (chunk_center_z / 5000.0f); // adjust denominator to stretch zones
  latitude = glm::clamp(latitude, -1.0f, 1.0f);
  float lat_abs = fabs(latitude);

  // --- Stage detection from height ---
  if (center_height < SEA_LEVEL) {
    return BiomeType::OCEAN;
  }

  if (center_height >= SEA_LEVEL && center_height < SEA_LEVEL + 10) {
    // Shelf stage → deserts / tropics / plains
    if (lat_abs < 0.3f) { // near equator
      if (biome_noise < 0.5f) return BiomeType::TROPICAL;
      else return BiomeType::PLAINS;
    } else if (lat_abs < 0.7f) { // mid-latitudes
      if (biome_noise < 0.4f) return BiomeType::DESERT;
      else return BiomeType::PLAINS;
    } else {
      return BiomeType::PLAINS; // higher latitudes default to plains
    }
  }

  // Inland lowlands
  if (center_height < 160) {
    if (biome_noise < 0.5f) {
      return BiomeType::PLAINS;
    } else {
      return BiomeType::FOREST;
    }
  }

  // High mountains
  return BiomeType::FOREST;
}
