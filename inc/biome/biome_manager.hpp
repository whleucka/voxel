#pragma once

#include "biome/biome.hpp"
#include "biome/biome_type.hpp"
#include <glm/glm.hpp>
#include <memory>

class BiomeManager {
public:
  static std::unique_ptr<Biome> createBiome(BiomeType type);
  static BiomeType getBiomeForChunk(int cx, int cz);
};
